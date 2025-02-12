#include <functional>
#include <iostream>
#include <signal.h>
#include <thread>

#include "fs/scanner.h"
#include "fs/watcher.h"
#include "fs/util.h"
#include "net/inet-server.h"
#include "net/protocol.h"
#include "process/command-process.h"
#include "process/sync-client-process.h"
#include "process/sync-server-process.h"
#include "process/transfer-process.h"
#include "util/log.h"
#include "index.h"
#include "util.h"

using namespace std;

void exitWithUsage(const string &progname) {
    cout << "Usage: " << progname << " instance-id cookie [--bind=<host:port>] [--path=/root/path] [--exclude=<regex>]*" << endl;
    exit(0);
}

int main(int argc, char **argv) {
    using namespace placeholders;

    // MSG_NOSIGNAL in send() apparently doesn't entirely prevent SIGPIPE from occurring.
    signal(SIGPIPE, SIG_IGN);

    if (argc < 3) {
        exitWithUsage(argv[0]);
    }

    Abspath ROOT = std::filesystem::current_path();
    const string INSTANCE_ID(argv[1]);
    const string COOKIE(argv[2]);
    string HOST;
    string PORT;

    vector<wregex> excludes;  // empty since not supported/needed by replica
    for (int i=3; i < argc; i++) {
        string str = argv[i];

        string::size_type eqPos = str.find("=", 0);
        if (eqPos == string::npos) {
            exitWithUsage(argv[0]);
        }

        string name = str.substr(2, eqPos - 2);
        string val = str.substr(eqPos + 1);

        if (name == "bind") {
            string::size_type colPos = val.find(":", 0);
            if (colPos == string::npos) {
                exitWithUsage(argv[0]);
            }
            HOST = val.substr(0, colPos);
            PORT = val.substr(colPos + 1);
        } else if (name == "exclude") {
            wregex r(wstring(val.begin(), val.end()));
            excludes.push_back(r);
        } else if (name == "path") {
            ROOT = val;
        } else {
            exitWithUsage(argv[0]);
        }
    }

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

    LOG("Starting server on " << HOST << ":" << PORT << " with protocol version " << PROTOCOL_VERSION);

    LOG("Indexing " << ROOT);
    cout << endl;

    Index index(ROOT);

    function<bool (const std::filesystem::path &)> filterFn = bind(filterPath, ref(ROOT), ref(excludes), _1);
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

    SyncServerProcess syncServer(HOST, PORT, ROOT, index, INSTANCE_ID);

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
