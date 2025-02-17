#include "log.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits.h>
#include <map>
#include <mutex>
#include <sys/ioctl.h>
#include <stdexcept>
#include <unistd.h>

using namespace std;

static chrono::time_point<chrono::system_clock> start(chrono::system_clock::now());

ostream& ts(ostream &stream) {
    chrono::duration<double> elapsed = chrono::system_clock::now() - start;
    return stream << "[" << setprecision(3) << fixed << elapsed.count() << "s] ";
}

static uint32_t nextThreadId = 1;
static map<thread::id, uint32_t> threadIds;
static map<thread::id, string> threadTags;
static mutex logMutex;

static string defaultStyle = "\033[39m";
static string threadStyles[] = {
	defaultStyle,
	"\033[31m",
	"\033[32m",
	"\033[33m",
	"\033[34m",
	"\033[35m",
	"\033[36m",
	"\033[37m",
	"\033[38m",
};

static string erase = "\033[F";
static string up = "\033[J";

string tagForThreadId(thread::id tid) {
	if (threadIds.find(tid) == threadIds.end()) {
		threadIds[tid] = nextThreadId++;
	}

	string tag = "";
	if (threadTags.find(tid) != threadTags.end()) {
		tag = threadTags[tid] + "-";
	}

	return tag;
}

///////////////////////////////
// StatusLine implementation //
///////////////////////////////

StatusLine::StatusLine(const string typeStr, const string identStr) {
	this->init(typeStr, identStr);
}

StatusLine::~StatusLine() {
	lock_guard<mutex> lock(logMutex);
	inst.erase(this->handle);
	dirtyFlag = true;
}

void StatusLine::init(const string typeStr, const string identStr) {
	lock_guard<mutex> lock(logMutex);
	this->initNoLock(typeStr, identStr);
}

void StatusLine::initNoLock(const string typeStr, const string identStr) {
	this->handle = nextHandle++;
	this->tid = this_thread::get_id();
	this->typeStr = typeStr;
	this->identStr = identStr;
	inst[this->handle] = this;
	dirtyFlag = true;
}

void StatusLine::update(const stringstream &ss) {
	lock_guard<mutex> lock(logMutex);
	stringstream status;
	status << this->typeStr << "[" << this->identStr << "] " << ss.str();
	this->statusStr = status.str();
	dirtyFlag = true;
}

void StatusLine::print(ostream &stream) {
	// Mutex should already be held prior to calling.

	string tag = tagForThreadId(this->tid);

	stringstream varsStream;
	for (const auto &var : this->env) {
		varsStream << var.first << " = " << VarToString(this->env, var.second) << " | ";
	}
	varsStream << " ";

	stringstream leftStream;
	leftStream << "{" << tag << "t" << threadIds[this->tid] << "}  "
		       << this->statusStr;

	string leftStr = leftStream.str();
	string varsStr = varsStream.str();
	int contentSpace = leftStr.size() + varsStr.size();
	int nSpaces = contentSpace > ncols ? 1 : (ncols - contentSpace);
	string spaces(nSpaces, ' ');

	stream << threadStyles[(threadIds[this->tid] - 1) % 9]
	       << leftStr
	       << spaces
	       << varsStr
	       << defaultStyle
	       << endl;
}

void StatusLine::serialize(std::ostream &stream) const {
	// Mutex should already be held prior to calling.

	::serialize(stream, this->typeStr);
	::serialize(stream, this->identStr);

	uint32_t size = this->env.size();
	::serialize(stream, size);
	for (const auto &var : this->env) {
		::serialize(stream, var.first);
		::serialize(stream, VarToString(this->env, var.second));
	}
	::serialize(stream, this->statusStr);
}

void StatusLine::deserialize(std::istream &stream) {
	// Mutex should already be held prior to calling.

	string typeStr, identStr;
	::deserialize(stream, typeStr);
	::deserialize(stream, identStr);
	this->initNoLock(typeStr, identStr);

	uint32_t size;
	::deserialize(stream, size);
	for (int i=0; i < size; i++) {
		string key, val;
		::deserialize(stream, key);
		::deserialize(stream, val);
		this->env[key] = Variable{ Type::STRING, Any(val) };
	}
	::deserialize(stream, this->statusStr);
}

