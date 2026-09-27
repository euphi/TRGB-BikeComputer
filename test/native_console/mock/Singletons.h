// Host mock of src/Singletons.h for test/native_console
#pragma once
#include <SimpleCLI.h>
#include <SerialConsole.h>

class BCLogger {
public:
	enum LogType {Log_Debug = 0, Log_Info, Log_Warn, Log_Error};
	enum LogTag {TAG_OP = 0, TAG_CLI};
	std::vector<std::string> lines;
	void log(LogType, LogTag, const String& s) {lines.push_back(s.s);}
	void logf(LogType, LogTag, const char* fmt, ...) {lines.push_back(fmt);}
};

extern SimpleCLI cli;
extern SerialConsole console;
extern BCLogger bclog;
