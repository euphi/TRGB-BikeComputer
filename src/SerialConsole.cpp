/*
 * SerialConsole.cpp
 *
 *  Created on: 27.09.2026
 */

#include "SerialConsole.h"
#include <Singletons.h>			// angle brackets: test/native_console substitutes a mock
#include <esp_heap_caps.h>
#include <strings.h>

namespace {
// Common prefix length of a and b, ignoring case
uint8_t commonLen(const char* a, const char* b, uint8_t max) {
	uint8_t n = 0;
	while (n < max && a[n] && b[n] && tolower(a[n]) == tolower(b[n])) n++;
	return n;
}
}

// ----- Matches -----

void SerialConsole::Matches::add(const char* candidate) {
	if (!candidate || strncasecmp(candidate, prefix, prefixLen) != 0) return;
	const uint8_t n = count < MAX_KEPT ? count : MAX_KEPT;
	for (uint8_t i = 0; i < n; i++) {
		if (strcasecmp(kept[i], candidate) == 0) return;
	}
	if (count == 0) {
		common = strnlen(candidate, 255);
	} else {
		common = commonLen(kept[0], candidate, common);
	}
	if (count < MAX_KEPT) kept[count] = candidate;
	if (count < 255) count++;
}

String SerialConsole::Matches::word(uint8_t i) const {
	if (i >= words) return String();
	return String(line + start[i], len[i]);
}

// ----- Setup and registration -----

void SerialConsole::setup() {
	if (!mutex) mutex = xSemaphoreCreateMutex();
	if (!hist) {
		// ~2 KB, touched only while typing: PSRAM is fine (doc/PITFALLS.md, "PSRAM und Display-Flackern")
		hist = static_cast<char (*)[LINE_LEN + 1]>(heap_caps_calloc(HIST_LEN + 1, LINE_LEN + 1, MALLOC_CAP_SPIRAM));
	}
	Command help = addCmd("help", [](cmd* c) {console.printHelp(Command(c).getArgument("command").getValue());}, completeCommandName);
	help.addPositionalArgument("command", "");
	help.setDescription("Lists the commands, or shows one: help [command]. Tab completes commands and arguments.");
}

Command SerialConsole::addCmd(const char* name, void (*callback)(cmd* c), Completer completer) {
	if (nCmds < MAX_CMDS) {
		cmds[nCmds++] = {name, completer};
	} else {
		bclog.logf(BCLogger::Log_Error, BCLogger::TAG_CLI, "SerialConsole: MAX_CMDS too small, no completion for %s", name);
	}
	return cli.addCmd(name, callback);
}

void SerialConsole::completeCommandName(uint8_t pos, Matches& m) {
	if (pos != 1) return;
	for (uint8_t i = 0; i < console.nCmds; i++) m.add(console.cmds[i].name);
}

void SerialConsole::printHelp(const String& name) {
	for (uint8_t i = 0; i < nCmds; i++) {
		if (name.length() && !name.equalsIgnoreCase(cmds[i].name)) continue;
		Command c = cli.getCmd(cmds[i].name);
		String s;
		c.toString(s, false);
		Serial.print(s);
		Serial.print("\r\n");
		if (c.hasDescription()) {
			Serial.print("    ");
			Serial.print(c.getDescription());
			Serial.print("\r\n");
		}
		if (name.length()) return;
	}
	if (name.length()) Serial.printf("Unknown command %s\r\n", name.c_str());
}

// ----- Output from other tasks -----

bool SerialConsole::lock(TickType_t wait) {
	return mutex && xSemaphoreTake(mutex, wait) == pdTRUE;
}

void SerialConsole::unlock() {
	xSemaphoreGive(mutex);
}

SerialConsole::Output::Output(SerialConsole& c) : con(c) {
	locked = con.lock(pdMS_TO_TICKS(100));
	if (locked) con.hide();
}

SerialConsole::Output::~Output() {
	if (!locked) return;
	if (con.shown) con.refresh();
	con.unlock();
}

void SerialConsole::hide() {
	if (!shown) return;
	Serial.write('\r');
	repeat(' ', PROMPT_LEN + drawnWidth);
	Serial.write('\r');
	drawnWidth = 0;
}

// ----- Input -----