void StatusLine::set(string name, StatusLine::String val) {
	lock_guard<mutex> lock(logMutex);
	this->env[name] = Variable{ Type::STRING, Any(val) };
}

void StatusLine::set(string name, StatusLine::Int val) {
	lock_guard<mutex> lock(logMutex);
	this->env[name] = Variable{ Type::INT, Any(val) };
}

void StatusLine::set(string name, StatusLine::StringFn val) {
	lock_guard<mutex> lock(logMutex);
	this->env[name] = Variable{ Type::STRING_FN, Any(val) };
}

void StatusLine::set(string name, StatusLine::IntFn val) {
	lock_guard<mutex> lock(logMutex);
	this->env[name] = Variable{ Type::INT_FN, Any(val) };
}

void StatusLine::add(string name, StatusLine::Int val) {
	lock_guard<mutex> lock(logMutex);
	
	auto search = this->env.find(name);
	if (search != this->env.end()) {
		Variable &var = search->second;
		Int oldVal = var.value.cast<Int>();
		Int newVal = oldVal + val;
		var.value = Any(newVal);
	} else {
		this->env[name] = { Type::INT, Any(val) };
	}
}

void StatusLine::Set(string name, StatusLine::String val) {
	lock_guard<mutex> lock(logMutex);
	gEnv[name] = Variable{ Type::STRING, Any(val) };
}

void StatusLine::Set(string name, StatusLine::Int val) {
	lock_guard<mutex> lock(logMutex);
	gEnv[name] = Variable{ Type::INT, Any(val) };
}

void StatusLine::Set(string name, StatusLine::StringFn val) {
	lock_guard<mutex> lock(logMutex);
	gEnv[name] = Variable{ Type::STRING_FN, Any(val) };
}

void StatusLine::Set(string name, StatusLine::IntFn val) {
	lock_guard<mutex> lock(logMutex);
	gEnv[name] = Variable{ Type::INT_FN, Any(val) };
}

void StatusLine::Add(string name, StatusLine::Int val) {
	lock_guard<mutex> lock(logMutex);
	
	auto search = gEnv.find(name);
	if (search != gEnv.end()) {
		Variable &var = search->second;
		Int oldVal = var.value.cast<Int>();
		Int newVal = oldVal + val;
		var.value = Any(newVal);
	} else {
		gEnv[name] = { Type::INT, Any(val) };
	}
}

void StatusLine::Log(const string &line) {
	logLines.push_back(line);
	while (logLines.size() > 100) {
		logLines.pop_front();
	}
}

void StatusLine::PrintAll(ostream &stream) {
	// Mutex should already be held prior to calling.

	int terminalWidth = 60;
	struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) {
		terminalWidth = w.ws_col;
    }

	linesPrinted = 0;

	for (const auto &line : logLines) {
		stream << line << endl;
		++linesPrinted;
	}

	stream << string(ncols, '-') << endl;
	++linesPrinted;

	for (pair<int, StatusLine*> pair : inst) {
		pair.second->print(stream);
		++linesPrinted;
	}

	stringstream ss;
	for (const auto &pair : gEnv) {
		if (ss.str().size() > terminalWidth - 16) {
			stream << ss.str() << endl;
			ss.str("");
			ss.clear();
			++linesPrinted;
		}
		ss << pair.first << " = " << VarToString(gEnv, pair.second) << "   ";
	}
	if (ss.str().size() > 0) {
		stream << ss.str() << endl;
		++linesPrinted;
	}

	dirtyFlag = false;
}

void StatusLine::ClearAll(ostream &stream) {
	// Mutex should already be held prior to calling.

	// for (int i=0; i < linesPrinted; i++) {
		// stream << "\033[2K";
	// }
	stream << "\033[1J";
}

void StatusLine::Refresh(ostream &stream) {
	struct winsize ws;
	ioctl(0, TIOCGWINSZ, &ws);
	ncols = ws.ws_col;

	bool isTty = isatty(1);

	if (isTty && dirtyFlag) {
		lock_guard<mutex> lock(logMutex);
		StatusLine::ClearAll(stream);
		StatusLine::PrintAll(stream);
	}
}

