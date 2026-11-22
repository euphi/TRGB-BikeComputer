/*
 * WebInstrument.cpp
 *
 * See WebInstrument.h for why this exists and for the "nothing blocking on async_tcp" rule.
 */

#include "WebInstrument.h"

#include "Singletons.h"
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <lvgl.h>

static const BCLogger::LogTag TAG = BCLogger::TAG_WEB;

namespace WebInstr {

namespace {

// ---------------------------------------------------------------------------
// Finished-request ring
// ---------------------------------------------------------------------------
// Single producer (async_tcp, in onEnd()), single consumer (FlusherTask, in drain()),
// so plain acquire/release on the two indices is enough -- no mutex, which matters
// because the producer side runs in a watchdog-guarded task.
//
// On overflow we drop the NEWEST record and count it, rather than evicting the oldest:
// evicting would mean the producer moves the consumer's index and this stops being a
// clean SPSC ring. For diagnostics either is fine; this one can't race.
// Drained every 5s by FlusherTask. One page load alone is ~9 requests (HTML, CSS, five
// fonts, favicon, JSON), so 16 overflowed on two quick navigations. 32 * 56 byte.
constexpr uint8_t RING_SIZE = 32;		// power of two, see the & (RING_SIZE-1) below
constexpr size_t  URL_LEN   = 40;

struct Entry {
	uint32_t startMs;
	uint16_t durMs;
	int32_t  heapDelta;		// internal free after - before, in byte (negative = consumed)
	uint32_t peakDrop;		// burst start free - lowest free while requests were open (true low, see onStart)
	uint32_t bytes;			// response body size, 0 until the handler calls noteBytes()
	char     url[URL_LEN];
};

// Internal RAM on purpose, despite internal RAM being the scarce pool here: at 16 * 56
// byte this is under 1KB, and in PSRAM it would be a small write on EVERY finished
// request -- an event-driven PSRAM access on a board whose panel DMAs its framebuffer
// from PSRAM without a bounce buffer. That is exactly the trigger pattern that causes
// the "frame shifted right with wraparound" artifact (see commit 7051e56). Cheap data
// belongs in internal RAM; only large AND genuinely cold buffers go to PSRAM here.
Entry ring[RING_SIZE] = {};
std::atomic<uint8_t>  ringHead{0};		// producer writes
std::atomic<uint8_t>  ringTail{0};		// consumer writes
std::atomic<uint32_t> ringDropped{0};

// drain(): thresholds above which a finished request is logged at Info
constexpr uint16_t SLOW_REQ_MS    = 1000;
constexpr int32_t  HEAVY_REQ_HEAP = 8192;		// internal heap still held at the end of the request

// ---------------------------------------------------------------------------
// In-flight bookkeeping
// ---------------------------------------------------------------------------
// Only needed to carry the body size from the handler (noteBytes) to onEnd. All three
// entry points run on async_tcp, which is a single task, so this needs no locking.
// CONFIG_LWIP_MAX_ACTIVE_TCP is 16; 8 slots is plenty for "how big was the body" and
// a miss just reports 0 bytes.
constexpr uint8_t PENDING_SLOTS = 8;
struct Pending {
	AsyncWebServerRequest* req;
	uint32_t bytes;
};
Pending pending[PENDING_SLOTS] = {};

std::atomic<uint16_t> openCount{0};
std::atomic<uint32_t> burstStartFree{0};	// internal free when the first request of a burst began
std::atomic<uint16_t> openPeakCount{0};

Pending* findPending(AsyncWebServerRequest* req) {
	for (auto& p : pending) {
		if (p.req == req) return &p;
	}
	return nullptr;
}

inline size_t internalFree() {
	return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

}	// anonymous namespace

void setup() {
	// Nothing to allocate any more -- the ring is a plain static array, see its comment.
	// Kept as an explicit entry point so the middleware registration in WifiWebserver.cpp
	// stays readable and there is somewhere to put future one-time setup.
}

void onStart(AsyncWebServerRequest* request) {
	const uint16_t now = openCount.fetch_add(1, std::memory_order_relaxed) + 1;
	if (now == 1) {
		// Burst start (nothing else open): restart the IDF's low-watermark tracking so that
		// heap_caps_get_minimum_free_size() below is the true low since NOW, including spikes
		// a sampler would miss. Global, so it also sees other tasks' dips (BLE connects).
		heap_caps_monitor_local_minimum_free_size_stop();
		heap_caps_monitor_local_minimum_free_size_start();
		burstStartFree.store(internalFree(), std::memory_order_relaxed);
	}
	uint16_t peak = openPeakCount.load(std::memory_order_relaxed);
	while (now > peak && !openPeakCount.compare_exchange_weak(peak, now, std::memory_order_relaxed)) {
		// retry: peak was refreshed by compare_exchange_weak
	}
	if (Pending* slot = findPending(nullptr)) {
		slot->req = request;
		slot->bytes = 0;
	}
}

void noteBytes(AsyncWebServerRequest* request, size_t bytes) {
	if (Pending* slot = findPending(request)) slot->bytes = bytes;
}

void onEnd(AsyncWebServerRequest* request, uint32_t startMs, size_t internalFreeAtStart) {
	uint32_t bytes = 0;
	if (Pending* slot = findPending(request)) {
		bytes = slot->bytes;
		slot->req = nullptr;
	}

	const size_t localMin = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	const uint32_t burstStart = burstStartFree.load(std::memory_order_relaxed);
	const uint32_t peakDrop = burstStart > localMin ? burstStart - static_cast<uint32_t>(localMin) : 0;
	uint16_t cur = openCount.load(std::memory_order_relaxed);
	while (cur > 0 && !openCount.compare_exchange_weak(cur, cur - 1, std::memory_order_relaxed)) {
		// retry
	}

	if (openCount.load(std::memory_order_relaxed) == 0) heap_caps_monitor_local_minimum_free_size_stop();
	const uint8_t head = ringHead.load(std::memory_order_relaxed);
	const uint8_t tail = ringTail.load(std::memory_order_acquire);
	if (static_cast<uint8_t>(head - tail) >= RING_SIZE) {
		ringDropped.fetch_add(1, std::memory_order_relaxed);
		return;
	}

	Entry& e = ring[head & (RING_SIZE - 1)];
	e.startMs   = startMs;
	const uint32_t dur = millis() - startMs;
	e.durMs     = (dur > UINT16_MAX) ? UINT16_MAX : static_cast<uint16_t>(dur);
	e.heapDelta = static_cast<int32_t>(internalFree()) - static_cast<int32_t>(internalFreeAtStart);
	e.bytes     = bytes;
	e.peakDrop  = peakDrop;
	// request->url() is still valid here: _onDisconnect() invokes the callback before
	// AsyncWebServer::_handleDisconnect() deletes the request (WebRequest.cpp).
	strlcpy(e.url, request->url().c_str(), URL_LEN);

	ringHead.store(head + 1, std::memory_order_release);
}

uint16_t open()     { return openCount.load(std::memory_order_relaxed); }
uint16_t openPeak() { return openPeakCount.load(std::memory_order_relaxed); }
void     resetPeak() { openPeakCount.store(openCount.load(std::memory_order_relaxed), std::memory_order_relaxed); }

void report(bool full, bool walkPsramHeap) {
	// Which of these are cheap matters a lot here, because this runs every 5 seconds:
	//   heap_caps_get_free_size()         -> sums a stored per-heap counter, O(1)
	//   heap_caps_get_minimum_free_size() -> stored counter, O(1)
	//   heap_caps_get_largest_free_block()-> heap_caps_get_info() -> walks EVERY block
	//   heap_caps_get_info()              -> walks EVERY block
	// Walking the INTERNAL heaps only touches internal RAM, which the display does not
	// care about. Walking the PSRAM heap reads block headers scattered across PSRAM while
	// the panel is DMAing its framebuffer from the same bus with no bounce buffer -- that
	// tears a frame ("shifted right with wraparound"). So PSRAM is reported with the O(1)
	// calls only, and the block walk is opt-in for the on-demand "mem" command.
	// The full report (every 60 s, and "mem") at Info; the 5 s cycles in between at Debug. A MEM
	// line every 5 s at Info was 720 lines an hour into the SD debug log, standing still included.
	// If nobody wants the Debug line, skip the internal heap walk as well.
	const BCLogger::LogType memLevel = full ? BCLogger::Log_Info : BCLogger::Log_Debug;
	if (bclog.checkLogLevel(memLevel, TAG)) {
		multi_heap_info_t internalInfo;
		heap_caps_get_info(&internalInfo, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

		// int=free/largest is the fragmentation view: a large total with a small largest block
		// is what makes a 1.5KB per-ACK malloc fail while "free heap" still looks healthy.
		// min= is the low-water mark since boot, i.e. what a load spike actually cost us.
		bclog.logf(memLevel, TAG,
		           "MEM int=%u/%u min=%u | dma=%u/%u | psram=%u min=%u | req=%u peak=%u",
		           internalInfo.total_free_bytes, internalInfo.largest_free_block, internalInfo.minimum_free_bytes,
		           heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
		           heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
		           open(), openPeak());
	}

	if (walkPsramHeap) {
		multi_heap_info_t psramInfo;
		heap_caps_get_info(&psramInfo, MALLOC_CAP_SPIRAM);
		bclog.logf(BCLogger::Log_Info, TAG, "PSRAM largest free block=%u byte (heap walk - expect one torn frame)",
		           psramInfo.largest_free_block);
	}

	// exchange(0), not load(): this runs every 5s, and the counter used to be cumulative,
	// so a single burst after boot was re-reported as the same "N dropped" every cycle
	// forever. Report only what was lost since the last report.
	const uint32_t dropped = ringDropped.exchange(0, std::memory_order_relaxed);
	if (dropped) {
		bclog.logf(BCLogger::Log_Warn, TAG, "Request ring overflowed - %u record(s) dropped since last report", dropped);
	}

	if (!full) return;
	// xTaskGetHandle() walks the task lists with the scheduler suspended, so keep this rare.
	// uxTaskGetStackHighWaterMark() returns byte on ESP-IDF FreeRTOS (not words).
	// The three tasks this firmware creates itself run on 3-4KB stacks and were never observed.
	//
	// Every name here must be shorter than configMAX_TASK_NAME_LEN (16). xTaskGetHandle()
	// does configASSERT(strlen(pcNameToQuery) < configMAX_TASK_NAME_LEN) and ABORTS on a
	// longer string -- it does not just fail to find it. That is what crashed this report
	// on the 21-char "BLEScanUndConnectTask" (since renamed to "BLEScanConnect" in
	// BLEDevices.cpp). xTaskCreate() truncates silently, so an over-long name is invisible
	// everywhere else and only blows up here; hence the guard rather than just the rename.
	static const char* const kTasks[] = {"async_tcp", "tiT", "esp_timer", "UI Task", "loopTask", "FlusherTask", "BLEScanConnect", "ImuTask"};
	for (const char* name : kTasks) {
		if (strlen(name) >= configMAX_TASK_NAME_LEN) {
			bclog.logf(BCLogger::Log_Warn, TAG, "Task name '%s' is >= configMAX_TASK_NAME_LEN (%d) - skipped, xTaskGetHandle() would abort",
			           name, configMAX_TASK_NAME_LEN);
			continue;
		}
		TaskHandle_t handle = xTaskGetHandle(name);
		if (handle) {
			bclog.logf(BCLogger::Log_Info, TAG, "STACK %-15s free=%u byte", name, uxTaskGetStackHighWaterMark(handle));
		}
	}
}

void reportDisplayBuffers() {
	lv_disp_t* disp = lv_disp_get_default();
	if (!disp || !disp->driver || !disp->driver->draw_buf) {
		bclog.log(BCLogger::Log_Warn, TAG, "No LVGL display registered - cannot report draw buffers");
		return;
	}
	const lv_disp_draw_buf_t* db = disp->driver->draw_buf;
	auto align = [](const void* p) -> unsigned {
		const uintptr_t a = reinterpret_cast<uintptr_t>(p);
		if (!a) return 0;
		unsigned n = 1;
		while (n < 64 && (a & n) == 0) n <<= 1;
		return (a & (n - 1)) ? n >> 1 : n;		// largest power of two dividing the address, capped at 64
	};
	bclog.logf(BCLogger::Log_Info, TAG, "LVGL draw_buf1=%p (align %u) draw_buf2=%p (align %u) size=%u px",
	           db->buf1, align(db->buf1), db->buf2, align(db->buf2), (unsigned)db->size);
}

void drain(bool all) {
	const uint8_t head = ringHead.load(std::memory_order_acquire);
	uint8_t tail = ringTail.load(std::memory_order_relaxed);
	while (tail != head) {
		const Entry& e = ring[tail & (RING_SIZE - 1)];
		// One page load is ~9 requests. Only the ones worth a look go out at Info: slow, or
		// holding on to a lot of internal heap. The rest at Debug; "mem" shows all of them.
		const bool notable = e.durMs > SLOW_REQ_MS || e.heapDelta < -HEAVY_REQ_HEAP;
		bclog.logf((all || notable) ? BCLogger::Log_Info : BCLogger::Log_Debug, TAG, "REQ %-32s %ums %d byte heap, peak -%u, %u byte body",
		           e.url, e.durMs, e.heapDelta, (unsigned)e.peakDrop, e.bytes);
		tail++;
		ringTail.store(tail, std::memory_order_release);
	}
}

}	// namespace WebInstr
