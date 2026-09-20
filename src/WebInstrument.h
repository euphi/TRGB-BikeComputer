/*
 * WebInstrument.h
 *
 * Memory and request diagnostics for the async web server.
 *
 * Why this exists: the server becomes unreliable (responses that never finish, sockets
 * that never close, mdns "Cannot allocate memory" while 6.8MB of PSRAM is free) and we
 * had no way to see it. The only memory line in the firmware logged MALLOC_CAP_DMA at
 * Log_Debug under TAG_SD -- a subset of the wrong pool, at a level that is off by
 * default. What actually runs out is internal DRAM (MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT):
 * lwIP pbufs come from there (CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP is not set) and
 * CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=0 keeps nothing back for them.
 *
 * Everything here is recorded on the async_tcp task and logged from FlusherTask. Nothing
 * in onStart()/noteBytes()/onEnd() may block, touch the SD card or call bclog -- handlers
 * run on async_tcp, which is watchdog-guarded with a 5s panic timeout.
 */

#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

namespace WebInstr {

// Allocates the PSRAM record ring. Call once, before the middleware is installed.
// Without it everything below still works, minus the per-request records.
void setup();

// --- called from the server middleware, i.e. on the async_tcp task ---
void onStart(AsyncWebServerRequest* request);
void noteBytes(AsyncWebServerRequest* request, size_t bytes);	// body size, once known
void onEnd(AsyncWebServerRequest* request, uint32_t startMs, size_t internalFreeAtStart);

// --- counters, readable from anywhere ---
uint16_t open();		// requests currently in flight
uint16_t openPeak();	// high-water since boot; never reset automatically, see resetPeak()
void     resetPeak();

// --- called from FlusherTask (and the "mem" CLI command) ---
// report() writes the MEM summary line; drain() writes one line per finished request.
//
// walkPsramHeap: include the largest free PSRAM block. This costs a full block walk of
// the PSRAM heap (heap_caps_get_info -> multi_heap_info -> iterates every block), i.e. a
// scattered read burst across PSRAM -- and this board's RGB panel DMAs its framebuffer
// straight out of PSRAM with no bounce buffer, so it WILL tear a frame. Never pass true
// from anything periodic; it exists for the on-demand "mem" CLI command only.
// The other figures are all O(1) counter reads and safe to poll.
void report(bool withStackWatermarks, bool walkPsramHeap = false);
void drain();

// One-shot diagnostic: addresses and alignment of LVGL's two draw buffers and of the
// panel framebuffer. TRGBSuppport.cpp allocates the draw buffers with a plain
// heap_caps_malloc(MALLOC_CAP_SPIRAM) -- no alignment guarantee -- and LVGL memcpys a
// full 460KB buffer into the framebuffer on every flush, on the same PSRAM bus the panel
// DMAs from without a bounce buffer. If those addresses stop being 64-byte aligned,
// tearing follows. Anything that allocates PSRAM before trgb.init() (a statically
// constructed object, say) can move them, so print them when chasing display artifacts.
void reportDisplayBuffers();

}	// namespace WebInstr
