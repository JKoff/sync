#include "fanout.h"

#include <stdexcept>
#include "../../util/log.h"

using namespace std;

void FanoutPolicy::push(const PolicyHost &host, const PolicyFile &file) {
	lock_guard<mutex> lock(this->m);

	this->pending[host].push_back(file);
	++this->hostStats[host].remaining;

	// Uncomment for perf timing
	// if (this->hostStats[host].remaining == 1) {
	// 	LOG("Started fanout.");
	// }

	this->cv.notify_one();
}

PolicyPlan FanoutPolicy::pop(const PolicyHost &host) {
	unique_lock<mutex> lock(this->m);
	this->cv.wait(lock, [this,host] { return !this->pending[host].empty(); });

	auto firstItem = this->pending[host].front();

	PolicyPlan plan;
	plan.file = firstItem;
	plan.steps.value = host;

	this->pending[host].pop_front();
	--this->hostStats[host].remaining;
	++this->hostStats[host].completed;

	// Uncomment for perf timing
	// if (this->hostStats[host].remaining == 0) {
	// 	LOG("Completed fanout.");
	// }

	return plan;
}

PolicyStats FanoutPolicy::stats(const PolicyHost &host) {
	lock_guard<mutex> lock(this->m);
	return this->hostStats[host];
}
