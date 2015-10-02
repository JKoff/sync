#ifndef PROCESS_SYNC_CLIENT_PROCESS_H
#define PROCESS_SYNC_CLIENT_PROCESS_H

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include "../index.h"
#include "../util.h"
#include "../net/inet-client.h"
#include "../net/protocol.h"
#include "../util/log.h"
#include "./transfer-process.h"

#include "process.h"

enum class SyncClientProcessMessageType {
	FULLSYNC,
	INFO
};

class SyncClientProcess : public Process<SyncClientProcessMessageType> {
public:
	SyncClientProcess(const PolicyHost &host, Index &index, TransferProcess &transferProc, bool verbose);

	///////////////////////////////////////
	// Interface methods (caller thread) //
	///////////////////////////////////////
	void castFullsync();
	MSG::InfoResp callInfo();
private:
	/////////////////////////////////////////
	// Implementation fns (managed thread) //
	/////////////////////////////////////////
	void main();
	void performFullsync();
	MSG::InfoResp performInfo();

	PolicyHost host;
	Index *index;
	Socket remote;
	TransferProcess *transferProc;
	StatusLine status;
	bool verbose;
};

#endif
