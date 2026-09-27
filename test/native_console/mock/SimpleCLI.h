// Host mock of SimpleCLI: commands with positional arguments; parse() splits on spaces,
// records the line and calls the callback.
#pragma once
#include <Arduino.h>
#include <string>
#include <vector>

struct cmd {
	std::string name, description;
	std::vector<std::string> argNames, argDefaults, argValues;
	void (*callback)(cmd*) = nullptr;
};

class Argument {
public:
	explicit Argument(const std::string& v = "") : v(v) {}
	String getValue() const {return String(v);}
private:
	std::string v;
};

class Command {
public:
	Command(cmd* p = nullptr) : p(p) {}
	Argument addPositionalArgument(const char* name, const char* def = "") {
		p->argNames.push_back(name);
		p->argDefaults.push_back(def);
		return Argument();
	}
	void setDescription(const char* d) {p->description = d;}
	bool hasDescription() const {return !p->description.empty();}
	String getDescription() const {return String(p->description);}
	Argument getArgument(const char* name) const {
		for (size_t i = 0; i < p->argNames.size(); i++) {
			if (p->argNames[i] == name) return Argument(i < p->argValues.size() ? p->argValues[i] : p->argDefaults[i]);
		}
		return Argument();
	}
	void toString(String& s, bool) const {
		s += p->name.c_str();
		for (auto& a : p->argNames) {s += " <"; s += a.c_str(); s += ">";}
	}
	cmd* p;
};

class SimpleCLI {
public:
	std::vector<cmd*> cmds;
	std::vector<std::string> parsed;
	Command addCmd(const char* name, void (*cb)(cmd*)) {
		cmd* c = new cmd;
		c->name = name;
		c->callback = cb;
		cmds.push_back(c);
		return Command(c);
	}
	Command getCmd(const char* name) {
		for (cmd* c : cmds) if (strcasecmp(c->name.c_str(), name) == 0) return Command(c);
		return Command();
	}
	void parse(const char* line) {
		parsed.push_back(line);
		std::vector<std::string> w;
		for (const char* p = line; *p;) {
			while (*p == ' ') p++;
			const char* s = p;
			while (*p && *p != ' ') p++;
			if (p > s) w.emplace_back(s, p - s);
		}
		if (w.empty()) return;
		for (cmd* c : cmds) {
			if (strcasecmp(c->name.c_str(), w[0].c_str()) == 0) {
				c->argValues.assign(w.begin() + 1, w.end());
				if (c->callback) c->callback(c);
				return;
			}
		}
	}
};