void SerialConsole::poll() {
	if (!started) {
		if (!lock(pdMS_TO_TICKS(100))) return;
		started = true;
		showPrompt();
		unlock();
	}
	if (!Serial.available()) return;
	if (!lock(pdMS_TO_TICKS(100))) return;			// keys stay in the receive buffer
	char cmdLine[LINE_LEN + 1];
	bool enter = false;
	// Stop at Enter: the command runs without the lock (its log output needs it), the rest of
	// the input waits for the next poll().
	while (!enter && Serial.available()) {
		int c = Serial.read();
		if (c < 0) break;
		enter = feed(static_cast<char>(c), cmdLine);
	}
	if (!enter) draw();
	unlock();
	if (!enter) return;

	run(cmdLine);
	if (lock(portMAX_DELAY)) {
		showPrompt();
		unlock();
	}
}

void SerialConsole::run(const char* cmdLine) {
	const char* p = cmdLine;
	while (*p == ' ') p++;
	if (!*p) return;
	// The log goes to the SD card and is picked up by the log service: nothing secret in it.
	// "wifi add|apset <ssid> <password>" is logged up to the subcommand only.
	size_t keep = 0;
	if (!strncasecmp(p, "wifi add", 8) && (p[8] == ' ' || p[8] == '\0')) keep = 8;
	else if (!strncasecmp(p, "wifi apset", 10) && (p[10] == ' ' || p[10] == '\0')) keep = 10;
	if (keep) {
		char shown[16];
		snprintf(shown, sizeof(shown), "%.*s", (int) keep, p);
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, "Received command: " + String(shown) + " ...");
	} else {
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, "Received command: " + String(p));
	}
	cli.parse(p);
}

// Returns true on Enter, with the line copied to cmdOut.
bool SerialConsole::feed(char c, char* cmdOut) {
	if (esc != ESC_NONE) {
		// A lone ESC (or Alt+key the sequence parser doesn't know) must not swallow later keys
		if (millis() - escTime > 100) esc = ESC_NONE;
		else if (feedEscape(c)) return false;
	}
	const bool afterCR = lastCR;
	lastCR = (c == '\r');

	switch (c) {
	case '\n':
		if (afterCR) return false;			// second half of CRLF
		// fall through
	case '\r':
		memcpy(cmdOut, line, len);
		cmdOut[len] = '\0';
		histAdd();
		scroll = 0;
		refresh();							// keys of this batch may not be on screen yet
		newline();
		shown = false;
		len = cur = 0;
		line[0] = '\0';
		return true;
	case 0x1b:
		esc = ESC_START;
		escTime = millis();
		break;
	case 0x7f:								// Backspace (DEL), most terminals
	case 0x08:								// Backspace, Windows miniterm
		if (cur > 0) erase(cur - 1, cur);
		break;
	case '\t':
		complete();
		break;
	case 0x01: cur = 0; redraw = true; break;							// Ctrl-A
	case 0x05: cur = len; redraw = true; break;							// Ctrl-E
	case 0x02: if (cur > 0) {cur--; redraw = true;} break;				// Ctrl-B
	case 0x06: if (cur < len) {cur++; redraw = true;} break;			// Ctrl-F
	case 0x04: if (cur < len) erase(cur, cur + 1); break;				// Ctrl-D
	case 0x0b: erase(cur, len); break;									// Ctrl-K
	case 0x15: erase(0, cur); break;									// Ctrl-U
	case 0x17: erase(wordLeft(), cur); break;							// Ctrl-W
	case 0x10: histMove(1); break;										// Ctrl-P
	case 0x0e: histMove(-1); break;										// Ctrl-N
	case 0x03:															// Ctrl-C
		cur = len;
		refresh();
		Serial.print("^C");
		newline();
		len = cur = scroll = 0;
		line[0] = '\0';
		histPos = -1;
		showPrompt();
		break;
	case 0x0c:															// Ctrl-L: redraw on a fresh line
		newline();
		redraw = true;
		break;
	default:
		if (c >= 0x20 && c < 0x7f) insert(&c, 1);
		// Everything else is dropped: other control keys, and non-ASCII (the cursor
		// arithmetic counts one byte per column).
		break;
	}
	return false;
}

