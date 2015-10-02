#ifndef NET_UNIX_CLIENT_H
#define NET_UNIX_CLIENT_H

#include "socket.h"

class UnixClient : public Socket {
public:
	UnixClient() = delete;
	// This constructor contains the run loop and therefore never terminates.
	UnixClient(const std::string &instanceId);
};

#endif
