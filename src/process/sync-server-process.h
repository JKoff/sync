#ifndef PROCESS_SYNC_SERVER_PROCESS_H
#define PROCESS_SYNC_SERVER_PROCESS_H

#include <condition_variable>
#include <functional>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include "../index.h"
#include "../util.h"
#include "../fs/scanner.h"
#include "../net/inet-server.h"
#include "../net/protocol.h"
#include "../util/log.h"
#include "./transfer-process.h"

#include "process.h"

enum class SyncServerProcessMessageType {
	FULLSYNC,
	SYNC_ONE,
	INFO
};

class SyncServerProcess : public Process<SyncServerProcessMessageType> {
	enum class ConnType {
		NEW,
	    SYNC,
	    XFR
	};

	// Connection state
	struct State {
		ConnType mode;
		Socket *remote;
		std::function<void (std::string)> statusFn;

		// XFR mode only
		std::string xfrPath;
		FileRecord::Type xfrType;

		// Stats
		uint64_t deleted;
		uint64_t receivedFiles;
		uint64_t receivedDirs;
	};

public:
	SyncServerProcess(
		const std::string &host, const std::string &port,
		const std::string &root,
		Index &index, const std::string &instanceId);
private:
	/////////////////////////////////////////
	// Implementation fns (managed thread) //
	/////////////////////////////////////////
	void main();
	bool syncLoop(State &st);
	bool xfrLoop(State &st);

	// Helpers
	void receiveFile(State &st);
	void removeFile(const std::string &path);

	std::string host, port, root, instanceId;
	Index *index;
};

#endif
