#ifndef FS_WATCHER_H
#define FS_WATCHER_H

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "scanner.h"

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#endif

// Watcher monitors the file system for changes.
// Only supported on macOS, no-op on other platforms.
class Watcher {
public:
	Watcher() = delete;
	Watcher(const std::filesystem::path &root, std::function<void (const FileRecord &rec)> callback);
	~Watcher();

	void onEvent(const std::filesystem::path& path);
private:

	std::function<void (const FileRecord &rec)> callback;

    std::atomic<bool> stop_requested{false};
    std::unique_ptr<std::thread> watch_thread;

#ifdef __APPLE__
    FSEventStreamRef stream;
    dispatch_queue_t queue;
	dispatch_semaphore_t semaphore = NULL;
#endif
};

#endif
