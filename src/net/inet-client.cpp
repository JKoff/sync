#include "inet-client.h"

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
#include <utility>

#include "../util.h"
#include "../util/log.h"
#include "util.h"

using namespace std;

InetClient::InetClient(const std::string &host, const std::string &port) {
    AddrInfo info(host, port);
    
    for (struct addrinfo *p = info.servinfo; p != nullptr; p = p->ai_next) {
        try {
            this->attemptConnect(p);
        } catch (const system_error &e) {
            ERR("InetClient failed to bind: " << e.what());

            close(this->sock);
            this->sock = 0;

            // Oh well, try next one.
        }
    }

    if (this->sock == 0) {
        // We couldn't bind to any address.
        throw runtime_error("InetClient could not connect to any address.");
    }
}

void InetClient::attemptConnect(struct addrinfo *p) {
    if ((this->sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
        throw system_error(errno, system_category(), "socket");
    }

    if (connect(this->sock, p->ai_addr, p->ai_addrlen) == -1) {
        throw system_error(errno, system_category(), "connect");
    }
}
