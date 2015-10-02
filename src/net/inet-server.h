#ifndef NET_INET_SERVER_H
#define NET_INET_SERVER_H

#include <functional>
#include <string>
#include "socket.h"
#include "../index.h"

class InetServer : public Socket {
public:
	InetServer() = delete;
	// This constructor contains the run loop and therefore never terminates.
	InetServer(const std::string &host, const std::string &port, function<void (Socket &)> connFn);
private:
	void attemptBind(struct addrinfo *p);
};

#endif
