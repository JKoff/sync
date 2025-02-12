#include "command-process.h"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

#include "../net/unix-server.h"
#include "../util/log.h"

using namespace std;

//////////////
// Dispatch //
//////////////

CommandProcess::CommandProcess(
    const string &instanceId,
    Index &index,
    vector<unique_ptr<SyncClientProcess>> &syncThreads
) {
    this->instanceId = instanceId;
    this->index = &index;
    this->syncThreads = &syncThreads;
    this->th = thread([this] () {
        LOG("-- Starting command thread.");
        StatusLine statusLine("Command");
        STATUS(statusLine, "Good to go.");

        try {
            this->main();
        } catch (const exception &e) {
            // Under no circumstance should a secondary process such as this one
            // interrupt execution of the program.
            ERR("Command thread: " << e.what());
        }
    });
}


/////////////////////////////////////////
// Implementation fns (managed thread) //
/////////////////////////////////////////

void CommandProcess::main() {
    UnixServer srv(this->instanceId, [this] (Socket &remote) {
        try {
            StatusLine statusLine("Command worker");
            STATUS(statusLine, "Awaiting...");

            remote.awaitWithHandler([this, &statusLine, &remote] (MSG::Type type, MSG::Base *msg) {
                // We do not have ownership over msg. It is merely borrowed.

                if (type == MSG::Type::INFO_REQ) {
                    STATUS(statusLine, "Got INFO_REQ");

                    MSG::InfoResp resp;
                    resp.payloads.push_back({
                        this->instanceId, "reachable", this->index->size(), this->index->hash()
                    });

                    for (auto &replica : *(this->syncThreads)) {
                        try {
                            MSG::InfoResp remoteResp = replica->callInfo();
                            for (const auto &payload : remoteResp.payloads) {
                                resp.payloads.push_back(payload);
                            }
                        } catch (timeout_error e) {
                            resp.payloads.push_back({ "", "DOWN", 0, 0 });
                        }
                    }

                    remote.send(resp);
                } else if (type == MSG::Type::FULLSYNC_CMD) {
                    STATUS(statusLine, "Got FULLSYNC_CMD");
                    LOG("Starting fullsync.");

                    for (auto &replica : *(this->syncThreads)) {
                        replica->castFullsync();
                    }
                } else if (type == MSG::Type::FLUSH_CMD) {
                    STATUS(statusLine, "Got FLUSH_CMD");
                    LOG("Flushing.");

                    // TODO
                } else if (type == MSG::Type::INSPECT_REQ) {
                    STATUS(statusLine, "Got INSPECT_CMD");

                    MSG::InspectReq *req = dynamic_cast<MSG::InspectReq*>(msg);

                    MSG::InspectResp resp;
                    resp.path = req->path;
                    resp.hash = this->index->hash(req->path);

                    set<std::filesystem::path> children = this->index->children(std::filesystem::path(req->path));
                    for (const std::filesystem::path &child : children) {
                        resp.children.push_back({ child.string(), this->index->hash(child) });
                    }

                    remote.send(resp);
                } else if (type == MSG::Type::LOG_REQ) {
                    STATUS(statusLine, "Got LOG_REQ");

                    MSG::LogResp resp;
                    remote.send(resp);
                } else {
                    // Ignore.
                    ERR("CommandProcess received bad message. Type: " << (int)msg->type);
                }
            }, chrono::seconds(3));
        } catch (const exception &e) {
            // Under no circumstance should a secondary process such as this one
            // interrupt execution of the program.
            ERR("Command thread worker: " << e.what());
        }
    });
}
