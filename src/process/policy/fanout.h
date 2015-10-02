#ifndef PROCESS_POLICY_FANOUT_H
#define PROCESS_POLICY_FANOUT_H

#include "policy.h"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>

class FanoutPolicy : public Policy {
public:
	FanoutPolicy(const PolicyHost &us) : Policy(us) {}
	virtual void push(const PolicyHost &host, const PolicyFile &file);
	virtual PolicyPlan pop(const PolicyHost &host);
	virtual PolicyStats stats(const PolicyHost &host);
private:
	std::condition_variable cv;
	std::map<PolicyHost, std::deque<PolicyFile>> pending;
	std::map<PolicyHost, PolicyStats> hostStats;
	std::mutex m;
};

#endif
