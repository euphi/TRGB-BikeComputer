/*
 * LogPush.cpp -- see LogPush.h.
 *
 * Web interface (the page is linked from /wifi):
 *
 *   GET  /push           the page: target, credentials, state, buttons
 *   GET  /push/state     LogPushStatus as JSON (never the password)
 *   POST /push/config    enabled=0|1, url, user, secret (empty = keep), clear=1 (remove secret)
 *   POST /push/now       upload now
 *   POST /push/test      create/check the target folder (MKCOL), report the HTTP answer
 *   POST /push/mark      record every file on the card as uploaded, without uploading
 */

#include "LogPush.h"

#include <Singletons.h>
#include <SD_MMC.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>
#include "LogSessions.h"
#include "NvsUtil.h"
#include "ClockSync.h"
#include "WebPage.h"

using namespace PushCore;

static const BCLogger::LogTag TAG = BCLogger::TAG_WIFI;
static const char* const NVS_NAMESPACE = "LogPush";
static const char* const NVS_KEY_CFG = "cfg";
static const char* const MANIFEST_TMP = "/bcpush.tmp";
static constexpr uint32_t TASK_STACK = 9216;
static constexpr int HTTP_TIMEOUT_MS = 20000;
static constexpr size_t MAX_DAYS = 512;
// Below this much free internal heap a run stops between two files (the web server sheds load
// at 20 KB, WifiWebserver.cpp).
static constexpr size_t ABORT_FREE_INTERNAL = 24 * 1024;

// certs/x509_crt_bundle.bin (Tools/make_cert_bundle.sh), embedded by board_build.embed_files.
extern const uint8_t caBundleStart[] asm("_binary_certs_x509_crt_bundle_bin_start");
extern const uint8_t caBundleEnd[] asm("_binary_certs_x509_crt_bundle_bin_end");

