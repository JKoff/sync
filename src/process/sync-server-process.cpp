#include "sync-server-process.h"

#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

#include "../fs/types.h"
#include "../util/log.h"

using namespace std;

//////////////
// Dispatch //
//////////////

SyncServerProcess::SyncServerProcess(
    const string &host, const string &port, const string &root, Index &index, const string &instanceId
) {
    this->host = host;
    this->port = port;
    this->root = root;
    this->instanceId = instanceId;
    this->index = &index;
    this->th = thread([this] () {
        StatusLine statusLine("SyncServerProcess");
        STATUS(statusLine, "Good to go.");

        this->main();
    });
}


/////////////////////////////////////////
// Implementation fns (managed thread) //
/////////////////////////////////////////

void SyncServerProcess::main() {
    InetServer srv(this->host, this->port, [this] (Socket &remote) {
        StatusLine statusLine("SyncServerProcess worker");

        bool notDone = true;

        try {
            while (notDone) {
                State st;
                st.mode = ConnType::NEW;
                st.remote = &remote;
                st.deleted = 0;
                st.receivedFiles = 0;
                st.receivedSymlinks = 0;
                st.receivedDirs = 0;
                st.statusFn = [&st, &statusLine] (string str) {
                    string modeStr;
                    switch (st.mode) {
                    case ConnType::NEW:
                        modeStr = "NEW";
                        break;
                    case ConnType::SYNC:
                        modeStr = "SYNC";
                        break;
                    case ConnType::XFR:
                        modeStr = "XFR";
                        break;
                    }
                    // LOG(
                    //     "[del=" << st.deleted <<
                    //     ", filesIn=" << st.receivedFiles <<
                    //     ", dirsIn=" << st.receivedDirs << "]  " <<
                    //     modeStr << " | " <<
                    //     str
                    // );
                    STATUS(
                        statusLine,
                        "[del=" << st.deleted <<
                        ", filesIn=" << st.receivedFiles <<
                        ", dirsIn=" << st.receivedDirs << "]  " <<
                        modeStr << " | " <<
                        str
                    );
                };

                st.statusFn("Idle");

                st.remote->awaitWithHandler([this, &st] (MSG::Type type, MSG::Base *msg) {
                    if (type == MSG::Type::SYNC_ESTABLISH_REQ) {
                        st.mode = ConnType::SYNC;
                        logTag("sync");
                    } else if (type == MSG::Type::XFR_ESTABLISH_REQ) {
                        MSG::XfrEstablishReq *req = dynamic_cast<MSG::XfrEstablishReq*>(msg);

                        st.mode = ConnType::XFR;
                        logTag("xfr");
                        st.xfrPath = root + req->plan.file.path;
                        st.xfrTargetPath = req->plan.file.targetPath;
                        st.xfrType = (FileRecord::Type)(req->plan.file.type);

                        /*assert(req->plan.value == this->host);

                        for (const Tree<PolicyHost> &fwdTree : req->plan.children) {
                            PolicyPlan fwdPlan = { req->plan.file, fwdTree };
                            // TODO: Send
                            ERR("Plan forwarding not implemented yet.");
                            exit(0);
                        }*/
                    } else {
                        throw runtime_error("Expected establish message in SyncServerProcess session.");
                    }
                }, chrono::seconds(3));

                st.statusFn("Established");

                switch (st.mode) {
                case ConnType::SYNC: notDone = this->syncLoop(st); break;
                case ConnType::XFR: notDone = this->xfrLoop(st); break;
                default: throw runtime_error("Connection in bad state.");
                }
            }
        } catch (const exception &e) {
            // Probably a network error. Peer disconnected, etc.
            // For now, not worth mentioning since it happens under normal conditions.
            ERR("SyncServerProcess worker thread exception: " << e.what());
        }
    });
}

