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
		std::filesystem::path xfrPath;
		std::filesystem::path xfrTargetPath;  // for symlinks only
		FileRecord::Type xfrType;

		// Stats
		uint64_t deleted;
		uint64_t receivedFiles;
		uint64_t receivedSymlinks;
		uint64_t receivedDirs;
	};

public:
	SyncServerProcess(
		const std::string &host, const std::string &port,
		const std::filesystem::path &root,
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
	void receiveSymlink(State &st);
	void removeFile(const std::filesystem::path &path);

	std::string host, port, instanceId;
	std::filesystem::path root;
	Index *index;
};

#endif
