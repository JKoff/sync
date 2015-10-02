#ifndef FS_WATCHER_H
#define FS_WATCHER_H

#include <functional>
#include <string>
#include <vector>
#include "fswatch/monitor.h"

#include "scanner.h"

class Watcher {
public:
	Watcher() = delete;
	Watcher(const std::string &root, std::function<void (const FileRecord &rec)> callback);
	~Watcher();

	void onEvents(const std::vector<event> &events);
private:
    fsw::monitor *fsMonitor;
	std::function<void (const FileRecord &rec)> callback;
};

#endif
