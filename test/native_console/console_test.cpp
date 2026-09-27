/*
 * Host test for src/SerialConsole.{h,cpp} -- no hardware, no PlatformIO. Keys go in through
 * a mock Serial, the output runs through a small VT100 emulator, and the checks look at the
 * resulting screen. Build and run from the repository root:
 *
 *   g++ -std=c++17 -O2 -Wall -Itest/native_console/mock -Isrc src/SerialConsole.cpp test/native_console/console_test.cpp -o /tmp/console_test && /tmp/console_test
 *
 * Exit code 0 = all checks passed.
 */

#include <SerialConsole.h>
#include <Singletons.h>

#include <cstdio>
#include <string>
#include <vector>

MockSerial Serial;
uint32_t mockMillis = 1000;
SimpleCLI cli;
SerialConsole console;
BCLogger bclog;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// A terminal behind miniterm's default filter: CR, LF, BS and printable characters. Anything
// else -- ESC, bell -- the filter would show as a symbol, so it is flagged, as is output that
// reaches the last column (the console keeps it free).
struct Screen {
	static constexpr int COLS = 80;
	std::vector<std::string> rows{1};
	int row = 0, col = 0;
	bool lastColumn = false;
	std::string unknown;

	void feed(const std::string& s) {
		for (size_t i = 0; i < s.size(); i++) {
			const char c = s[i];
			if (c == '\r') {col = 0;}
			else if (c == '\n') {row++; if (row >= (int)rows.size()) rows.resize(row + 1);}
			else if (c == '\b') {if (col > 0) col--;}
			else if (c >= 0x20 && c < 0x7f) {
				if (col >= COLS - 1) lastColumn = true;
				if ((int)rows[row].size() <= col) rows[row].resize(col + 1, ' ');
				rows[row][col++] = c;
			} else {
				char b[8];
				snprintf(b, sizeof(b), "<%02x>", (unsigned char)c);
				unknown += b;
			}
		}
	}
	const std::string& last() const {return rows[row];}
	std::string lastTrimmed() const {std::string s = rows[row]; while (!s.empty() && s.back() == ' ') s.pop_back(); return s;}
};

static Screen scr;

static std::string trimmed(std::string s) {
	while (!s.empty() && s.back() == ' ') s.pop_back();
	return s;
}

static void flushOut() {
	scr.feed(Serial.out);
	Serial.out.clear();
}

// Keys arrive as one batch, like a paste or a fast escape sequence
static void keys(const std::string& k) {
	Serial.in += k;
	console.poll();
	flushOut();
}

// Every key as its own batch
static void type(const std::string& k) {
	for (char c : k) keys(std::string(1, c));
}

static void pollAll() {
	while (Serial.available()) {console.poll(); flushOut();}
}

// Clears the line (Ctrl-E, Ctrl-U) and forgets what was parsed
static void reset() {
	keys("\x05\x15");
	cli.parsed.clear();
}

// Trailing spaces can't be seen on the screen -- the cursor position shows them
#define EXPECT_LINE(text, cursor) do { \
	CHECK(scr.lastTrimmed() == trimmed(std::string("bc> ") + (text)), "line \"%s\", expected \"bc> %s\"", scr.lastTrimmed().c_str(), text); \
	CHECK(scr.col == 4 + (cursor), "cursor %d, expected %d", scr.col - 4, (int)(cursor)); } while (0)

static std::vector<std::string> ran;

