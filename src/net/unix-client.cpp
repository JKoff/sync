#include "unix-client.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <system_error>

using namespace std;

UnixClient::UnixClient(const std::string &instanceId) {
    string tmpname = "/tmp/sync." + instanceId;

    if ((this->sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        throw system_error(errno, system_category(), "socket");
    }

// http://www.lists.apple.com/archives/macnetworkprog/2002/Dec/msg00091.html
#ifndef MSG_NOSIGNAL
    int on = 1;
    if (setsockopt(this->sock, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) == -1) {
        throw system_error(errno, system_category(), "setsockopt SO_NOSIGPIPE");
    }
#endif

    struct sockaddr_un remote;
    remote.sun_family = AF_UNIX;
    strncpy(remote.sun_path, tmpname.c_str(), sizeof(remote.sun_path));
    socklen_t len = sizeof(struct sockaddr_un);
    if (connect(this->sock, (struct sockaddr *)(&remote), len) == -1) {
        throw system_error(errno, system_category(), "connect");
    }
}