// Handles c as part of an escape sequence; false if c ended the sequence without using it.
bool SerialConsole::feedEscape(char c) {
	char key = 0;
	switch (esc) {
	case ESC_START:
		if (c == '[') {esc = ESC_CSI; escParam[0] = escParam[1] = 0; escParams = 0; return true;}
		if (c == 'O') {esc = ESC_SS3; return true;}
		esc = ESC_NONE;
		if (c == 'b') {cur = wordLeft(); redraw = true; return true;}					// Alt-B
		if (c == 'f') {cur = wordRight(); redraw = true; return true;}					// Alt-F
		if (c == 0x7f || c == 0x08) {erase(wordLeft(), cur); return true;}				// Alt-Backspace
		return false;
	case ESC_CSI:
		if (c >= '0' && c <= '9') {
			if (escParams < 2) escParam[escParams] = escParam[escParams] * 10 + (c - '0');
			return true;
		}
		if (c == ';') {if (escParams < 2) escParams++; return true;}
		if (c < 0x40 || c > 0x7e) return true;		// intermediate bytes, ignored
		esc = ESC_NONE;
		if (c == '~') {
			switch (escParam[0]) {
			case 1: case 7: key = 'H'; break;
			case 4: case 8: key = 'F'; break;
			case 3: if (cur < len) erase(cur, cur + 1); return true;
			default: return true;					// Insert, PgUp/PgDn, F-keys
			}
		} else {
			key = c;
		}
		break;
	case ESC_SS3:
		esc = ESC_NONE;
		key = c;
		break;
	default:
		esc = ESC_NONE;
		return false;
	}

	// ESC [1;5C and friends: Ctrl (5) or Alt (3) plus arrow moves by word
	const bool byWord = escParams >= 1 && (escParam[1] == 5 || escParam[1] == 3);
	switch (key) {
	case 'A': histMove(1); break;
	case 'B': histMove(-1); break;
	case 'C': if (cur < len) {cur = byWord ? wordRight() : cur + 1; redraw = true;} break;
	case 'D': if (cur > 0) {cur = byWord ? wordLeft() : cur - 1; redraw = true;} break;
	case 'H': cur = 0; redraw = true; break;
	case 'F': cur = len; redraw = true; break;
	default: break;
	}
	return true;
}

// ----- Editing -----

void SerialConsole::insert(const char* s, uint8_t n) {
	if (n > LINE_LEN - len) n = LINE_LEN - len;
	if (n == 0) return;
	if (cur < len) {
		memmove(line + cur + n, line + cur, len - cur);
		redraw = true;
	}
	memcpy(line + cur, s, n);
	len += n;
	cur += n;
	line[len] = '\0';
}

void SerialConsole::erase(uint8_t from, uint8_t to) {
	if (to <= from) return;
	memmove(line + from, line + to, len - to);
	len -= to - from;
	line[len] = '\0';
	cur = from;
	redraw = true;
}

void SerialConsole::setLine(const char* s) {
	len = cur = 0;
	insert(s, strnlen(s, LINE_LEN));
	redraw = true;
}

uint8_t SerialConsole::wordLeft() const {
	uint8_t p = cur;
	while (p > 0 && line[p - 1] == ' ') p--;
	while (p > 0 && line[p - 1] != ' ') p--;
	return p;
}

uint8_t SerialConsole::wordRight() const {
	uint8_t p = cur;
	while (p < len && line[p] == ' ') p++;
	while (p < len && line[p] != ' ') p++;
	return p;
}

void SerialConsole::histAdd() {
	histPos = -1;
	if (!hist || len == 0) return;
	if (histCount > 0 && strcmp(histEntry(0), line) == 0) return;
	memcpy(hist[histNext], line, len + 1);
	histNext = (histNext + 1) % HIST_LEN;
	if (histCount < HIST_LEN) histCount++;
}

// dir 1 = older, -1 = newer
void SerialConsole::histMove(int8_t dir) {
	if (!hist) return;
	char* edited = hist[HIST_LEN];
	const int8_t pos = histPos + dir;
	if (pos < -1 || pos >= histCount) return;
	if (histPos == -1) memcpy(edited, line, len + 1);
	histPos = pos;
	setLine(pos == -1 ? edited : histEntry(pos));
}