bool SyncServerProcess::syncLoop(State &st) {
    st.statusFn("Waiting...");

    bool finished = false;
    while (!finished) {
        st.remote->awaitWithHandler([this, &st, &finished] (MSG::Type type, MSG::Base *msg) {
            if (type == MSG::Type::INFO_REQ) {
                st.statusFn("Got INFO_REQ");

                MSG::InfoResp resp;
                resp.payloads.push_back({
                    this->instanceId,
                    "reachable",
                    this->index->size(),
                    this->index->hash()
                });
                st.remote->send(resp);

                finished = true;
            } else if (type == MSG::Type::DIFF_REQ) {
                st.statusFn("Got DIFF_REQ");

                MSG::DiffReq *req = dynamic_cast<MSG::DiffReq*>(msg);
                MSG::DiffResp resp;
                // LOG("Has payload |queries|=" << req->queries.size() << " and epoch=" << req->epoch);
                for (const auto &query : req->queries) {
                    bool matches = this->index->hash(query.path) == query.hash;
                    // LOG("Checking if '" << query.path << "' matches " << query.hash << ". Answer? " << matches);
                    // LOG("The hash we have for it is " << this->index->hash(query.path));
                    this->index->setEpoch(query.path, req->epoch);
                    this->index->setExpectedHash(query.path, query.hash);
                    if (!matches) {
                        resp.answers.push_back({ query.path });
                    }
                }
                st.remote->send(resp);
            } else if (type == MSG::Type::DIFF_COMMIT) {
                st.statusFn("Got DIFF_COMMIT");

                MSG::DiffCommit *req = dynamic_cast<MSG::DiffCommit*>(msg);
                list<Relpath> deleted = this->index->commit(req->epoch);
                for (auto i : deleted) {
                    Relpath path = root + i;
                    this->removeFile(path);
                    scanSingle(path, [this] (const FileRecord &rec) {
                        this->index->update(rec);
                    });

                    StatusLine::Add("del", 1);
                    ++st.deleted;
                }

                finished = true;
            } else {
                finished = true;
                ERR("Unknown message " << static_cast<int>(type));
                throw runtime_error("Unknown message.");
            }
        }, chrono::seconds(0));
    }

    return false;
}

void SyncServerProcess::receiveFile(State &st) {
    ofstream f(st.xfrPath, ios_base::trunc);
    if (f.fail()) {
        StatusLine::Add("fileWriteErr", 1);
        throw runtime_error("Failed to open file " + st.xfrPath);
    }

    bool done = false;

    while (!done) {
        unique_ptr<MSG::XfrBlock> block =
            st.remote->awaitWithType<MSG::XfrBlock>(MSG::Type::XFR_BLOCK);

        f.write(reinterpret_cast<char*>(block->data.data()), block->data.size());
        if (f.bad()) {
            StatusLine::Add("fileWriteErr", 1);
            throw runtime_error("File is now in 'bad' state " + st.xfrPath);
        }

        if (block->data.size() < MSG::XfrBlock::MAX_SIZE) {
            done = true;
        }
    }
}

void SyncServerProcess::receiveSymlink(State &st) {
    std::filesystem::create_symlink(st.xfrTargetPath, st.xfrPath);
}

void SyncServerProcess::removeFile(const string &path) {
    try {
        File f(path);
        f.remove();
    } catch (does_not_exist_error e) {
        // Good, our job is already done.
    }
}

bool SyncServerProcess::xfrLoop(State &st) {
    switch (st.xfrType) {
    case FileRecord::Type::DIRECTORY:
        // TODO: Setting world-readable isn't necessarily what we need.
        mkdir(st.xfrPath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);

        StatusLine::Add("dirsIn", 1);
        ++st.receivedDirs;
        break;
    case FileRecord::Type::DOES_NOT_EXIST:
        this->removeFile(st.xfrPath);

        StatusLine::Add("del", 1);
        ++st.deleted;
        break;
    case FileRecord::Type::FILE:
        this->receiveFile(st);

        ++st.receivedFiles;
        StatusLine::Add("filesIn", 1);
        break;
    case FileRecord::Type::SYMLINK:
        this->receiveSymlink(st);

        ++st.receivedSymlinks;
        StatusLine::Add("symlinksIn", 1);
        break;
    }

    scanSingle(st.xfrPath, [this] (const FileRecord &rec) {
        this->index->update(rec);
    });

    return true;
}
