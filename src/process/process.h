#ifndef PROCESS_PROCESS_H
#define PROCESS_PROCESS_H

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include "../util/any.h"

class timeout_error : public runtime_error {
public:
    explicit timeout_error(const string &str) : runtime_error(str) { }
    explicit timeout_error(const char *str) : runtime_error(str) { }
};

template <typename MessageType>
class Process {
public:
	typedef MessageType MT;

	typedef struct {
		MT type;
		uint64_t refid;
		Any payload;
	} Message;

	Process() : refid(0), finished(false) {}
	virtual ~Process() {}

	void join() {
		while (!this->th.joinable()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}

		this->th.join();
	}
protected:
	////////////////////////////
	// Used by calling thread //
	////////////////////////////

	// Should be used via methods of subclass, otherwise clients need to worry
	// about MessageType and payload ugliness.
	void cast(Process::Message &msg) {
		std::lock_guard<std::mutex> lock(this->m);
		msg.refid = this->refid++;
		this->messages.push(move(msg));
		this->messageCv.notify_one();
	}

	Any call(Process::Message &msg, chrono::duration<uint64_t> timeout) {
		this->cast(msg);

		std::unique_lock<std::mutex> lock(this->m);
		bool status = this->replyCv.wait_for(lock, timeout, [this, &msg] {
			return this->replies.find(msg.refid) != this->replies.end();
		});

		if (!status) {
			throw timeout_error("Call did not receive a response in time.");
		}

		Any result = move(this->replies.at(msg.refid));
		this->replies.erase(msg.refid);

		return result;
	}

	////////////////////////////
	// Used by managed thread //
	////////////////////////////

	/**
	 * For a managed thread with no internal concurrency, it can be useful to do a peek()
	 * followed by a consume() on success.
	 */
	
	Message peek() {
		std::unique_lock<std::mutex> lock(this->m);
		this->messageCv.wait(lock, [this] { return !this->messages.empty(); });
		return move(this->messages.front());
	}

	Message consume() {
		std::unique_lock<std::mutex> lock(this->m);
		this->messageCv.wait(lock, [this] { return !this->messages.empty(); });
		Message result = move(this->messages.front());
		this->messages.pop();
		return result;
	}

	void reply(uint64_t refid, Any value) {
		std::lock_guard<std::mutex> lock(this->m);
		this->replies[refid] = move(value);
		this->replyCv.notify_all();
	}
	
	std::condition_variable messageCv, replyCv;
	std::queue<Process::Message> messages;
	std::map<uint64_t, Any> replies;
	std::mutex m;
	std::thread th;
	uint64_t refid;
	bool finished;
};

#endif
