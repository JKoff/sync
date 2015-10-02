#ifndef PROCESS_COMMAND_PROCESS_H
#define PROCESS_COMMAND_PROCESS_H

#include <string>
#include "./sync-client-process.h"

#include "process.h"

enum class CommandProcessMessageType {
};

class CommandProcess : public Process<CommandProcessMessageType> {
public:
	CommandProcess(
		const std::string &instanceId,
		Index &index,
		std::vector<std::unique_ptr<SyncClientProcess>> &syncThreads
	);
private:
	/////////////////////////////////////////
	// Implementation fns (managed thread) //
	/////////////////////////////////////////
	void main();

	std::string instanceId;
	Index *index;
	std::vector<std::unique_ptr<SyncClientProcess>> *syncThreads;
};

#endif
