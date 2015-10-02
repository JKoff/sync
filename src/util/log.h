#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include "any.h"

/////////////
// Logging //
/////////////

#define LOG(x) { \
	std::stringstream _ss; \
	_ss << x; \
	log(_ss, std::cout); \
}

#define ERR(x) { \
	std::stringstream _ss; \
	_ss << x; \
	log(_ss, std::cerr); \
}

// Tags current thread for logging purposes.
void logTag(std::string tag);

void log(const std::stringstream &ss, std::ostream &stream);

void logbuf(const void *buf, size_t len, std::ostream &stream);


////////////////
// StatusLine //
////////////////

#define STATUS(status, x) { \
	std::stringstream _ss; \
	_ss << x; \
	status.update(_ss); \
}

#define STATUSVAR(status, x, y) { \
	std::stringstream _ss; \
	_ss << y; \
	status.set(x, _ss.str()); \
}

#define STATUSGLOBAL(x, y) { \
	std::stringstream _ss; \
	_ss << y; \
	StatusLine::Set(x, _ss.str()); \
}

class StatusLine {
public:
	enum class Type {
		STRING,     // string
		INT,        // int64_t
		STRING_FN,  // StringFn
		INT_FN      // IntFn
	};

	struct Variable {
		Type type;
		Any value;
	};

	typedef std::map<std::string, Variable> Env;
	
	typedef std::string String;
	typedef int64_t Int;
	typedef std::function<String (const Env &)> StringFn;
	typedef std::function<Int (const Env &)> IntFn;


	StatusLine() = default;
	StatusLine(const std::string typeStr, const std::string identStr="");
	~StatusLine();
	// Must be called after default constructor is used.
	void init(const std::string typeStr, const std::string identStr);
	void update(const std::stringstream &ss);

	void set(std::string name, String val);
	void set(std::string name, Int val);
	void set(std::string name, StringFn val);
	void set(std::string name, IntFn val);
	void add(std::string name, Int val);

	void print(std::ostream &stream);

	static void Set(std::string name, String val);
	static void Set(std::string name, Int val);
	static void Set(std::string name, StringFn val);
	static void Set(std::string name, IntFn val);
	static void Add(std::string name, Int val);

	static void PrintAll(std::ostream &stream);
	static void ClearAll(std::ostream &stream);
	static void Refresh(std::ostream &stream);
private:
	static std::string VarToString(const Env &env, const Variable &var);

	static bool dirtyFlag;
	static int linesPrinted;
	static int nextHandle;
	static int ncols;
	static std::map<int, StatusLine*> inst;
	static Env gEnv;

	int handle;
	std::thread::id tid;
	std::string typeStr, identStr;
	std::string statusStr;
	Env env;
};


///////////////
// Utilities //
///////////////

// Output timestamp relative to program start to stream.
std::ostream& ts(std::ostream &stream);

#endif
