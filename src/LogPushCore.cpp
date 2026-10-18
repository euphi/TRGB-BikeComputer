/*
 * LogPushCore.cpp -- see LogPushCore.h.
 */

#include "LogPushCore.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace PushCore {

static bool startsWith(const char* s, const char* prefix) {
	return strncmp(s, prefix, strlen(prefix)) == 0;
}

static const char* afterScheme(const char* url) {
	if (startsWith(url, "https://")) return url + 8;
	if (startsWith(url, "http://")) return url + 7;
	return nullptr;
}

bool validUrl(const char* url) {
	if (!url) return false;
	const size_t len = strlen(url);
	if (len > URL_MAX) return false;
	const char* host = afterScheme(url);
	if (!host || !*host || *host == '/') return false;
	for (const char* p = url; *p; p++) {
		const unsigned char c = static_cast<unsigned char>(*p);
		if (c <= ' ' || c >= 0x7f || c == '"' || c == '\'' || c == '<' || c == '>' || c == '?' || c == '#' || c == '\\') return false;
	}
	return true;
}

bool isHttps(const char* url) {
	return url && startsWith(url, "https://");
}

static bool wordChar(char c) {
	return isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool validDay(const char* day) {
	if (!day) return false;
	const size_t len = strlen(day);
	if (len < 1 || len > 16 || strcmp(day, "CUR") == 0) return false;
	for (size_t i = 0; i < len; i++) {
		if (!wordChar(day[i])) return false;
	}
	return true;
}

// sdlayout.py: ^[A-Za-z][A-Za-z0-9_]{0,31}\.[A-Za-z0-9]{1,4}$
bool pushable(const char* name, uint32_t size) {
	if (!name || size == 0) return false;
	if (!isalpha(static_cast<unsigned char>(name[0])) || name[0] == 'x') return false;
	const char* dot = strchr(name, '.');
	if (!dot || dot - name > 32) return false;
	for (const char* p = name + 1; p < dot; p++) {
		if (!wordChar(*p)) return false;
	}
	const size_t ext = strlen(dot + 1);
	if (ext < 1 || ext > 4) return false;
	for (const char* p = dot + 1; *p; p++) {
		if (!isalnum(static_cast<unsigned char>(*p))) return false;
	}
	return true;
}

bool relPath(const char* day, const char* name, char* out, size_t len) {
	const size_t cap = len < REL_MAX + 1 ? len : REL_MAX + 1;
	const int w = (day && *day) ? snprintf(out, cap, "%s/%s", day, name) : snprintf(out, cap, "%s", name);
	return w > 0 && static_cast<size_t>(w) < cap;
}

bool joinUrl(const char* base, const char* rel, char* out, size_t len) {
	size_t baseLen = strlen(base);
	while (baseLen && base[baseLen - 1] == '/') baseLen--;
	const int w = snprintf(out, len, "%.*s/%s", static_cast<int>(baseLen), base, rel);
	return w > 0 && static_cast<size_t>(w) < len;
}

bool parentUrl(const char* url, char* out, size_t len) {
	const char* host = afterScheme(url);
	if (!host) return false;
	size_t end = strlen(url);
	while (end && url[end - 1] == '/') end--;		// "…/b/" is the same folder as "…/b"
	const char* slash = nullptr;
	for (const char* p = url + end; p > host; p--) {
		if (p[-1] == '/') {slash = p - 1; break;}
	}
	if (!slash || slash < host) return false;			// only the host is left
	const size_t n = static_cast<size_t>(slash - url);
	if (n + 1 > len) return false;
	memcpy(out, url, n);
	out[n] = 0;
	return true;
}

bool shareLink(const char* url, char* davBase, size_t baseLen, char* token, size_t tokenLen) {
	const char* host = afterScheme(url);
	if (!host) return false;
	const char* s = strstr(host, "/s/");
	if (!s) return false;
	const char* tok = s + 3;
	size_t n = 0;
	while (isalnum(static_cast<unsigned char>(tok[n]))) n++;
	if (n == 0 || n > TOKEN_MAX || n + 1 > tokenLen) return false;
	if (tok[n] && !(tok[n] == '/' && tok[n + 1] == 0)) return false;	// something after the token
	size_t prefix = static_cast<size_t>(s - url);
	static const char INDEX[] = "/index.php";
	const size_t il = sizeof(INDEX) - 1;
	if (prefix >= il && strncmp(url + prefix - il, INDEX, il) == 0) prefix -= il;
	const int w = snprintf(davBase, baseLen, "%.*s/public.php/webdav", static_cast<int>(prefix), url);
	if (w <= 0 || static_cast<size_t>(w) >= baseLen) return false;
	memcpy(token, tok, n);
	token[n] = 0;
	return true;
}

bool dropName(const char* host, const char* day, const char* name, char* out, size_t len) {
	const int w = snprintf(out, len, "%s~%s~%s", host, day ? day : "", name);
	return w > 0 && static_cast<size_t>(w) < len;
}

// --- Manifest ---------------------------------------------------------------------------

void Manifest::attach(Entry* mem, size_t capacity) {
	e = mem;
	cap = mem ? capacity : 0;
	n = 0;
}

size_t Manifest::lowerBound(const char* path) const {
	size_t lo = 0, hi = n;
	while (lo < hi) {
		const size_t mid = (lo + hi) / 2;
		if (strcmp(e[mid].path, path) < 0) lo = mid + 1;
		else hi = mid;
	}
	return lo;
}

int Manifest::find(const char* path) const {
	const size_t i = lowerBound(path);
	return (i < n && strcmp(e[i].path, path) == 0) ? static_cast<int>(i) : -1;
}

bool Manifest::set(const char* path, uint32_t size, bool seen) {
	if (!path || !*path || strlen(path) > REL_MAX) return false;
	const size_t i = lowerBound(path);
	if (i < n && strcmp(e[i].path, path) == 0) {
		e[i].size = size;
		if (seen) e[i].seen = 1;
		return true;
	}
	if (n >= cap) return false;
	memmove(&e[i + 1], &e[i], (n - i) * sizeof(Entry));
	memset(&e[i], 0, sizeof(Entry));
	strcpy(e[i].path, path);
	e[i].size = size;
	e[i].seen = seen ? 1 : 0;
	n++;
	return true;
}

bool Manifest::parseLine(const char* line) {
	if (!line) return false;
	const char* sp = strchr(line, ' ');
	if (!sp || sp == line || static_cast<size_t>(sp - line) > REL_MAX) return false;
	char path[REL_MAX + 1];
	memcpy(path, line, sp - line);
	path[sp - line] = 0;
	if (!isdigit(static_cast<unsigned char>(sp[1]))) return false;
	char* end = nullptr;
	const unsigned long size = strtoul(sp + 1, &end, 10);
	while (*end == '\r' || *end == '\n') end++;
	if (*end) return false;
	return set(path, static_cast<uint32_t>(size));
}

size_t Manifest::formatLine(const char* path, uint32_t size, char* out, size_t len) {
	const int w = snprintf(out, len, "%s %lu\n", path, static_cast<unsigned long>(size));
	return (w > 0 && static_cast<size_t>(w) < len) ? static_cast<size_t>(w) : 0;
}

bool Manifest::needsPush(const char* path, uint32_t size) {
	const int i = find(path);
	if (i < 0) return true;
	e[i].seen = 1;
	return e[i].size != size;
}

size_t Manifest::seenCount() const {
	size_t s = 0;
	for (size_t i = 0; i < n; i++) s += e[i].seen;
	return s;
}

bool Manifest::needsCompaction(size_t fileLines) const {
	return fileLines >= seenCount() + COMPACT_SLACK;
}

}	// namespace PushCore
