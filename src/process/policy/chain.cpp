#include "chain.h"

#include <stdexcept>
#include "../../util/log.h"

using namespace std;

void ChainPolicy::push(const PolicyHost &host, const PolicyFile &file) {
	lock_guard<mutex> lock(this->m);

	this->pending[file].push_back(host);
	++this->hostStats[host].remaining;

	// Uncomment for perf timing
	// if (this->hostStats[host].remaining == 1) {
	// 	LOG("Started fanout.");
	// }

	this->cv.notify_one();
}

PolicyPlan ChainPolicy::pop(const PolicyHost &host) {
	unique_lock<mutex> lock(this->m);
	this->cv.wait(lock, [this] { return !this->pending.empty(); });

	auto firstItem = this->pending.begin();

	PolicyPlan plan;
	plan.file = firstItem->first;
	// Given host must be first.
	plan.steps.value = host;

	Tree<PolicyHost> *curNode = &(plan.steps);
	for (const PolicyHost &remoteHost : firstItem->second) {
		// Given host must be first.
		if (remoteHost == host) {
			continue;
		}

		curNode->children.push_back({ remoteHost });
		curNode = &(curNode->children[0]);
	}

	this->pending.erase(firstItem);
	--this->hostStats[host].remaining;
	++this->hostStats[host].completed;

	// Uncomment for perf timing
	// if (this->hostStats[host].remaining == 0) {
		// LOG("Completed fanout.");
	// }

	return plan;
}

PolicyStats ChainPolicy::stats(const PolicyHost &host) {
	lock_guard<mutex> lock(this->m);
	return this->hostStats[host];
}

void ChainPolicy::waitUntilEmpty() {
	unique_lock<mutex> lock(this->m);
	this->empty_cv.wait(lock, [this] {
		int nQueued = 0;
		for (const pair<const PolicyHost, PolicyStats>& p : this->hostStats) {
			const PolicyStats &stats = p.second;
			nQueued += stats.remaining;
		}
		return nQueued == 0;
	});
}
