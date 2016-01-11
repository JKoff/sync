#ifndef NET_UNIX_CLIENT_H
#define NET_UNIX_CLIENT_H

#include <utility>
#include "socket.h"

class UnixClient : public Socket {
public:
	UnixClient() = delete;
	// This constructor contains the run loop and therefore never terminates.
	UnixClient(const std::string &instanceId);

	UnixClient(const UnixClient &other) = delete;
	UnixClient(UnixClient &&other) : Socket(std::move(other)) {}
};

#endif