namespace {

struct Lock {
	SemaphoreHandle_t m;
	explicit Lock(SemaphoreHandle_t mutex): m(mutex) {if (m) xSemaphoreTake(m, portMAX_DELAY);}
	~Lock() {if (m) xSemaphoreGive(m);}
};

// mbedTLS allocates its record buffers (2 x 16 KB) and the handshake state with calloc. The
// default for this build is internal RAM only (CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC), which a TLS
// connection would nearly use up. Switched at runtime instead of in custom_sdkconfig: no IDF
// rebuild, and only for the time of a run. heap_caps_free() takes memory from either.
void* tlsCallocPsram(size_t n, size_t size) {
	void* p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	return p ? p : heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
void* tlsCallocInternal(size_t n, size_t size) {
	return heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
void setTlsPsram(bool on) {
	mbedtls_platform_set_calloc_free(on ? tlsCallocPsram : tlsCallocInternal, heap_caps_free);
}

size_t freeInternal() {
	return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

bool ok2xx(int code) {return code >= 200 && code < 300;}

// WebDAV over one kept-alive connection (HTTPClient reuses it while the host stays the same).
class Dav {
public:
	// user empty + secret = bearer token (the log service). nickname: X-NC-Nickname for a files
	// drop that is a "file request" (Nextcloud then files the uploads in a folder of that name).
	Dav(const char* url, const char* user, const char* secret, const char* nickname)
		: user(user), secret(secret), nickname(nickname), https(isHttps(url)) {
		if (https) tls.setCACertBundle(caBundleStart, caBundleEnd - caBundleStart);
		http.setReuse(true);
		http.setTimeout(HTTP_TIMEOUT_MS);
		http.setConnectTimeout(10000);
	}
	~Dav() {http.end();}

	int put(const char* url, File& f) {
		if (!begin(url)) return HTTPC_ERROR_CONNECTION_REFUSED;
		http.addHeader("Content-Type", "application/octet-stream");
		const int code = http.sendRequest("PUT", &f, f.size());
		finish(code);
		return code;
	}

	int put(const char* url, const char* text) {
		if (!begin(url)) return HTTPC_ERROR_CONNECTION_REFUSED;
		http.addHeader("Content-Type", "text/plain");
		const int code = http.sendRequest("PUT", reinterpret_cast<uint8_t*>(const_cast<char*>(text)), strlen(text));
		finish(code);
		return code;
	}

	int mkcol(const char* url) {
		if (!begin(url)) return HTTPC_ERROR_CONNECTION_REFUSED;
		const int code = http.sendRequest("MKCOL");
		finish(code);
		return code;
	}

	// Creates the folder and, on 409 (parent missing), the ones above it. 201 = it exists now.
	// 405 counts as existing: Nextcloud's answer for an existing folder, and the log service's
	// for a method it does not have (it needs no folders).
	int ensureDir(const char* url, int depth = 0) {
		int code = mkcol(url);
		if (code == 409 && depth < 6) {
			char parent[URL_MAX + 1];
			if (parentUrl(url, parent, sizeof(parent)) && ensureDir(parent, depth + 1) == 201) code = mkcol(url);
		}
		return (code == 201 || code == 405) ? 201 : code;
	}

private:
	const char* user;
	const char* secret;
	const char* nickname;
	const bool https;
	NetworkClient plain;
	NetworkClientSecure tls;
	HTTPClient http;

	bool begin(const char* url) {
		if (!http.begin(https ? static_cast<NetworkClient&>(tls) : plain, url)) return false;
		if (user[0]) {
			http.setAuthorizationType("Basic");
			http.setAuthorization(user, secret);
		} else if (secret[0]) {					// the log service with BIKELOG_REQUIRE_AUTH
			http.setAuthorizationType("Bearer");
			http.setAuthorization(secret);
		}
		if (nickname) http.addHeader("X-NC-Nickname", nickname);
		// The log service treats such an upload like a pulled file: a session deleted there
		// stays deleted (a plain PUT would bring it back). Nextcloud ignores the header.
		http.addHeader("X-BikeLog-Auto", "1");
		return true;
	}

	void finish(int code) {
		// A failed request may have left half a body on the wire -- start over next time.
		if (code < 0 || !ok2xx(code)) http.setReuse(false);
		http.end();
		http.setReuse(true);
	}
};

const char* httpText(int code) {
	switch (code) {
	case 401: return "access denied (user / password)";
	case 403: return "forbidden";
	case 404: return "not found";
	case 405: return "method not allowed";
	case 409: return "folder missing";
	case 413: return "file too large for the server";
	case 507: return "server storage full";
	default: return "";
	}
}

void describeHttp(int code, char* out, size_t len) {
	if (code < 0) {
		snprintf(out, len, "%s", HTTPClient::errorToString(code).c_str());
	} else {
		snprintf(out, len, "HTTP %d %s", code, httpText(code));
	}
}

struct Todo {
	char rel[REL_MAX + 1];
	uint32_t size;
};

}	// namespace

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void LogPush::setup() {
	mtx = xSemaphoreCreateMutex();
	load();
	registerCli();
	registerRoutes();
}

void LogPush::load() {
	Lock lock(mtx);
	memset(&cfg, 0, sizeof(cfg));
	cfg.version = BLOB_VERSION;
	if (NvsUtil::namespaceExists(NVS_NAMESPACE)) {
		Preferences p;
		if (p.begin(NVS_NAMESPACE, true)) {
			Blob b;
			if (NvsUtil::loadBlob(p, NVS_KEY_CFG, b) && b.version == BLOB_VERSION) {
				b.url[URL_MAX] = b.user[USER_MAX] = b.secret[SECRET_MAX] = 0;
				cfg = b;
			}
			p.end();
		}
	}
	bclog.logf(BCLogger::Log_Info, TAG, "Log push %s%s", cfg.enabled ? "on" : "off",
	           cfg.url[0] ? "" : " (no target)");
}

void LogPush::save() {
	Preferences p;
	if (!p.begin(NVS_NAMESPACE, false) || !NvsUtil::saveBlob(p, NVS_KEY_CFG, cfg)) {
		bclog.log(BCLogger::Log_Error, TAG, "Saving the log push settings to NVS failed");
	}
	p.end();
}

LogPush::SaveResult LogPush::configure(bool enabled, const char* url, const char* user, const char* secret, bool clearSecret) {
	if (url[0] && !validUrl(url)) return SaveResult::BAD_URL;
	if (strlen(user) > USER_MAX || strlen(secret) > SECRET_MAX) return SaveResult::TOO_LONG;
	{
		Lock lock(mtx);
		cfg.version = BLOB_VERSION;
		cfg.enabled = enabled && url[0];
		strlcpy(cfg.url, url, sizeof(cfg.url));
		strlcpy(cfg.user, user, sizeof(cfg.user));
		if (clearSecret) cfg.secret[0] = 0;
		else if (secret[0]) strlcpy(cfg.secret, secret, sizeof(cfg.secret));
		save();
	}
	nextDue = 0;		// try the new target right away (after SETTLE_MS if just connected)
	lastRunMs = 0;
	bclog.logf(BCLogger::Log_Info, TAG, "Log push settings saved: %s", enabled && url[0] ? "on" : "off");
	return SaveResult::OK;
}

void LogPush::setEnabled(bool on) {
	Lock lock(mtx);
	const bool v = on && cfg.url[0];
	if (cfg.enabled == v) return;
	cfg.enabled = v;
	save();
}

void LogPush::getStatus(LogPushStatus& out) {
	Lock lock(mtx);
	out = st;
	out.enabled = cfg.enabled;
	out.configured = cfg.url[0] != 0;
	out.hasSecret = cfg.secret[0] != 0;
	out.running = running;
	strlcpy(out.url, cfg.url, sizeof(out.url));
	strlcpy(out.user, cfg.user, sizeof(out.user));
	out.lastRunAgoS = lastRunMs ? (millis() - lastRunMs) / 1000 : 0;
}

bool LogPush::isBusyWith(const String& sdPath) {
	Lock lock(mtx);
	return busyPath[0] && sdPath == busyPath;
}

void LogPush::setState(const char* state, const char* error) {
	Lock lock(mtx);
	strlcpy(st.state, state, sizeof(st.state));
	if (error) strlcpy(st.lastError, error, sizeof(st.lastError));
}

// ---------------------------------------------------------------------------
// When
// ---------------------------------------------------------------------------

void LogPush::poll(bool online) {
	const uint32_t now = millis();
	if (!online) {
		onlineSince = 0;
		return;
	}
	if (!onlineSince) {
		onlineSince = now ? now : 1;
		nextDue = 0;							// every (re)connect: once after SETTLE_MS
	}
	if (running) return;

	const uint8_t req = pending.exchange(0);
	if (req) {
		start(static_cast<Mode>(req), true);
		return;
	}
	bool enabled;
	{
		Lock lock(mtx);
		enabled = cfg.enabled && cfg.url[0];
	}
	if (!enabled || now - onlineSince < SETTLE_MS) return;
	if (nextDue && static_cast<int32_t>(now - nextDue) < 0) return;
	if (LogSessions::busy()) return;			// the last session is still moving out of CUR/
	start(Mode::PUSH, false);
}

bool LogPush::request(Mode m) {
	if (running) return false;
	{
		Lock lock(mtx);
		if (!cfg.url[0] && m != Mode::MARK) return false;
	}
	pending = static_cast<uint8_t>(m);			// poll() starts it while online
	if (m == Mode::MARK) start(m, true);		// needs no network
	return true;
}

bool LogPush::start(Mode m, bool manual) {
	if (running.exchange(true)) return false;
	pending = 0;
	if (freeInternal() < MIN_FREE_INTERNAL && m != Mode::MARK) {
		running = false;
		if (manual) pending = static_cast<uint8_t>(m);		// poll() tries again
		char msg[64];
		snprintf(msg, sizeof(msg), "waiting for memory (%u KB free)", (unsigned) (freeInternal() / 1024));
		setState("waiting", msg);
		nextDue = millis() + MEMORY_RETRY_MS;
		return false;
	}
	TaskArg* arg = new (std::nothrow) TaskArg{this, m, manual};
	if (!arg || xTaskCreate(taskMain, "LogPush", TASK_STACK, arg, 1, nullptr) != pdPASS) {
		delete arg;
		running = false;
		nextDue = millis() + MEMORY_RETRY_MS;
		setState("failed", "could not start the task");
		return false;
	}
	return true;
}

void LogPush::taskMain(void* p) {
	const TaskArg a = *static_cast<TaskArg*>(p);
	delete static_cast<TaskArg*>(p);
	a.self->run(a.mode, a.manual);
	bclog.logf(BCLogger::Log_Debug, TAG, "Log push task: %u byte stack left", (unsigned) uxTaskGetStackHighWaterMark(nullptr));
	a.self->running = false;
	vTaskDelete(nullptr);
}

// ---------------------------------------------------------------------------
// One run
// ---------------------------------------------------------------------------

namespace {

// Reads the manifest; returns the number of lines (for the compaction).
size_t loadManifest(Manifest& man) {
	size_t lines = 0;
	File f = SD_MMC.open(LogPush::MANIFEST_PATH, FILE_READ);
	if (!f) return 0;
	char line[REL_MAX + 16];
	while (f.available()) {
		const size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
		line[n] = 0;
		lines++;
		man.parseLine(line);
	}
	f.close();
	return lines;
}

bool appendManifest(const char* rel, uint32_t size) {
	char line[REL_MAX + 16];
	const size_t n = Manifest::formatLine(rel, size, line, sizeof(line));
	File f = SD_MMC.open(LogPush::MANIFEST_PATH, FILE_APPEND);
	if (!f) return false;
	const bool ok = n && f.write(reinterpret_cast<const uint8_t*>(line), n) == n;
	f.close();
	return ok;
}

// Writes the entries of files still on the card; replaces the manifest.
bool rewriteManifest(const Manifest& man) {
	File f = SD_MMC.open(MANIFEST_TMP, FILE_WRITE);
	if (!f) return false;
	bool ok = true;
	char line[REL_MAX + 16];
	for (size_t i = 0; ok && i < man.count(); i++) {
		const Entry& e = man.at(i);
		if (!e.seen) continue;
		const size_t n = Manifest::formatLine(e.path, e.size, line, sizeof(line));
		ok = n && f.write(reinterpret_cast<const uint8_t*>(line), n) == n;
	}
	f.close();
	if (!ok) {
		SD_MMC.remove(MANIFEST_TMP);
		return false;
	}
	SD_MMC.remove(LogPush::MANIFEST_PATH);
	return SD_MMC.rename(MANIFEST_TMP, LogPush::MANIFEST_PATH);
}

// Every pushable file on the card, outside CUR/: visit(rel, size).
template <typename F>
bool walkCard(F visit) {
	char (*days)[17] = static_cast<char(*)[17]>(heap_caps_malloc(MAX_DAYS * 17, MALLOC_CAP_SPIRAM));
	if (!days) return false;
	size_t nDays = 0;
	char rel[REL_MAX + 1];
	File root = SD_MMC.open(BCLogger::LOGDIR);
	if (!root || !root.isDirectory()) {
		if (root) root.close();
		free(days);
		return false;
	}
	// One directory handle open at a time: every open file costs ~4 KB of internal heap.
	for (File f = root.openNextFile(); f; f = root.openNextFile()) {
		if (f.isDirectory()) {
			if (validDay(f.name()) && nDays < MAX_DAYS) strlcpy(days[nDays++], f.name(), 17);
		} else if (pushable(f.name(), f.size()) && relPath("", f.name(), rel, sizeof(rel))) {
			visit(rel, static_cast<uint32_t>(f.size()));
		}
		f.close();
	}
	root.close();
	for (size_t d = 0; d < nDays; d++) {
		File dir = SD_MMC.open(BCLogger::LOGDIR + "/" + days[d]);
		if (!dir || !dir.isDirectory()) {
			if (dir) dir.close();
			continue;
		}
		for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
			if (!f.isDirectory() && pushable(f.name(), f.size()) && relPath(days[d], f.name(), rel, sizeof(rel))) {
				visit(rel, static_cast<uint32_t>(f.size()));
			}
			f.close();
		}
		dir.close();
		vTaskDelay(1);
	}
	free(days);
	return true;
}

// Where a file goes: below the folder URL as on the card, or flat into a files drop.
bool targetUrl(const Blob& c, bool drop, const char* dropBase, const char* host, const char* rel, char* out, size_t len) {
	if (!drop) return joinUrl(c.url, rel, out, len);
	char day[REL_MAX + 1] = "";
	const char* name = rel;
	if (const char* slash = strchr(rel, '/')) {
		snprintf(day, sizeof(day), "%.*s", static_cast<int>(slash - rel), rel);
		name = slash + 1;
	}
	char flat[WifiCfg::HOSTNAME_MAX + REL_MAX + 4];
	return dropName(host, day, name, flat, sizeof(flat)) && joinUrl(dropBase, flat, out, len);
}

}	// namespace

void LogPush::run(Mode m, bool manual) {
	Blob c;
	{
		Lock lock(mtx);
		c = cfg;
		st.filesDone = st.filesTodo = st.bytesDone = st.bytesTodo = 0;
		st.current[0] = 0;
	}
	lastRunMs = millis();
	if (m == Mode::PUSH) nextDue = lastRunMs + RETRY_MS;		// overwritten below on success
	// A Nextcloud upload link instead of a WebDAV folder: no account, the token is the user.
	char dropBase[URL_MAX + 1], token[TOKEN_MAX + 1], host[WifiCfg::HOSTNAME_MAX + 1];
	const bool drop = shareLink(c.url, dropBase, sizeof(dropBase), token, sizeof(token));
	webserver.getHostname(host, sizeof(host));
	const char* user = drop ? token : c.user;
	const char* nickname = drop ? host : nullptr;

	if (m == Mode::TEST) {
		setState("testing");
		setTlsPsram(true);
		int code;
		{
			Dav dav(c.url, user, c.secret, nickname);
			if (drop) {
				// A files drop can only be written to: leave a small file (the log service drops it).
				char url[URL_MAX + 64], text[64];
				snprintf(url, sizeof(url), "%s/%s~push-test.txt", dropBase, host);
				snprintf(text, sizeof(text), "%s log push test\n", host);
				code = dav.put(url, text);
				if (ok2xx(code)) code = 201;
			} else {
				code = dav.ensureDir(c.url);
			}
		}
		setTlsPsram(false);
		char msg[96];
		if (code == 201) {
			setState("ok", "");
			snprintf(msg, sizeof(msg), drop ? "upload link works (test file written)" : "target reachable, folder exists");
			nextDue = 0;					// upload right away
		} else {
			describeHttp(code, msg, sizeof(msg));
			setState("failed", msg);
		}
		{
			Lock lock(mtx);
			strlcpy(st.current, msg, sizeof(st.current));
		}
		bclog.logf(BCLogger::Log_Info, TAG, "Log push test: %s", msg);
		return;
	}

	setState("listing");
	Entry* mem = static_cast<Entry*>(heap_caps_calloc(MANIFEST_CAP, sizeof(Entry), MALLOC_CAP_SPIRAM));
	Todo* todo = m == Mode::PUSH ? static_cast<Todo*>(heap_caps_calloc(TODO_CAP, sizeof(Todo), MALLOC_CAP_SPIRAM)) : nullptr;
	if (!mem || (m == Mode::PUSH && !todo)) {
		free(mem);
		free(todo);
		setState("failed", "out of memory (PSRAM)");
		return;
	}
	Manifest man;
	man.attach(mem, MANIFEST_CAP);
	const size_t lines = loadManifest(man);

	size_t nTodo = 0, skippedTodo = 0;
	uint32_t bytesTodo = 0;
	bool full = false;
	const bool walked = walkCard([&](const char* rel, uint32_t size) {
		if (m == Mode::MARK) {
			if (!man.set(rel, size, true)) full = true;
		} else if (man.needsPush(rel, size)) {
			if (nTodo < TODO_CAP) {
				strlcpy(todo[nTodo].rel, rel, sizeof(todo[nTodo].rel));
				todo[nTodo].size = size;
				bytesTodo += size;
				nTodo++;
			} else {
				skippedTodo++;
			}
		}
	});
	if (!walked) {
		free(mem);
		free(todo);
		setState("failed", "SD card not readable");
		return;
	}
	if (full) bclog.logf(BCLogger::Log_Warn, TAG, "Log push: manifest full (%u files) - delete old sessions from the card", (unsigned) MANIFEST_CAP);

	if (m == Mode::MARK) {
		const bool ok = rewriteManifest(man);
		{
			Lock lock(mtx);
			st.manifestEntries = man.seenCount();
		}
		setState(ok ? "ok" : "failed", ok ? "" : "manifest not writable");
		bclog.logf(BCLogger::Log_Info, TAG, "Log push: %u file(s) on the card marked as uploaded", (unsigned) man.seenCount());
		free(mem);
		return;
	}

	{
		Lock lock(mtx);
		st.filesTodo = nTodo;
		st.bytesTodo = bytesTodo;
	}
	if (nTodo) {
		bclog.logf(BCLogger::Log_Info, TAG, "Log push: %u file(s), %u KB to upload%s", (unsigned) nTodo,
		           (unsigned) (bytesTodo / 1024), skippedTodo ? " (more in the next run)" : "");
	}

	char err[96] = "";
	uint32_t done = 0;
	if (nTodo) {
		setTlsPsram(true);
		Dav dav(c.url, user, c.secret, nickname);
		setState("uploading");
		for (size_t i = 0; i < nTodo; i++) {
			const Todo& t = todo[i];
			if (WiFi.status() != WL_CONNECTED) {strlcpy(err, "WiFi lost", sizeof(err)); break;}
			if (freeInternal() < ABORT_FREE_INTERNAL) {strlcpy(err, "internal memory low", sizeof(err)); break;}
			{
				Lock lock(mtx);
				if (!cfg.enabled && !manual) {strlcpy(err, "switched off", sizeof(err)); break;}
			}
			const String sdPath = BCLogger::LOGDIR + "/" + t.rel;
			char url[URL_MAX + REL_MAX + 2];
			if (!targetUrl(c, drop, dropBase, host, t.rel, url, sizeof(url))) {strlcpy(err, "URL too long", sizeof(err)); break;}
			{
				Lock lock(mtx);
				// BCLogger::deleteFile() asks isBusyWith() before it removes a file: an open file
				// that gets deleted damages the file system (CONFIG_FATFS_FS_LOCK=0, doc/PITFALLS.md)
				strlcpy(busyPath, sdPath.c_str(), sizeof(busyPath));
				strlcpy(st.current, t.rel, sizeof(st.current));
			}
			File f = SD_MMC.open(sdPath, FILE_READ);
			if (!f) {						// deleted in the meantime (log service, /del/)
				Lock lock(mtx);
				busyPath[0] = 0;
				continue;
			}
			const uint32_t size = static_cast<uint32_t>(f.size());
			int code = dav.put(url, f);
			if (!drop && (code == 409 || code == 404)) {		// the day folder does not exist yet
				char dir[URL_MAX + REL_MAX + 2];
				if (parentUrl(url, dir, sizeof(dir)) && dav.ensureDir(dir) == 201) {
					f.seek(0);
					code = dav.put(url, f);
				}
			}
			f.close();
			{
				Lock lock(mtx);
				busyPath[0] = 0;
			}
			if (!ok2xx(code)) {
				char what[64];
				describeHttp(code, what, sizeof(what));
				snprintf(err, sizeof(err), "%s: %s", t.rel, what);
				break;
			}
			man.set(t.rel, size, true);
			if (!appendManifest(t.rel, size)) {strlcpy(err, "manifest not writable", sizeof(err)); break;}
			done++;
			Lock lock(mtx);
			st.filesDone = done;
			st.bytesDone += size;
		}
		setTlsPsram(false);
	}

	if (!err[0] && man.needsCompaction(lines + done) && !rewriteManifest(man)) {
		bclog.log(BCLogger::Log_Warn, TAG, "Log push: manifest could not be compacted");
	}
	{
		Lock lock(mtx);
		st.manifestEntries = man.count();
		st.current[0] = 0;
	}
	free(mem);
	free(todo);

	if (err[0]) {
		setState("failed", err);
		bclog.logf(BCLogger::Log_Warn, TAG, "Log push stopped after %u of %u file(s): %s", (unsigned) done, (unsigned) nTodo, err);
		return;
	}
	nextDue = millis() + (skippedTodo ? 1000 : PERIOD_MS);
	{
		Lock lock(mtx);
		strlcpy(st.state, "ok", sizeof(st.state));
		st.lastError[0] = 0;
		st.lastOk = ClockSync::isValidNow() ? time(nullptr) : 0;
		st.lastOkFiles = done;
	}
	if (done) bclog.logf(BCLogger::Log_Info, TAG, "Log push: %u file(s) uploaded", (unsigned) done);
}

// ---------------------------------------------------------------------------
// CLI and web page
// ---------------------------------------------------------------------------

void LogPush::registerCli() {
	Command pushCmd = console.addCmd("push", +[](cmd* c) {
		Command command(c);
		const String a = command.getArgument(0).getValue();
		if (a == "now" || a == "test" || a == "mark") {
			const Mode m = a == "now" ? Mode::PUSH : a == "test" ? Mode::TEST : Mode::MARK;
			Serial.println(logPush.request(m) ? "Started (needs WiFi, except mark)" : "Busy, or no target set (/push)");
			return;
		}
		if (a == "on" || a == "off") {
			logPush.setEnabled(a == "on");
		}
		LogPushStatus s;
		logPush.getStatus(s);
		Serial.printf("Push %s, target %s%s, %s\r\n", s.enabled ? "on" : "off", s.url[0] ? s.url : "(none - set on /push)",
		              s.hasSecret ? " (password set)" : "", s.running ? "running" : s.state);
		if (s.filesTodo) Serial.printf("Last run: %u of %u file(s), %u of %u KB\r\n", (unsigned) s.filesDone, (unsigned) s.filesTodo,
		                               (unsigned) (s.bytesDone / 1024), (unsigned) (s.bytesTodo / 1024));
		if (s.lastError[0]) Serial.printf("Error: %s\r\n", s.lastError);
		if (s.lastRunAgoS) Serial.printf("%u s ago, %u file(s) known as uploaded\r\n", (unsigned) s.lastRunAgoS, (unsigned) s.manifestEntries);
	}, +[](uint8_t pos, SerialConsole::Matches& m) {
		if (pos == 1) {
			for (const char* a : {"status", "now", "test", "mark", "on", "off"}) m.add(a);
		}
	});
	pushCmd.addPositionalArgument("a", "status");
	pushCmd.setDescription("Log upload to WebDAV/Nextcloud: push [status] | push now | push test | push mark (all as uploaded) | push on|off");
}

static const char PAGE_CSS[] =
	"h3{margin:22px 0 6px}"
	"form label{display:block;margin-top:10px}"
	"form .row,.row{margin-top:12px}"
	"input[type=text],input[type=password],input[type=url]{width:100%}"
	"code{word-break:break-all}";

static const char PAGE_BODY[] = R"HTML(
<p class="eyebrow">Uploads finished sessions whenever the bike computer is online - for a bike that is
not parked in reach of the WiFi the log service pulls from. The files stay on the card; the service
takes pushed and pulled files alike.</p>
<p id="status">&nbsp;</p>
<p class="eyebrow" id="detail">&nbsp;</p>
<div class="row">
  <button type="button" onclick="act('/push/now')">Upload now</button>
  <button type="button" onclick="act('/push/test')">Test target</button>
  <button type="button" onclick="mark()">Mark all as uploaded</button>
</div>

<h3>Target</h3>
<form onsubmit="save();return false;" autocomplete="off">
  <label><input type="checkbox" id="en"> upload automatically</label>
  <label for="url">Upload link or folder URL (WebDAV)</label>
  <input type="text" id="url" maxlength="200" placeholder="https://cloud.example.com/s/TOKEN">
  <label for="user">User</label>
  <input type="text" id="user" maxlength="64" placeholder="empty for an upload link">
  <label for="pw">Password</label>
  <input type="password" id="pw" maxlength="128" autocomplete="new-password" placeholder="leave empty to keep the stored one">
  <label><input type="checkbox" id="clr"> remove the stored password</label>
  <div class="row"><input type="submit" value="Save"></div>
</form>
<p class="eyebrow">Simplest: a Nextcloud <b>upload link</b> (share a folder, "File drop"), e.g.
<code>https://cloud.example.com/s/TOKEN</code> - user empty, password only if the share has one. The files
arrive there flat as <code>DEVICE~DAY~FILE</code> (this device: <b id="host"></b>).<br>
Or a WebDAV folder with an account: <code>https://CLOUD/remote.php/dav/files/USER/BikeLog-Inbox/DEVICE</code>
plus user and app password (Settings, Security, "Devices &amp; sessions"); missing folders are created.<br>
Or directly to the log service: <code>http://SERVER:8080/api/v1/devices/DEVICE/files</code>, user empty.<br>
The log service files the sessions under the device BIKELOG_PULL_TARGETS maps the network name to.</p>
)HTML";

static const char PAGE_SCRIPT[] = R"JS(
const $=id=>document.getElementById(id);
let filled=false;
async function post(url,data){
  try{const r=await fetch(url,{method:'POST',body:new URLSearchParams(data||{})});
    const t=await r.text();toast(t||(r.ok?'OK':'Failed ('+r.status+')'),!r.ok);return r.ok;
  }catch(e){toast('Request failed',true);return false;}
}
function kb(b){return (b/1024).toFixed(0)+' KB';}
async function load(){
  try{
    const s=await (await fetch('/push/state',{cache:'no-store'})).json();
    let t=!s.configured?'No target set':!s.enabled?'Off':s.running?'Running: '+s.state:'On, '+s.state;
    if(s.lastError)t+=' - '+s.lastError;
    $('status').textContent=t;
    let d=[];
    if(s.running&&s.filesTodo)d.push('file '+(s.filesDone+1)+' of '+s.filesTodo+(s.current?' ('+s.current+')':'')+', '+kb(s.bytesDone)+' of '+kb(s.bytesTodo));
    else if(s.current)d.push(s.current);
    if(s.lastRunAgo)d.push('last run '+Math.round(s.lastRunAgo/60)+' min ago');
    if(s.lastOk)d.push('last complete upload '+new Date(s.lastOk*1000).toLocaleString()+' ('+s.lastOkFiles+' file(s))');
    if(s.manifest)d.push(s.manifest+' file(s) known as uploaded');
    $('detail').textContent=d.join(' · ')||' ';
    $('host').textContent=s.hostname;
    if(!filled){filled=true;$('en').checked=s.enabled;$('url').value=s.url;$('user').value=s.user;
      $('pw').placeholder=s.hasSecret?'stored - leave empty to keep it':'not set';}
  }catch(e){$('status').textContent='No answer from the device';}
}
async function save(){
  if(await post('/push/config',{enabled:$('en').checked?1:0,url:$('url').value.trim(),user:$('user').value.trim(),
      secret:$('pw').value,clear:$('clr').checked?1:0})){$('pw').value='';$('clr').checked=false;filled=false;load();}
}
async function act(u){if(await post(u))setTimeout(load,500);}
function mark(){if(confirm('Record every file on the card as uploaded, without uploading it? Only new sessions will be uploaded then.'))act('/push/mark');}
load();setInterval(load,3000);
)JS";

