// Host mock: just enough of Arduino.h for src/SerialConsole.cpp
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <strings.h>

using std::max;
using std::min;

class String {
public:
	String() {}
	String(const char* s) : s(s ? s : "") {}
	String(const char* p, unsigned n) : s(p, n) {}
	String(const std::string& x) : s(x) {}
	unsigned length() const {return s.size();}
	const char* c_str() const {return s.c_str();}
	bool isEmpty() const {return s.empty();}
	bool equalsIgnoreCase(const String& o) const {return strcasecmp(s.c_str(), o.s.c_str()) == 0;}
	bool operator==(const char* o) const {return s == o;}
	String& operator+=(const String& o) {s += o.s; return *this;}
	String& operator+=(const char* o) {s += o; return *this;}
	String& operator+=(char c) {s += c; return *this;}
	friend String operator+(const String& a, const String& b) {return String(a.s + b.s);}
	friend String operator+(const char* a, const String& b) {return String(a + b.s);}
	std::string s;
};

// Serial: input comes from a queue the test fills, output is collected in a string
class MockSerial {
public:
	std::string in, out;
	size_t inPos = 0;
	int available() {return in.size() - inPos;}
	int read() {return inPos < in.size() ? static_cast<uint8_t>(in[inPos++]) : -1;}
	size_t write(const uint8_t* p, size_t n) {out.append(reinterpret_cast<const char*>(p), n); return n;}
	size_t write(uint8_t c) {out += static_cast<char>(c); return 1;}
	size_t print(const char* s) {out += s; return strlen(s);}
	size_t print(char c) {out += c; return 1;}
	size_t print(const String& s) {out += s.s; return s.length();}
	size_t println(const char* s) {out += s; out += "\r\n"; return strlen(s) + 2;}
	size_t println(const String& s) {return println(s.c_str());}
	size_t printf(const char* fmt, ...) {
		char b[512];
		va_list a;
		va_start(a, fmt);
		int n = vsnprintf(b, sizeof(b), fmt, a);
		va_end(a);
		out += b;
		return n;
	}
};
extern MockSerial Serial;

extern uint32_t mockMillis;
inline uint32_t millis() {return mockMillis;}
