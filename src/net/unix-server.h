#ifndef NET_UNIX_SERVER_H
#define NET_UNIX_SERVER_H

#include <functional>
#include <memory>
#include <string>
#include "socket.h"
#include "../index.h"

class UnixServer : public Socket {
public:
	UnixServer() = delete;
	// This constructor contains the run loop and therefore never terminates.
	UnixServer(const std::string &instanceId, std::function<void (Socket&)> connFn);
	~UnixServer();

private:
	string tmppath;
};

#endif
