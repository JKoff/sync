#ifndef PROCESS_POLICY_CHAIN_H
#define PROCESS_POLICY_CHAIN_H

#include "policy.h"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>

class ChainPolicy : public Policy {
public:
	ChainPolicy(const PolicyHost &us) : Policy(us) {}
	virtual void push(const PolicyHost &host, const PolicyFile &file);
	virtual PolicyPlan pop(const PolicyHost &host);
	virtual PolicyStats stats(const PolicyHost &host);
private:
	std::condition_variable cv;
	std::map<PolicyFile, std::deque<PolicyHost>> pending;
	std::map<PolicyHost, PolicyStats> hostStats;
	std::mutex m;
};

#endif