void StatusLine::Serialize(ostream &stream) {
	lock_guard<mutex> lock(logMutex);

	uint32_t size;

	// static std::map<uint32_t, StatusLine*> inst;
	size = inst.size();
	::serialize(stream, size);
	for (const auto &i : inst) {
		::serialize(stream, i.second);
	}

	size = gEnv.size();
	::serialize(stream, size);
	for (const auto &var : gEnv) {
		::serialize(stream, var.first);
		::serialize(stream, VarToString(gEnv, var.second));
	}
}

void StatusLine::Deserialize(istream &stream, unique_ptr<StatusLine[]> &statusLines) {
	lock_guard<mutex> lock(logMutex);

	uint32_t size;

	::deserialize(stream, size);
	statusLines = unique_ptr<StatusLine[]>(new StatusLine[size]);
	for (int i=0; i < size; i++) {
		::deserialize(stream, statusLines[i]);
	}

	::deserialize(stream, size);
	for (int i=0; i < size; i++) {
		string key, val;
		::deserialize(stream, key);
		::deserialize(stream, val);
		gEnv[key] = Variable{ Type::STRING, Any(val) };
	}
}

string StatusLine::VarToString(const StatusLine::Env &env, const StatusLine::Variable &var) {
	switch (var.type) {
		case Type::STRING: {
			return var.value.cast<String>();
		} case Type::INT: {
			stringstream ss;
			// This gets us comma-separated thousands in output, at least on my system.
			ss.imbue(locale(""));
			ss << fixed << var.value.cast<Int>();
			return ss.str();
		} case Type::STRING_FN: {
			StringFn fn = var.value.cast<StringFn>();
			return fn(env);
		} case Type::INT_FN: {
			IntFn fn = var.value.cast<IntFn>();

			stringstream ss;
			// This gets us comma-separated thousands in output, at least on my system.
			ss.imbue(locale(""));
			ss << fixed << fn(env);

			return ss.str();
		}
	}
}

bool StatusLine::dirtyFlag = false;
int StatusLine::linesPrinted = 0;
int StatusLine::nextHandle = 0;
int StatusLine::ncols = 0;
map<uint32_t, StatusLine*> StatusLine::inst;
deque<string> StatusLine::logLines;
StatusLine::Env StatusLine::gEnv;


/////////
// Log //
/////////

static bool silent;

void logSilent(bool isSilent) {
	silent = isSilent;
}

void log(const stringstream &ss, ostream &stream, bool isUrgent) {
	if (silent && !isUrgent) {
		return;
	}

	lock_guard<mutex> lock(logMutex);

	bool isTty = isatty(1);

	if (isTty) {
		StatusLine::ClearAll(stream);
	}

	thread::id tid = this_thread::get_id();
	string tag = tagForThreadId(tid);

	stringstream outss;
	outss << (isTty ? threadStyles[(threadIds[tid] - 1) % 9] : "")
	   << ts
	   << "{" << tag << "t" << threadIds[tid] << "}  "
	   << ss.str()
	   << (isTty ? defaultStyle : "");

	if (isTty) {
		StatusLine::Log(outss.str());
	} else {
		stream << outss.str() << endl;
	}

	if (isTty) {
		StatusLine::PrintAll(stream);
	}
}

void logbuf(const void *buf, size_t len, ostream &stream) {
	if (silent) {
		return;
	}

	stringstream ssfmt;
	for (int i=0; i < len; i++) {
		ssfmt << std::setfill('0') << std::setw(2) << std::hex
			  << static_cast<unsigned int>(((const unsigned char*)buf)[i])
			  << L" ";

		if (i % 4 == 3) {
			ssfmt << "| ";
		}

		if (i % 16 == 15) {
			log(ssfmt, stream);
			ssfmt.str("");
			ssfmt.clear();
		}
	}
	log(ssfmt, stream);
}

void logTag(string tag) {
	if (silent) {
		return;
	}
	
	lock_guard<mutex> lock(logMutex);
	threadTags[this_thread::get_id()] = tag;
}

void nested_exception_to_stream(const std::exception &e, std::ostream &stream, int level) {
	// stream << std::string(level, ' ') << e.what() << endl;
	stream << e.what();
	try {
		rethrow_if_nested(e);
	} catch (const exception &nested) {
		stream << " > ";
		nested_exception_to_stream(nested, stream, level + 1);
	}
}
