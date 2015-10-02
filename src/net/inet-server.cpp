#include "inet-server.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <system_error>
#include <thread>

#include "../util.h"
#include "../util/log.h"
#include "util.h"

using namespace std;

class InetRemoteSocket : public Socket {
public:
	InetRemoteSocket() = delete;
	InetRemoteSocket(unsigned int localSocket) {
        socklen_t sin_size = sizeof this->their_addr;

	    if ((this->sock = accept(localSocket, (struct sockaddr *)(&this->their_addr), &sin_size)) == -1) {
	    	throw system_error(errno, system_category(), "accept");
	    }
	}
    string peerString() {
        char s[INET6_ADDRSTRLEN];

        void *in_addr;
        if (this->their_addr.ss_family == AF_INET) {
            in_addr = &(((struct sockaddr_in*)&this->their_addr)->sin_addr);
        } else {
            in_addr = &(((struct sockaddr_in6*)&this->their_addr)->sin6_addr);
        }

        inet_ntop(their_addr.ss_family, in_addr, s, sizeof s);

        return string(s);
    }

private:
    struct sockaddr_storage their_addr;
};

InetServer::InetServer(const std::string &host, const std::string &port, function<void (Socket &)> connFn) {
    AddrInfo info(host, port);
    
    for (struct addrinfo *p = info.servinfo; p != nullptr; p = p->ai_next) {
        try {
            this->attemptBind(p);
        } catch (const system_error &e) {
            ERR("InetServer failed to bind: " << e.what());

            close(this->sock);
            this->sock = 0;

            // Oh well, try next one.
        }
    }

    if (this->sock == 0) {
        // We couldn't bind to any address.
        throw runtime_error("InetServer could not bind to any address.");
    }

    if (listen(this->sock, 10) == -1) {
        throw system_error(errno, system_category(), "listen");
    }

    const uint32_t MAX_THREADS = 10;
    Socket sockets[MAX_THREADS];
    thread conns[MAX_THREADS];
    for (int connId=0; ; connId = (connId + 1) % MAX_THREADS) {
        if (conns[connId].joinable()) {
            conns[connId].join();
        }

        sockets[connId] = InetRemoteSocket(this->sock);
        conns[connId] = thread(bind(connFn, ref(sockets[connId])));
    }
}

void InetServer::attemptBind(struct addrinfo *p) {
    if ((this->sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
        throw system_error(errno, system_category(), "socket");
    }

    int yes = 1;
    if (setsockopt(this->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        throw system_error(errno, system_category(), "setsockopt");
    }

    if (::bind(this->sock, p->ai_addr, p->ai_addrlen) == -1) {
        throw system_error(errno, system_category(), "bind");
    }
}
