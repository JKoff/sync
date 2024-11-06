#include "transfer-process.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "../net/persistent-socket.h"
#include "../util/log.h"

using namespace std;

typedef struct {
    PolicyHost host;
    PolicyFile file;
} TransferMsgPayload;


//////////////////////////
// TransferCounterGuard //
//////////////////////////

/**
    RAII-style mechanism for transiently incrementing a TransferCounter.
    Will increment when its lifetime begins and decrement once its lifetime ends.
 */
class TransferCounterGuard {
public:
    TransferCounterGuard() = delete;
    TransferCounterGuard(TransferCounter &parent) : parent(parent) {
        parent.incr();
    }
    ~TransferCounterGuard() {
        parent.decr();
    }
private:
    TransferCounter &parent;
};



////////////////////
// TransferWorker //
////////////////////

class TransferWorker {
public:
    TransferWorker() = default;
    void bind(const string &root, Policy *policy, const PolicyHost &host, TransferCounter &xfrCounter) {
        this->root = root;
        this->policy = policy;
        this->host = host;
        this->th = thread([this, &xfrCounter] () {
            LOG("-- Starting TransferWorker");

            StatusLine status("TransferWorker", this->host.toString());

            function<void (string)> statusFn = [this, &status] (string str) {
                PolicyStats stats = this->policy->stats(this->host);
                STATUS(status, str);
                STATUSVAR(status, "completed", stats.completed);
                STATUSVAR(status, "remaining", stats.remaining);
            };

            PersistentSocket sock([this] () {
                return this->host.connect();
            });

            // Transfer loop
            for (;;) {
                statusFn("Idle");
                PolicyPlan plan = this->policy->pop(this->host);
                TransferCounterGuard xfrCounterGuard(xfrCounter);

                try {
                    statusFn("Transferring");
                    this->transfer(plan, sock, statusFn);
                } catch (const exception &e) {
                    // Assume it was a failure and requeue it.
                    LOG("TransferWorker: " << e.what());
                    statusFn(e.what());
                    this->policy->push(this->host, plan.file);

                    // Just in case error is transient and network-related.
                    // Wouldn't want a tight loop causing more problems.
                    this_thread::sleep_for(chrono::seconds(2));
                }
            }
        });
    }
    void transfer(const PolicyPlan &plan, PersistentSocket &hostSock, std::function<void (string)> statusFn) {
        assert(plan.steps.value == this->host);
        statusFn("Transferring - " + plan.file.path);

        MSG::XfrEstablishReq req;
        req.plan = plan;
        hostSock.send(req);

        if (plan.file.type == FileRecord::Type::FILE) {
            ifstream f(this->root + req.plan.file.path, ifstream::binary);
            if (f.fail()) {
                StatusLine::Add("fileReadErr", 1);
                throw runtime_error("Failed to open file " + req.plan.file.path);
            }

            // If this resize is changed to a reserve, the contents of the buffer are sometimes
            // zero after the read. I don't understand why.
            this->block.data.resize(MSG::XfrBlock::MAX_SIZE);

            do {
                f.read((char*)this->block.data.data(), MSG::XfrBlock::MAX_SIZE);
                if (f.bad()) {
                    StatusLine::Add("fileReadErr", 1);
                    throw runtime_error("File is now in 'bad' state " + req.plan.file.path);
                }
                this->block.data.resize(f.gcount());
                hostSock.send(this->block);
                StatusLine::Add("essentialOut", this->block.data.size());
            } while (f.good());

            // If file is already empty, closing protocol is already satisfied.
            if (this->block.data.size() == MSG::XfrBlock::MAX_SIZE) {
                this->block.data.resize(0);
                hostSock.send(this->block);
            }

            StatusLine::Add("filesOut", 1);
        } else if (plan.file.type == FileRecord::Type::SYMLINK) {
            this->block.data.resize(0);
            hostSock.send(this->block);
            StatusLine::Add("symlinksOut", 1);
        }
    }
private:
    thread th;
    string root;
    Policy *policy;
    PolicyHost host;
    // We reuse a block instead of freeing and reallocing over and over again.
    MSG::XfrBlock block;
};

//////////////
// Dispatch //
//////////////

TransferProcess::TransferProcess(
    const string &root, const PolicyHost &us, Policy &policy, const vector<PolicyHost> &peers
) : root(root), policy(&policy), us(us), peers(peers) {
    this->th = thread([this] () {
        this->main();
    });
}


/////////////////////////////////////////
// Implementation fns (managed thread) //
/////////////////////////////////////////

void TransferProcess::main() {
    // Set up pool of workers
    size_t WORKERS_PER_PEER = 2;
    size_t npeers = this->peers.size();
    vector<TransferWorker> workers(WORKERS_PER_PEER * npeers);
    for (int i=0, n=workers.size(); i < n; i++) {
        workers[i].bind(this->root, this->policy, this->peers[i % npeers], this->xfrCounter);
    }

    for (;;) {
        Message msg = this->consume();
        switch (msg.type) {
        case MT::TRANSFER: {
            TransferMsgPayload payload = msg.payload.cast<TransferMsgPayload>();
            this->performTransfer(payload.host, payload.file);
            break;
        }
        case MT::INFO:
            this->reply(msg.refid, this->performInfo());
            break;
        }
    }
}

void TransferProcess::performTransfer(const PolicyHost &host, const PolicyFile &file) {
    this->policy->push(host, file);
}

MSG::InfoResp TransferProcess::performInfo() {
    MSG::InfoReq msg;
    // this->remote.send(msg);
    // return *this->remote.awaitWithType<MSG::InfoResp>(MSG::Type::INFO_RESP).release();
    cout << "Unimplemented: TransferProcess::performInfo" << endl;
    exit(0);
    // return msg;
}

///////////////////////////////////////
// Interface methods (caller thread) //
///////////////////////////////////////

void TransferProcess::castTransfer(const PolicyHost &host, const PolicyFile &file) {
	Message msg;
	msg.type = MT::TRANSFER;
    TransferMsgPayload payload = { host, file };
    msg.payload = Any(payload);
	this->cast(msg);
}

MSG::InfoResp TransferProcess::callInfo() {
	Message msg;
	msg.type = MT::INFO;
    Any a = this->call(msg, chrono::seconds(5));
    MSG::InfoResp resp = a.cast<MSG::InfoResp>();
    return resp;
}
