#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <netdb.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>

class AddrInfo {
public:
    AddrInfo(const std::string &host, const std::string &port);
    ~AddrInfo();

    AddrInfo(const AddrInfo&) = delete;
    AddrInfo(AddrInfo&&) = delete;
    AddrInfo& operator=(const AddrInfo& other) = delete;
    AddrInfo& operator=(AddrInfo&& other) = delete;

    struct addrinfo *servinfo;
};

#endif