int main() {
	console.setup();
	console.addCmd("ping", [](cmd*) {ran.push_back("ping"); Serial.println("Pong!");});
	console.addCmd("mem", [](cmd*) {ran.push_back("mem");});
	console.addCmd("showbat", [](cmd*) {ran.push_back("showbat");});
	console.addCmd("showloglevel", [](cmd*) {ran.push_back("showloglevel");});
	console.addCmd("loglevel", [](cmd*) {ran.push_back("loglevel");}, [](uint8_t pos, SerialConsole::Matches& m) {
		static const char* tags[] = {"RAW", "FL", "BLE", "STAT", "WIFI"};
		static const char* levels[] = {"DEBUG", "INFO", "WARN", "ERROR"};
		if (pos == 1) for (auto t : tags) m.add(t);
		else if (pos == 2) for (auto l : levels) m.add(l);
		else {m.add("-serial"); m.add("-file");}
	});
	console.addCmd("rq", [](cmd*) {ran.push_back("rq");}, [](uint8_t pos, SerialConsole::Matches& m) {
		if (pos == 1) for (const char* a : {"status", "interval", "raw", "ref"}) m.add(a);
		else if (pos == 2 && m.word(1).equalsIgnoreCase("ref")) {m.add("start"); m.add("stop");}
	});

	printf("prompt\n");
	console.poll();
	flushOut();
	EXPECT_LINE("", 0);

	printf("typing and Enter (CR)\n");
	type("ping\r");
	CHECK(cli.parsed.size() == 1 && cli.parsed[0] == "ping", "parsed %zu", cli.parsed.size());
	CHECK(trimmed(scr.rows[scr.row - 2]) == "bc> ping", "echo row \"%s\"", scr.rows[scr.row - 2].c_str());
	CHECK(trimmed(scr.rows[scr.row - 1]) == "Pong!", "output row \"%s\"", scr.rows[scr.row - 1].c_str());
	EXPECT_LINE("", 0);

	printf("CRLF is one Enter, LF alone works, empty lines aren't parsed\n");
	cli.parsed.clear();
	type("mem\r\n");
	type("mem\n");
	type("\r\n\r  \r");
	CHECK(cli.parsed.size() == 2, "parsed %zu lines", cli.parsed.size());

	printf("pasted lines run one per poll, in order\n");
	cli.parsed.clear();
	Serial.in += "ping\r\nmem\r\n";
	pollAll();
	CHECK(cli.parsed.size() == 2 && cli.parsed[0] == "ping" && cli.parsed[1] == "mem", "parsed %zu", cli.parsed.size());
	EXPECT_LINE("", 0);

	printf("cursor movement and insert\n");
	reset();
	type("png");
	keys("\x1b[D");
	keys("\x1b[D");
	type("i");
	EXPECT_LINE("ping", 2);
	keys("\x1b[F");
	EXPECT_LINE("ping", 4);
	keys("\x1b[H");
	EXPECT_LINE("ping", 0);
	keys("\x1bOF");									// SS3 End (xterm application mode)
	EXPECT_LINE("ping", 4);
	keys("\x1b[1~");								// Home, VT220 style
	EXPECT_LINE("ping", 0);
	keys("\x1b[4~");
	EXPECT_LINE("ping", 4);
	keys("\x01");									// Ctrl-A
	keys("\x06\x06");								// Ctrl-F
	EXPECT_LINE("ping", 2);
	keys("\x02");									// Ctrl-B
	EXPECT_LINE("ping", 1);
	keys("\r");
	CHECK(cli.parsed.size() == 1 && cli.parsed[0] == "ping", "Enter with cursor inside the line");

	printf("deleting\n");
	reset();
	type("pingg");
	keys("\x7f");									// DEL = Backspace
	EXPECT_LINE("ping", 4);
	keys("\x08");									// BS (Windows miniterm)
	EXPECT_LINE("pin", 3);
	keys("\x1b[H\x1b[3~");							// Delete at start
	EXPECT_LINE("in", 0);
	keys("\x04");									// Ctrl-D
	EXPECT_LINE("n", 0);
	reset();
	type("loglevel BLE DEBUG");
	keys("\x17");									// Ctrl-W
	EXPECT_LINE("loglevel BLE ", 13);
	keys("\x17");
	EXPECT_LINE("loglevel ", 9);
	keys("\x1b[D\x1b[D\x1b[D");
	keys("\x0b");									// Ctrl-K
	EXPECT_LINE("loglev", 6);
	keys("\x1b[D");
	keys("\x15");									// Ctrl-U
	EXPECT_LINE("v", 0);

	printf("word movement\n");
	reset();
	type("loglevel BLE DEBUG");
	keys("\x1b[1;5D");								// Ctrl-Left
	EXPECT_LINE("loglevel BLE DEBUG", 13);
	keys("\x1b" "b");								// Alt-B
	EXPECT_LINE("loglevel BLE DEBUG", 9);
	keys("\x1b[1;5C");								// Ctrl-Right
	EXPECT_LINE("loglevel BLE DEBUG", 12);
	keys("\x1b" "f");								// Alt-F
	EXPECT_LINE("loglevel BLE DEBUG", 18);

	printf("escape sequences split across polls, unknown ones, lone ESC\n");
	reset();
	type("ab");
	type("\x1b[D");									// one byte per poll
	EXPECT_LINE("ab", 1);
	keys("\x1b[5~\x1b[2~\x1b[15~");					// PgUp, Insert, F5: ignored
	EXPECT_LINE("ab", 1);
	keys("\x1b");
	mockMillis += 500;								// ESC key alone, next key much later
	type("x");
	EXPECT_LINE("axb", 2);

	printf("Ctrl-C discards the line\n");
	reset();
	type("rq status");
	keys("\x03");
	CHECK(trimmed(scr.rows[scr.row - 1]) == "bc> rq status^C", "row \"%s\"", scr.rows[scr.row - 1].c_str());
	EXPECT_LINE("", 0);
	CHECK(cli.parsed.empty(), "nothing parsed");

	printf("history\n");
	reset();
	type("ping\r");
	type("mem\r");
	type("mem\r");									// duplicate of the newest: kept once
	type("sho");
	keys("\x1b[A");
	EXPECT_LINE("mem", 3);
	keys("\x1b[A");
	EXPECT_LINE("ping", 4);
	keys("\x10");									// Ctrl-P
	keys("\x0e");									// Ctrl-N
	keys("\x1b[B");
	EXPECT_LINE("mem", 3);
	keys("\x1b[B");
	EXPECT_LINE("sho", 3);							// the edited line comes back
	keys("\x1b[B");
	EXPECT_LINE("sho", 3);
	keys("\x1b[A");
	type("x\r");
	CHECK(cli.parsed.back() == "memx", "recalled and edited: %s", cli.parsed.back().c_str());

	printf("Tab: command names\n");
	reset();
	type("pi\t");
	EXPECT_LINE("ping ", 5);
	reset();
	type("sh\t");
	EXPECT_LINE("show", 4);							// common prefix, no list yet
	int rowBefore = scr.row;
	type("\t");										// nothing to add: list
	CHECK(scr.row == rowBefore + 2, "list takes a row (%d -> %d)", rowBefore, scr.row);
	CHECK(scr.rows[scr.row - 1].find("showbat") != std::string::npos && scr.rows[scr.row - 1].find("showloglevel") != std::string::npos,
	      "list row \"%s\"", scr.rows[scr.row - 1].c_str());
	EXPECT_LINE("show", 4);
	type("b\t");
	EXPECT_LINE("showbat ", 8);
	reset();
	type("x\t");
	EXPECT_LINE("x", 1);							// no match: bell only
	reset();
	type("\t");										// empty line: all commands
	CHECK((scr.rows[scr.row - 2] + scr.rows[scr.row - 1]).find("help") != std::string::npos, "all commands listed");

	printf("Tab: arguments\n");
	reset();
	type("loglevel b\t");
	EXPECT_LINE("loglevel BLE ", 13);
	type("d\t");
	EXPECT_LINE("loglevel BLE DEBUG ", 19);
	type("-s\t");
	EXPECT_LINE("loglevel BLE DEBUG -serial ", 27);
	reset();
	type("LOGL\t");									// case-insensitive, candidate's spelling wins
	EXPECT_LINE("loglevel ", 9);
	reset();
	type("rq ref s\t");
	EXPECT_LINE("rq ref st", 9);
	type("o\t");
	EXPECT_LINE("rq ref stop ", 12);
	reset();
	type("rq  sta\t");								// double space between words
	EXPECT_LINE("rq  status ", 11);
	reset();
	type("help sh\t");
	EXPECT_LINE("help show", 9);
	reset();
	type("mem x\t");								// command without completer
	EXPECT_LINE("mem x", 5);
	reset();
	type("rq ref");
	keys("\x1b[D\x1b[D\x1b[D\x1b[D");				// cursor after "rq": complete in the middle
	type("\t");
	EXPECT_LINE("rq ref", 3);
	reset();
	type("pi xyz");
	keys("\x1b[H\x1b[C\x1b[C\t");
	EXPECT_LINE("ping xyz", 5);						// no second space before an existing one

	printf("help\n");
	reset();
	type("help\r");
	{
		bool found = false;
		for (auto& r : scr.rows) if (r.rfind("loglevel", 0) == 0) found = true;
		CHECK(found, "help lists loglevel");
	}
	type("help rq\r");
	CHECK(trimmed(scr.rows[scr.row - 1]) == "rq", "help rq: \"%s\"", scr.rows[scr.row - 1].c_str());

	printf("long lines scroll sideways, never touching the last column\n");
	reset();
	std::string longLine;
	for (int i = 0; i < 100; i++) longLine += char('a' + i % 26);
	scr.lastColumn = false;							// help's long description doesn't count
	type(longLine);
	CHECK(!scr.lastColumn, "last column written");
	CHECK(scr.lastTrimmed().size() <= 79, "screen line %zu wide", scr.lastTrimmed().size());
	CHECK(scr.lastTrimmed().substr(scr.lastTrimmed().size() - 10) == longLine.substr(90), "tail visible");
	keys("\x1b[H");
	CHECK(scr.lastTrimmed().substr(4, 10) == longLine.substr(0, 10), "head visible after Home");
	CHECK(scr.col == 4, "cursor at start: %d", scr.col);
	keys("\x1b[F");
	CHECK(scr.col <= 78, "cursor inside the screen: %d", scr.col);
	keys("\r");
	CHECK(cli.parsed.back() == longLine, "long line parsed intact");
	CHECK(!scr.lastColumn, "last column written");

	printf("line length limit\n");
	reset();
	type(std::string(130, 'x'));
	keys("\r");
	CHECK(cli.parsed.back().size() == 120, "line cut to %zu", cli.parsed.back().size());

	printf("log output from another task lands above the prompt\n");
	reset();
	type("loglevel BL");
	keys("\x1b[D");
	{
		SerialConsole::Output out(console);
		Serial.println("12:00:00: some log line");
	}
	flushOut();
	CHECK(trimmed(scr.rows[scr.row - 1]) == "12:00:00: some log line", "log row \"%s\"", scr.rows[scr.row - 1].c_str());
	EXPECT_LINE("loglevel BL", 10);
	type("x");
	EXPECT_LINE("loglevel BxL", 11);

	printf("log output while a command runs is left alone\n");
	reset();
	console.addCmd("logs", [](cmd*) {
		SerialConsole::Output out(console);
		Serial.println("log from inside a command");
	});
	type("logs\r");
	CHECK(trimmed(scr.rows[scr.row - 1]) == "log from inside a command", "row \"%s\"", scr.rows[scr.row - 1].c_str());
	CHECK(trimmed(scr.rows[scr.row - 2]) == "bc> logs", "row \"%s\"", scr.rows[scr.row - 2].c_str());
	EXPECT_LINE("", 0);

	printf("Ctrl-L redraws on a fresh line\n");
	type("pi");
	rowBefore = scr.row;
	keys("\x0c");
	CHECK(scr.row == rowBefore + 1, "new row");
	EXPECT_LINE("pi", 2);

	printf("a shorter line blanks out the rest of the longer one\n");
	reset();
	type("rq status");
	keys("\x1b[H\x0b");								// Home, Ctrl-K
	EXPECT_LINE("", 0);
	CHECK(scr.last() == std::string(13, ' ').replace(0, 4, "bc> "), "row \"%s\"", scr.last().c_str());
	type("mem");
	keys("\x1b[A");									// history: longer line
	keys("\x1b[B");
	EXPECT_LINE("mem", 3);

	printf("non-ASCII and other control characters are dropped\n");
	reset();
	type("a\xc3\xa4\x07\x1f" "b");
	EXPECT_LINE("ab", 2);

	CHECK(scr.unknown.empty(), "unknown terminal output: %s", scr.unknown.c_str());

	printf(failures ? "%d FAILED\n" : "all passed\n", failures);
	return failures ? 1 : 0;
}