void SerialConsole::complete() {
	uint8_t ws = cur;								// start of the word at the cursor
	while (ws > 0 && line[ws - 1] != ' ') ws--;

	Matches m;
	m.line = line;
	m.prefix = line + ws;
	m.prefixLen = cur - ws;
	for (uint8_t p = 0; p < ws && m.words < Matches::MAX_WORDS;) {
		while (p < ws && line[p] == ' ') p++;
		if (p >= ws) break;
		m.start[m.words] = p;
		while (p < ws && line[p] != ' ') p++;
		m.len[m.words] = p - m.start[m.words];
		m.words++;
	}

	if (m.words == 0) {
		for (uint8_t i = 0; i < nCmds; i++) m.add(cmds[i].name);
	} else {
		const String name = m.word(0);
		for (uint8_t i = 0; i < nCmds; i++) {
			if (cmds[i].completer && name.equalsIgnoreCase(cmds[i].name)) {
				cmds[i].completer(m.words, m);
				break;
			}
		}
	}

	if (m.count == 0) return;					// no bell: miniterm would print it as a symbol
	const bool progress = m.common > m.prefixLen;
	if (progress || m.count == 1) {
		// Replace what was typed, so the candidate's spelling wins ("b" -> "BLE")
		erase(ws, cur);
		insert(m.kept[0], m.common);
	}
	if (m.count == 1) {
		if (cur == len || line[cur] != ' ') insert(" ", 1);
		else {cur++; redraw = true;}
		return;
	}
	if (progress) return;						// the next Tab lists

	// Nothing to add: list the candidates in columns below the prompt
	uint8_t colW = 0;
	const uint8_t kept = m.count < Matches::MAX_KEPT ? m.count : Matches::MAX_KEPT;
	for (uint8_t i = 0; i < kept; i++) colW = max<uint8_t>(colW, strnlen(m.kept[i], WIDTH - 2) + 2);
	const uint8_t perRow = max(1, (WIDTH - 1) / colW);
	refresh();									// keys of this batch may not be on screen yet
	newline();
	for (uint8_t i = 0; i < kept; i++) {
		Serial.printf("%-*s", colW, m.kept[i]);
		if ((i + 1) % perRow == 0 || i + 1 == kept) Serial.print("\r\n");
	}
	if (m.count > kept) Serial.printf("... %u more\r\n", m.count - kept);
	redraw = true;
}

// ----- Screen -----

void SerialConsole::showPrompt() {
	shown = true;
	refresh();
}

// Brings the screen up to date after a batch of keys: only the new characters if all that
// happened was typing at the end of a line that fits, the whole line otherwise.
void SerialConsole::draw() {
	if (!shown) return;
	if (redraw || scroll > 0 || len >= VISIBLE || len < drawnLen || cur != len) {
		refresh();
	} else if (len > drawnLen) {
		Serial.write(reinterpret_cast<const uint8_t*>(line + drawnLen), len - drawnLen);
		drawnLen = drawnWidth = len;
	}
}

// Redraws prompt and line with nothing but CR, BS and spaces: miniterm's and "pio device
// monitor"'s default filter shows ESC and the other control characters as symbols, and would
// turn ANSI sequences into garbage.
void SerialConsole::refresh() {
	if (cur < scroll) scroll = cur;
	if (cur > scroll + VISIBLE - 1) scroll = cur - VISIBLE + 1;
	if (len <= VISIBLE) scroll = 0;
	const uint8_t n = min<uint8_t>(len - scroll, VISIBLE);
	const uint8_t pad = drawnWidth > n ? drawnWidth - n : 0;		// rest of the longer old line

	Serial.write('\r');
	Serial.write(reinterpret_cast<const uint8_t*>(PROMPT), PROMPT_LEN);
	Serial.write(reinterpret_cast<const uint8_t*>(line + scroll), n);
	repeat(' ', pad);
	repeat('\b', pad + scroll + n - cur);
	redraw = false;
	drawnLen = len;
	drawnWidth = n;
}

void SerialConsole::newline() {
	Serial.print("\r\n");
	drawnWidth = 0;
}

// No buffer on the stack: refresh() also runs in the tasks that log, some with tight stacks.
void SerialConsole::repeat(char c, uint8_t n) {
	static const char spaces[] = "                                                                                ";
	static const char backs[] = "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
	                            "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
	static_assert(sizeof(spaces) - 1 >= WIDTH && sizeof(backs) - 1 >= WIDTH, "repeat() tables shorter than a line");
	const char* src = c == ' ' ? spaces : backs;
	if (n > WIDTH) n = WIDTH;
	if (n) Serial.write(reinterpret_cast<const uint8_t*>(src), n);
}
