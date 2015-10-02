#include <chrono>
#include <functional>
#include <iostream>
#include <regex>
#include <signal.h>
#include <thread>

#include "fs/scanner.h"
#include "fs/watcher.h"
#include "fs/util.h"
#include "net/unix-server.h"
#include "net/inet-client.h"
#include "process/command-process.h"
#include "process/sync-client-process.h"
#include "process/transfer-process.h"
#include "process/policy/chain.h"
#include "process/policy/fanout.h"
#include "index.h"
#include "util.h"
#include "util/log.h"

using namespace std;

void exitWithUsage(const string &progname) {
    cout << "Usage: " << progname << " instance-id cookie [--replica=<host:port>]* [--exclude=<regex>]* [--verbose]" << endl;
    exit(0);
}

int main(int argc, char **argv) {
    using namespace placeholders;

    // MSG_NOSIGNAL in send() apparently doesn't entirely prevent SIGPIPE from occurring.
    signal(SIGPIPE, SIG_IGN);

    if (argc < 3) {
        exitWithUsage(argv[0]);
    }

    const Abspath ROOT = realpath(".");
    const string INSTANCE_ID = argv[1];
    const string COOKIE = argv[2];
    bool verbose = false;
    vector<unique_ptr<SyncClientProcess>> syncThreads;
    Index index(ROOT);

    vector<string> replicas;
    vector<regex> excludes;
    for (int i=3; i < argc; i++) {
        string str = argv[i];
        if (str == "--verbose") {
            verbose = true;
            continue;
        }

        string::size_type eqPos = str.find("=", 0);
        if (eqPos == string::npos) {
            exitWithUsage(argv[0]);
        }

        string name = str.substr(2, eqPos - 2);
        string val = str.substr(eqPos + 1);

        if (name == "replica") {
            replicas.push_back(val);
        } else if (name == "exclude") {
            regex r(val);
            excludes.push_back(r);
        }
    }

    Socket::CryptoInit(COOKIE);


    //////////////////
    // Status setup //
    //////////////////

    StatusLine mainStatusLine("Main");
    STATUS(mainStatusLine, "Starting...");

    if (replicas.size() > 0) {
        LOG("Starting with replicas:");
        for (auto replica : replicas) {
            LOG("\t" << replica);
        }
    }
    LOG("");

    LOG("Indexing " << ROOT);
    LOG("");

    thread statusUpdateThread = thread([] () {
        StatusLine statusLine("Status update");
        for (;;) {
            STATUS(statusLine, "Refreshing periodically.");
            
            StatusLine::Int lastOutbound = 0;
            StatusLine::Set("outRate", [&lastOutbound] (const StatusLine::Env &env) -> int64_t {
                auto outbound = env.find("outbound");
                if (outbound != env.end()) {
                    const StatusLine::Variable &var = outbound->second;
                    StatusLine::Int newOutbound = var.value.cast<StatusLine::Int>();
                    lastOutbound = newOutbound;
                    return (newOutbound - lastOutbound) * 4;
                } else {
                    return 0;
                }
            });

            StatusLine::Refresh(cout);
            this_thread::sleep_for(chrono::milliseconds(250));
        }
    });

    thread inputThread = thread([] () {
        StatusLine statusLine("Input");
        int mode = 0;
        for (;;) {
            STATUS(statusLine, "Mode: " << mode);
            getchar();
            mode = (mode + 1) % 2;

            string erase = "\033[F";
            string up = "\033[J";
            cout << up << erase;
            StatusLine::Refresh(cout);
        }
    });


    /////////////////////////
    // Connect to replicas //
    /////////////////////////

    vector<PolicyHost> policyHosts(replicas.size());
    transform(replicas.begin(), replicas.end(), policyHosts.begin(), [] (string replica) {
        vector<string> parts = tokenize(replica, ':');
        string host = parts[0];
        string port = parts[1];
        return PolicyHost(host, port);
    });

    PolicyHost us(INSTANCE_ID);
    // ChainPolicy policy(us);
    FanoutPolicy policy(us);
    TransferProcess transferProc(ROOT, us, policy, policyHosts);

    for (auto policyHost : policyHosts) {
        syncThreads.push_back(unique_ptr<SyncClientProcess>(
            new SyncClientProcess(policyHost, index, transferProc, verbose)));
    }


    /////////////////////////////
    // Get up to speed locally //
    /////////////////////////////

    function<bool (const string &)> filterFn = bind(filterPath, ref(ROOT), ref(excludes), _1);

    function<void (const FileRecord &)> updateFn = [&index, &filterFn] (const FileRecord &rec) {
        if (filterFn(rec.path)) {
            index.update(rec);
        }
    };

    function<void (const FileRecord &)> updateSingleFn =
        [&ROOT, &index, &filterFn, &transferProc, &policyHosts] (const FileRecord &rec) {
            if (filterFn(rec.path)) {
                index.update(rec);

                Relpath path = rec.path.substr(ROOT.length());
                PolicyFile file = { path, rec.type };
                for (auto policyHost : policyHosts) {
                    transferProc.castTransfer(policyHost, file);
                }
            }
        };

    thread watcherThread([ROOT, &updateSingleFn] () {
        LOG("-- Starting watcher thread.");
        StatusLine statusLine("Watcher");
        STATUS(statusLine, "Watching filesystem...");
        Watcher watcher(ROOT, updateSingleFn);
    });

    thread fullscanThread([ROOT, &index, &filterFn, &updateFn] () {
        LOG("-- Starting fullscan thread.");
        StatusLine statusLine("Fullscan");
        STATUS(statusLine, "Scanning filesystem...");
        index.rebuildBlock([ROOT, &filterFn, &updateFn] () {
            performFullScan(ROOT, updateFn, filterFn);
        });
    });

    CommandProcess cmdProc(INSTANCE_ID, index, syncThreads);

    STATUS(mainStatusLine, "Waiting for full scan...");
    fullscanThread.join();

    LOG("Initial scan complete.");
    LOG("Index value: " << index.hash());
    LOG("");


    ///////////////////////////////////
    // Then get replicas up to speed //
    ///////////////////////////////////

    for (auto &replica : syncThreads) {
        replica->castFullsync();
    }

    STATUS(mainStatusLine, "Good to go.");
    watcherThread.join();
}
