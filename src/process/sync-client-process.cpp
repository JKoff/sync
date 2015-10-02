#include "sync-client-process.h"

#include <functional>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace std;

//////////////
// Dispatch //
//////////////

SyncClientProcess::SyncClientProcess(const PolicyHost &host, Index &index, TransferProcess &transferProc) {
    this->host = host;
    this->index = &index;
    this->transferProc = &transferProc;
    this->th = thread([this] () {
        LOG("-- Starting SyncClientProcess thread for " << this->host);
        this->status.init("SyncClientProcess", this->host.toString());
        for (;;) {
            try {
                this->main();
            } catch (const exception &e) {
                STATUS(this->status, e.what());
                LOG("SyncClientProcess: " << e.what());
                
                // Probably a network error.
                this_thread::sleep_for(chrono::seconds(10));
            }
        }
    });
}


/////////////////////////////////////////
// Implementation fns (managed thread) //
/////////////////////////////////////////

void SyncClientProcess::main() {
    STATUS(this->status, "Connecting");

    this->remote = this->host.connect();

    for (;;) {
        STATUS(this->status, "Establishing session");
        this->remote.send(MSG::SyncEstablishReq());

        STATUS(this->status, "Established");
        Message msg = this->peek();
        switch (msg.type) {
        case MT::FULLSYNC:
            this->performFullsync();
            break;
        case MT::INFO:
            this->reply(msg.refid, this->performInfo());
            break;
        }
        this->consume();
    }
}

void SyncClientProcess::performFullsync() {
    /**
     * An epoch serves as a txnid for a fullsync.
     * Why do we use the index hash? Well, we should always be sending the same DiffReqs
     * to the replica given a index hash. So we don't use a uuid or coordinated serial id.
     * TODO: Review this decision for soundness.
     */
    uint64_t epoch = this->index->hash();
    STATUS(this->status, "Fullsync " << epoch);
    LOG("Started fullsync.");

	this->index->diff([this,epoch] (const deque<string>& seen) {
        // Oracle function
        deque<string> result;

        // Stats for progress indication
        uint64_t queryCtr = 0;
        uint64_t answerCtr = 0;
        function<void (string str)> updateStats = [this, epoch, &queryCtr, &answerCtr] (string str="") {
            STATUS(
                this->status,
                "Fullsync " << epoch << " | " <<
                answerCtr << " out of " << queryCtr << " need update. " <<
                str
            );
        };

        // Let's check in with the remote.
        deque<string> sent(seen);
        MSG::DiffReq req;
        unique_ptr<MSG::DiffResp> resp;
        req.epoch = epoch;

        updateStats("-->");

        while (!sent.empty()) {
            string front = sent.front();
            req.queries.push_back({ front, this->index->hash(front) });
            sent.pop_front();

            if (req.queries.size() == MSG::DiffReq::MAX_RECORDS) {
                queryCtr += req.queries.size();
                StatusLine::Add("client queries", req.queries.size());
                this->remote.send(req);
                
                req.queries.clear();

                updateStats("<--");

                resp = this->remote.awaitWithType<MSG::DiffResp>(MSG::Type::DIFF_RESP);
                for (const auto &answer : resp->answers) {
                    result.push_back(answer.path);
                }
                answerCtr += resp->answers.size();
                StatusLine::Add("client answers", resp->answers.size());

                updateStats("-->");
            }
        }

        if (req.queries.size() > 0) {
            queryCtr += req.queries.size();
            StatusLine::Add("client queries", req.queries.size());
            this->remote.send(req);

            resp = this->remote.awaitWithType<MSG::DiffResp>(MSG::Type::DIFF_RESP);
            for (const auto &answer : resp->answers) {
                result.push_back(answer.path);
            }
            answerCtr += resp->answers.size();
            StatusLine::Add("client answers", resp->answers.size());
            updateStats("<--");
        }

        return result;
    }, [this] (const PolicyFile &file) {
        // Emit function
        this->transferProc->castTransfer(this->host, file);
    });

    MSG::DiffCommit msg;
    msg.epoch = epoch;
    this->remote.send(msg);

    LOG("Finished fullsync.");
}

MSG::InfoResp SyncClientProcess::performInfo() {
    MSG::InfoReq msg;
    this->remote.send(msg);
    return *this->remote.awaitWithType<MSG::InfoResp>(MSG::Type::INFO_RESP).release();
}

///////////////////////////////////////
// Interface methods (caller thread) //
///////////////////////////////////////

void SyncClientProcess::castFullsync() {
	Message msg;
	msg.type = MT::FULLSYNC;
	this->cast(msg);
}

MSG::InfoResp SyncClientProcess::callInfo() {
	Message msg;
	msg.type = MT::INFO;
    Any a = this->call(msg, chrono::seconds(5));
    MSG::InfoResp resp = a.cast<MSG::InfoResp>();
    return resp;
	// return this->call(msg, chrono::seconds(5)).cast<MSG::InfoResp>();
}
