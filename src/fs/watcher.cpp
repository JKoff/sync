#include "watcher.h"

#include <iostream>
#include <sstream>
#include <string>

#include "scanner.h"
#include "../util/log.h"

using namespace std;

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>

void eventCallback(
    ConstFSEventStreamRef streamRef,
    void *clientCallBackInfo,
    size_t numEvents,
    void *eventPaths,
    const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId eventIds[])
{
    Watcher *watcher = reinterpret_cast<Watcher*>(clientCallBackInfo);
    char **paths = reinterpret_cast<char **>(eventPaths);
    for (size_t i = 0; i < numEvents; i++) {
        watcher->onEvent(std::filesystem::path(paths[i], paths[i] + strlen(paths[i])));
    }
}

Watcher::Watcher(const std::filesystem::path &root, std::function<void (const FileRecord &rec)> callback)
    : callback(callback) {
    FSEventStreamContext context = {0, this, NULL, NULL, NULL};
    CFStringRef mypath = CFStringCreateWithCString(NULL, root.string().c_str(), kCFStringEncodingUTF8);
    CFArrayRef pathsToWatch = CFArrayCreate(NULL, (const void **)&mypath, 1, NULL);
    FSEventStreamRef stream = FSEventStreamCreate(NULL,
                                                  &eventCallback,
                                                  &context,
                                                  pathsToWatch,
                                                  kFSEventStreamEventIdSinceNow,
                                                  0.1,
                                                  kFSEventStreamCreateFlagFileEvents);

    if (stream == NULL) {
        throw std::runtime_error("Failed to create FSEventStream");
    }

    dispatch_queue_t queue = dispatch_queue_create("ca.jkoff.sync.FSEventStream", NULL);
    FSEventStreamSetDispatchQueue(stream, queue);

    FSEventStreamStart(stream);

    this->stream = stream;
    this->queue = queue;
	this->semaphore = dispatch_semaphore_create(0);

    CFRelease(pathsToWatch);
    CFRelease(mypath);
}

Watcher::~Watcher() {
    if (stream != NULL) {
        FSEventStreamStop(stream);
        FSEventStreamInvalidate(stream);
        FSEventStreamRelease(stream);
        stream = NULL;
    }
    if (queue != NULL) {
        dispatch_release(queue);
        queue = NULL;
    }
	if (semaphore != NULL) {
		dispatch_release(semaphore);
		semaphore = NULL;
	}
}

void Watcher::onEvent(const std::filesystem::path& path) {
    scanSingle(path, callback);
}
#else
Watcher::Watcher(const std::filesystem::path &root, std::function<void (const FileRecord &rec)> callback) { }
Watcher::~Watcher() { }
void Watcher::onEvent(const std::filesystem::path& path) { }
#endif
