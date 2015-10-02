#include <chrono>
#include <functional>
#include <iostream>
#include <signal.h>
#include <string>
#include <thread>

#include "net/unix-client.h"
#include "net/protocol.h"
#include "util/log.h"
#include "util.h"

using namespace std;

void usageAndExit(string progname) {
    cout << "Usage: " << progname << " instance-id cookie command" << endl;
    cout << "Available commands:" << endl;
    cout << "    info               Index statistics and information." << endl;
    cout << "    sync               Start a full sync." << endl;
    cout << "    inspect <path>     Inspect a specific entry in the index." << endl;
    exit(0);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        usageAndExit(argv[0]);
    }

    // MSG_NOSIGNAL in send() apparently doesn't entirely prevent SIGPIPE from occurring.
    signal(SIGPIPE, SIG_IGN);

    string instanceid = argv[1];
    string cookie = argv[2];
    string command = argv[3];

    Socket::CryptoInit(cookie);

    UnixClient client(instanceid);

    if (command == "info") {
        MSG::InfoReq msg;
        client.send(msg);

        unique_ptr<MSG::InfoResp> resp = client.awaitWithType<MSG::InfoResp>(MSG::Type::INFO_RESP);
        LOG("");
        for (const auto &response : resp->payloads) {
            cout << "Payload" << endl;
            cout << "\tStatus: " << response.status << endl;
            cout << "\tInstance identifier: " << response.instanceId << endl;
            cout << "\tFiles indexed: " << response.filesIndexed << endl;
            cout << "\tIndex content hash: " << response.hash << endl;
        }
    } else if (command == "sync") {
        MSG::FullsyncCmd cmd;
        client.send(cmd);
    } else if (command == "inspect") {
        string arg = argv[4];

        MSG::InspectReq msg;
        msg.path = arg;
        client.send(msg);

        unique_ptr<MSG::InspectResp> resp = client.awaitWithType<MSG::InspectResp>(MSG::Type::INSPECT_RESP);
        LOG("");
        cout << "Payload" << endl;
        cout << "    Path: " << resp->path << endl;
        cout << "    Hash: " << resp->hash << endl;
        cout << "    nSub: " << resp->children.size() << endl;
        cout << "    Children: " << endl;
        for (const auto &child : resp->children) {
            cout << "        " << child.path << " : " << child.hash << endl;
        }
    } else {
        usageAndExit(argv[0]);
    }
}