static String param(AsyncWebServerRequest* request, const char* name) {
	return request->hasParam(name, true) ? request->getParam(name, true)->value() : String();
}

void LogPush::registerRoutes() {
	AsyncWebServer& server = webserver.getServer();
	server.on("/push/state", HTTP_GET, [this](AsyncWebServerRequest* request) {
		LogPushStatus s;
		getStatus(s);
		JsonDocument doc;
		doc["enabled"] = s.enabled;
		doc["configured"] = s.configured;
		doc["running"] = s.running;
		doc["url"] = s.url;
		doc["user"] = s.user;
		doc["hasSecret"] = s.hasSecret;
		doc["state"] = s.state;
		doc["current"] = s.current;
		doc["lastError"] = s.lastError;
		doc["filesDone"] = s.filesDone;
		doc["filesTodo"] = s.filesTodo;
		doc["bytesDone"] = s.bytesDone;
		doc["bytesTodo"] = s.bytesTodo;
		doc["manifest"] = s.manifestEntries;
		doc["lastRunAgo"] = s.lastRunAgoS;
		doc["lastOk"] = static_cast<int64_t>(s.lastOk);
		doc["lastOkFiles"] = s.lastOkFiles;
		char host[WifiCfg::HOSTNAME_MAX + 1];
		webserver.getHostname(host, sizeof(host));
		doc["hostname"] = host;
		String out;
		serializeJson(doc, out);
		AsyncWebServerResponse* response = request->beginResponse(200, "application/json", out);
		response->addHeader("Cache-Control", "no-store");
		request->send(response);
	});

	server.on("/push/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
		const SaveResult r = configure(param(request, "enabled") == "1", param(request, "url").c_str(),
		                               param(request, "user").c_str(), param(request, "secret").c_str(),
		                               param(request, "clear") == "1");
		switch (r) {
		case SaveResult::OK: request->send(200, "text/plain", "Saved"); break;
		case SaveResult::BAD_URL: request->send(400, "text/plain", "The URL must start with http:// or https:// and contain no spaces, '?' or '#'"); break;
		case SaveResult::TOO_LONG: request->send(400, "text/plain", "User or password too long"); break;
		}
	});

	static const struct {const char* uri; Mode mode; const char* ok;} ACTIONS[] = {
		{"/push/now", Mode::PUSH, "Upload started"},
		{"/push/test", Mode::TEST, "Testing ..."},
		{"/push/mark", Mode::MARK, "Marking ..."},
	};
	for (const auto& a : ACTIONS) {
		const Mode mode = a.mode;
		const char* okText = a.ok;
		server.on(a.uri, HTTP_POST, [this, mode, okText](AsyncWebServerRequest* req) {
			if (request(mode)) req->send(200, "text/plain", okText);
			else req->send(409, "text/plain", "Busy, or no target set");
		});
	}

	// Registered last: the prefix route "/push" also matches "/push/..." (WifiRoutes.cpp).
	server.on("/push", HTTP_GET, [](AsyncWebServerRequest* request) {
		String html;
		html.reserve(8192);
		WebPage::begin(html, "Log upload", PAGE_CSS);
		html += PAGE_BODY;
		WebPage::end(html, PAGE_SCRIPT);
		request->send(200, "text/html", html);
	});
}
