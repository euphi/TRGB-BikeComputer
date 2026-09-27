/*
 * SerialConsole.h
 *
 *  Created on: 27.09.2026
 *
 * Line editor for the serial CLI: a "bc> " prompt with cursor movement, history and Tab
 * completion, for terminals that send every key as it is typed -- pyserial miniterm /
 * "pio device monitor", picocom, screen, PuTTY. Parsing and running the commands stays with
 * SimpleCLI (cli).
 *
 * The output uses only CR, backspace and spaces, no ANSI sequences: miniterm's default
 * filter would print ESC as a symbol. Keys arrive as the usual VT100/xterm sequences.
 * Lines longer than the assumed 80 columns scroll sideways.
 *
 * Keys: Left/Right, Home/End, Ctrl-A/E/B/F, Alt-B/F and Ctrl-Left/Right (word), Backspace,
 * Del, Ctrl-D (delete at cursor), Ctrl-K/U (kill to end/start), Ctrl-W (kill word),
 * Up/Down and Ctrl-P/N (history), Tab (complete; again to list), Ctrl-C (discard line),
 * Ctrl-L (redraw). Enter may arrive as CR, LF or CRLF.
 *
 * Log output from other tasks must go through Output (BCLogger does): it erases the input
 * line, lets the log line through and redraws the prompt with the half-typed command below it.
 */

#pragma once

#include <Arduino.h>
#include <SimpleCLI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class SerialConsole {
public:
	// Collects the completion candidates for the word at the cursor. add() drops candidates
	// that don't start with what is typed (case-insensitive, like SimpleCLI). The candidate
	// strings must outlive the Tab key press -- string literals and static tables.
	class Matches {
	public:
		void add(const char* candidate);
		// Word i of the line before the word being completed (0 = command name), "" if absent
		String word(uint8_t i) const;
	private:
		friend class SerialConsole;
		static constexpr uint8_t MAX_WORDS = 8;
		static constexpr uint8_t MAX_KEPT = 48;			// listed on screen; more are only counted
		const char* line = nullptr;
		uint8_t start[MAX_WORDS] = {};
		uint8_t len[MAX_WORDS] = {};
		uint8_t words = 0;
		const char* prefix = nullptr;
		uint8_t prefixLen = 0;
		const char* kept[MAX_KEPT] = {};
		uint8_t count = 0;
		uint8_t common = 0;								// length of the prefix all candidates share
	};
	// Offers the candidates for word number pos of the command's line (1 = first argument).
	typedef void (*Completer)(uint8_t pos, Matches& m);

	// After trgb.init(): the history goes to PSRAM (doc/PITFALLS.md). Commands may be
	// registered before.
	void setup();
	// Registers the command with SimpleCLI and for Tab completion. Commands added with
	// cli.addCmd() directly work, but can't be completed.
	Command addCmd(const char* name, void (*callback)(cmd* c), Completer completer = nullptr);
	// From loop(): reads the keys, runs a completed command line.
	void poll();

	// Wrap any output to Serial from other tasks in this; waits up to 100 ms for the console.
	class Output {
	public:
		explicit Output(SerialConsole& c);
		~Output();
		Output(const Output&) = delete;
		Output& operator=(const Output&) = delete;
	private:
		SerialConsole& con;
		bool locked;
	};

private:
	static constexpr uint8_t LINE_LEN = 120;
	static constexpr uint8_t WIDTH = 80;				// assumed terminal width; longer lines scroll sideways
	static constexpr uint8_t HIST_LEN = 16;
	static constexpr uint8_t MAX_CMDS = 24;
	static constexpr const char* PROMPT = "bc> ";
	static constexpr uint8_t PROMPT_LEN = 4;
	// Line chars shown after the prompt. The last column stays empty: a character there makes
	// some terminals wrap on the next output, and "\r" then returns to the wrong row.
	static constexpr uint8_t VISIBLE = WIDTH - 1 - PROMPT_LEN;

	struct Entry {
		const char* name;
		Completer completer;
	};
	Entry cmds[MAX_CMDS] = {};
	uint8_t nCmds = 0;

	SemaphoreHandle_t mutex = nullptr;
	bool lock(TickType_t wait);
	void unlock();

	char line[LINE_LEN + 1] = {};
	uint8_t len = 0;
	uint8_t cur = 0;
	uint8_t scroll = 0;			// first line index shown on screen
	bool started = false;		// the first prompt has been printed
	bool shown = false;			// the prompt is the last line on the screen (not while a command runs)
	bool redraw = false;		// screen differs from line/cur by more than an append
	uint8_t drawnLen = 0;		// chars of line already on screen when !redraw
	uint8_t drawnWidth = 0;		// line chars on the screen row after the prompt (to blank out)

	enum EscState : uint8_t {ESC_NONE, ESC_START, ESC_CSI, ESC_SS3};
	EscState esc = ESC_NONE;
	uint16_t escParam[2] = {};
	uint8_t escParams = 0;
	uint32_t escTime = 0;
	bool lastCR = false;

	// Ring buffer in PSRAM, HIST_LEN entries plus one slot for the line being edited while
	// browsing. nullptr if the allocation failed -- then there is no history.
	char (*hist)[LINE_LEN + 1] = nullptr;
	uint8_t histCount = 0;
	uint8_t histNext = 0;		// slot the next entry goes to
	int8_t histPos = -1;		// entry shown while browsing, 0 = newest; -1 = the edited line
	char* histEntry(uint8_t age) {return hist[(histNext + HIST_LEN - 1 - age) % HIST_LEN];}
	void histAdd();
	void histMove(int8_t dir);

	// All of these run with the mutex held.
	bool feed(char c, char* cmdOut);
	bool feedEscape(char c);
	void insert(const char* s, uint8_t n);
	void erase(uint8_t from, uint8_t to);
	void setLine(const char* s);
	uint8_t wordLeft() const;
	uint8_t wordRight() const;
	void complete();
	void draw();
	void refresh();
	void hide();
	void newline();
	static void repeat(char c, uint8_t n);
	void showPrompt();

	void run(const char* cmdLine);
	void printHelp(const String& name);
	static void completeCommandName(uint8_t pos, Matches& m);
};
