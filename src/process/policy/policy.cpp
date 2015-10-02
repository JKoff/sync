#include "policy.h"

#include "../../net/inet-client.h"
#include "../../net/unix-client.h"

using namespace std;

Socket PolicyHost::connect() const {
	if (this->isLocal) {
		return UnixClient(this->instanceId);
	} else {
		return InetClient(this->host, this->port);
	}
}

bool PolicyHost::operator==(const PolicyHost &other) const {
	if (this->isLocal) {
		return this->instanceId == other.instanceId;
	} else {
		return this->host == other.host && this->port == other.port;
	}
}
