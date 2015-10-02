#include "util.h"

#include <arpa/inet.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <unistd.h>
#include <system_error>

#include "../util/log.h"

using namespace std;

AddrInfo::AddrInfo(const std::string &host, const std::string &port) : servinfo(nullptr) {
    struct addrinfo hints;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &this->servinfo) != 0) {
        throw system_error(errno, system_category(), "getaddrinfo");
    }
}

AddrInfo::~AddrInfo() {
    if (this->servinfo != nullptr) {
        freeaddrinfo(this->servinfo);
    }
}
