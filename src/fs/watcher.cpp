#include "watcher.h"

#include <iostream>
#include <sstream>
#include <string>

#include "scanner.h"

using namespace std;

void onEventsWrapper(const vector<event> &events, void *context) {
	Watcher *watcher = static_cast<Watcher*>(context);
	watcher->onEvents(events);
}

Watcher::Watcher(const string &root, std::function<void (const FileRecord &rec)> callback) {
	this->callback = callback;

    fsMonitor = fsw::monitor::create_default_monitor({ root.c_str() }, &onEventsWrapper, this);
    fsMonitor->start();
}

void Watcher::onEvents(const vector<event> &events) {
	for (auto iter=events.begin(); iter != events.end(); iter++) {
		scanSingle(iter->get_path(), this->callback);
	}
}

Watcher::~Watcher() {
    delete fsMonitor;
}
