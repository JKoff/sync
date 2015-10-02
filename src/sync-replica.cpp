#include <functional>
#include <iostream>
#include <signal.h>
#include <thread>

#include "fs/scanner.h"
#include "fs/watcher.h"
#include "fs/util.h"
#include "net/inet-server.h"
#include "process/command-process.h"
#include "process/sync-client-process.h"
#include "process/sync-server-process.h"
#include "process/transfer-process.h"
#include "util/log.h"
#include "index.h"
#include "util.h"

using namespace std;

int main(int argc, char **argv) {
    using namespace placeholders;

    // MSG_NOSIGNAL in send() apparently doesn't entirely prevent SIGPIPE from occurring.
    signal(SIGPIPE, SIG_IGN);

    if (argc < 3) {
        cout << "Usage: " << argv[0] << " instance-id cookie [host] [port]" << endl;
        exit(0);
    }

    const string ROOT = realpath(".");
    const string INSTANCE_ID(argv[1]);
    const string COOKIE(argv[2]);
    const string HOST(argc >= 4 ? argv[3] : "0.0.0.0");
    const string PORT(argc >= 5 ? argv[4] : "7440");

    Socket::CryptoInit(COOKIE);

    StatusLine mainStatusLine("Main");
    STATUS(mainStatusLine, "Starting...");

    thread statusUpdateThread = thread([] () {
        StatusLine statusLine("Status update");
        for (;;) {
            STATUS(statusLine, "Refreshing periodically.");
            StatusLine::Refresh(cout);
            this_thread::sleep_for(chrono::milliseconds(250));
        }
    });

    LOG("Starting server on " << HOST << ":" << PORT);

    LOG("Indexing " << ROOT);
    cout << endl;

    Index index(ROOT);

    vector<regex> excludes;  // empty since not supported/needed by replica
    function<bool (const string &)> filterFn = bind(filterPath, ref(ROOT), ref(excludes), _1);
    function<void (const FileRecord &)> updateFn = [&index, &filterFn] (const FileRecord &rec) {
        if (filterFn(rec.path)) {
            index.update(rec);
        }
    };

    thread fullscanThread([ROOT, &index, &filterFn, &updateFn] () {
        LOG("-- Starting fullscan thread.");
        StatusLine statusLine("Fullscan");
        STATUS(statusLine, "Scanning filesystem...");
        index.rebuildBlock([ROOT, &filterFn, &updateFn] () {
            performFullScan(ROOT, updateFn, filterFn);
        });
    });

    SyncServerProcess syncServer(HOST, PORT, index, INSTANCE_ID);

    vector<unique_ptr<SyncClientProcess>> emptySyncThreads;
    CommandProcess cmdProc(INSTANCE_ID, index, emptySyncThreads);

    STATUS(mainStatusLine, "Waiting for full scan...");
    fullscanThread.join();

    LOG("Initial scan complete.");
    LOG("Index value: " << index.hash());
    cout << endl;
    
    STATUS(mainStatusLine, "Good to go.");
    syncServer.join();
}
