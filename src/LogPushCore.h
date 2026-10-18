/*
 * LogPushCore.h
 *
 * The pure part of the log push (LogPush.h): configuration blob, URL building and the manifest
 * of what has been uploaded -- no Arduino, no SD, no network. Host test:
 * test/native_logpush/logpush_test.cpp.
 *
 * The push is the counterpart of the log service's pull for a bike computer that is parked
 * outside the home WiFi: whenever it is online it uploads every finished session file it has
 * not uploaded yet with a plain WebDAV PUT to
 *
 *   <base URL>/<day>/<name>          e.g. .../BikeLog-Inbox/TRGB-FL/20261018/L_143012.bin
 *   <base URL>/<name>                files directly in /BIKECOMP (old layout)
 *
 * -- the same relative paths the pull uses (/log/<day>/<name>). The base URL is a folder on a
 * Nextcloud (the log service collects it from there, doc/LOGSERVICE.md "Push") or the service's
 * own upload endpoint .../api/v1/devices/<device>/files.
 *
 * The manifest (one line "<path> <size>" per uploaded file, on the SD card) is what makes this
 * cheap: a file is uploaded again only if its size changed (LogSessions renews I_ files when the
 * summary format changes). It is kept sorted in memory for the lookups; the caller provides the
 * memory (PSRAM on the device).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace PushCore {

constexpr size_t URL_MAX = 200;
constexpr size_t USER_MAX = 64;
constexpr size_t SECRET_MAX = 128;		// Nextcloud app password, or a bearer token of the log service
constexpr size_t REL_MAX = 47;			// "<day>/<name>" -- sdlayout.py: day <= 16, name <= 37

// What goes into the NVS. Bump `version` if a field moves.
struct Blob {
	uint8_t version;
	uint8_t enabled;
	uint8_t reserved[2];
	char url[URL_MAX + 1];
	char user[USER_MAX + 1];			// empty + secret = "Authorization: Bearer <secret>"
	char secret[SECRET_MAX + 1];		// never logged, never sent to a client
};
constexpr uint8_t BLOB_VERSION = 1;

// http:// or https://, a host, no whitespace, quotes, '<>', query or fragment.
bool validUrl(const char* url);
bool isHttps(const char* url);
// A day directory the push takes: [A-Za-z0-9_]{1,16}, not the working directory "CUR".
bool validDay(const char* day);
// A file the push takes: sdlayout.py's name pattern, not hidden ('x...'), not empty.
bool pushable(const char* name, uint32_t size);

// "<day>/<name>", or "<name>" for an empty day. false if it does not fit REL_MAX.
bool relPath(const char* day, const char* name, char* out, size_t len);
// "<base>/<rel>"; a trailing '/' of the base is dropped. false if it does not fit `len`.
bool joinUrl(const char* base, const char* rel, char* out, size_t len);
// The URL one path segment up: "https://h/a/b/" -> "https://h/a". false at the host itself.
bool parentUrl(const char* url, char* out, size_t len);

// Nextcloud share link ("upload link", files drop): "https://cloud/s/TOKEN" (also
// ".../index.php/s/TOKEN"). Its WebDAV side is "https://cloud/public.php/webdav" with the token as
// the Basic auth user and the share password, if any, as the password. false for any other URL.
// A files drop takes nothing but PUT and keeps only the last path segment (Nextcloud 31,
// FilesDropPlugin), so uploads to it get a flat name: dropName().
constexpr size_t TOKEN_MAX = 64;
bool shareLink(const char* url, char* davBase, size_t baseLen, char* token, size_t tokenLen);
// "<host>~<day>~<name>" ("<host>~~<name>" for a file directly in /BIKECOMP); the log service
// splits it again (inbox.py). false if it does not fit.
bool dropName(const char* host, const char* day, const char* name, char* out, size_t len);

struct Entry {
	char path[REL_MAX + 1];
	uint32_t size;
	uint8_t seen;			// the file is still on the card (compact() keeps only these)
};

class Manifest {
public:
	// Uses `mem` for up to `cap` entries; empties the manifest.
	void attach(Entry* mem, size_t cap);
	size_t count() const {return n;}
	size_t capacity() const {return cap;}
	const Entry& at(size_t i) const {return e[i];}

	// One line of the manifest file ("<path> <size>", trailing CR/LF allowed). Later lines win.
	// false for a malformed line (ignored) or when full.
	bool parseLine(const char* line);
	// "<path> <size>\n"; returns the length, 0 if it does not fit.
	static size_t formatLine(const char* path, uint32_t size, char* out, size_t len);

	bool set(const char* path, uint32_t size, bool seen = false);	// insert or update; false when full
	// Not uploaded yet, or with a different size. Marks the entry as seen.
	bool needsPush(const char* path, uint32_t size);
	int find(const char* path) const;					// index or -1
	// Worth rewriting the file: it has this many lines, but only count()/seen ones are needed.
	bool needsCompaction(size_t fileLines) const;
	size_t seenCount() const;

private:
	Entry* e = nullptr;
	size_t n = 0;
	size_t cap = 0;
	size_t lowerBound(const char* path) const;
};

// Rewrite the manifest when it has at least this many lines too many.
constexpr size_t COMPACT_SLACK = 64;

}	// namespace PushCore
