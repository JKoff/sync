#ifndef PROCESS_TRANSFER_PROCESS_H
#define PROCESS_TRANSFER_PROCESS_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include "../index.h"
#include "../util.h"
#include "../net/inet-client.h"
#include "../net/protocol.h"
#include "policy/policy.h"

#include "process.h"

enum class TransferProcessMessageType {
	TRANSFER,
	INFO
};

/**
 * TransferProcess is instructed to send file X to host Y.
 * It acts as a controller + supervisor for policy + pool of transfer workers.
 * 
 * Policy: responsible for any policy around transfers (use N sockets, etc.)
 * TransferWorkerProcess: responsible for actual transfers
 */

class TransferProcess : public Process<TransferProcessMessageType> {
public:
	TransferProcess(
		const std::string &root, const PolicyHost &us, Policy &policy,
		const std::vector<PolicyHost> &peers
	);

	///////////////////////////////////////
	// Interface methods (caller thread) //
	///////////////////////////////////////
	void castTransfer(const PolicyHost &host, const PolicyFile &file);
	MSG::InfoResp callInfo();
private:
	/////////////////////////////////////////
	// Implementation fns (managed thread) //
	/////////////////////////////////////////
	void main();
	void performTransfer(const PolicyHost &host, const PolicyFile &file);
	MSG::InfoResp performInfo();

	std::string root;
	Policy *policy;
	PolicyHost us;
	std::vector<PolicyHost> peers;
};

#endif
