#include "unix-server.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

#include "../util.h"

using namespace std;

class UnixRemoteSocket : public Socket {
public:
	UnixRemoteSocket() = delete;
	UnixRemoteSocket(unsigned int localSocket) {
    	struct sockaddr_un remote;
    	socklen_t len = sizeof(struct sockaddr_un);
	    if ((this->sock = accept(localSocket, (struct sockaddr *)(&remote), &len)) == -1) {
	    	throw system_error(errno, system_category(), "accept");
	    }
	}
};

UnixServer::UnixServer(const string &instanceId, function<void (Socket&)> connFn) {
	this->tmppath = "/tmp/sync." + instanceId;

    if ((this->sock = ::socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        throw system_error(errno, system_category(), "socket");
    }

    struct sockaddr_un local;
    local.sun_family = AF_UNIX;
    strncpy(local.sun_path, this->tmppath.c_str(), sizeof(local.sun_path));
    unlink(this->tmppath.c_str());
    if (::bind(this->sock, (struct sockaddr *)(&local), sizeof(struct sockaddr_un)) == -1) {
        throw system_error(errno, system_category(), "bind");
    }

	if (::listen(this->sock, 10) == -1) {
        throw system_error(errno, system_category(), "listen");
    }

    const uint32_t MAX_THREADS = 2;
    Socket sockets[MAX_THREADS];
    thread conns[MAX_THREADS];
    for (int connId=0; ; connId = (connId + 1) % MAX_THREADS) {
        if (conns[connId].joinable()) {
            conns[connId].join();
        }

        sockets[connId] = UnixRemoteSocket(this->sock);
        conns[connId] = thread(bind(connFn, ref(sockets[connId])));
	}
}

UnixServer::~UnixServer() {
	if (this->tmppath.size() > 0) {
		unlink(this->tmppath.c_str());
	}
}
