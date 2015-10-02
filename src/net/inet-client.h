#ifndef NET_INET_CLIENT_H
#define NET_INET_CLIENT_H

#include <string>
#include "socket.h"
#include "../index.h"

class InetClient : public Socket {
public:
	InetClient() {}
	// This constructor contains the run loop and therefore never terminates.
	InetClient(const std::string &host, const std::string &port);
private:
	void attemptConnect(struct addrinfo *p);
};

#endif
