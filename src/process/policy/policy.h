#ifndef PROCESS_POLICY_POLICY_H
#define PROCESS_POLICY_POLICY_H

#include <ostream>
#include <string>
#include <sstream>
#include <vector>
#include "../../fs/types.h"
// #include "../../util/serialize.h"

class Socket;

class PolicyHost {
public:
	PolicyHost() = default;
	PolicyHost(std::string instanceId) : instanceId(instanceId), isLocal(true) {}
	PolicyHost(std::string host, std::string port) : host(host), port(port), isLocal(false) {}

	Socket connect() const;

	bool operator==(const PolicyHost &other) const;

	std::string toString() const {
		std::stringstream stream;
		stream << *this;
		return stream.str();
	}

	friend std::ostream& operator<<(std::ostream &stream, const PolicyHost &h) {
		if (h.isLocal) {
			stream << h.instanceId;
		} else {
			stream << h.host << ":" << h.port;
		}
		return stream;
	}

	void serialize(std::ostream &stream) const {
		::serialize(stream, this->isLocal);
		if (this->isLocal) {
			::serialize(stream, this->instanceId);
		} else {
			::serialize(stream, this->host);
			::serialize(stream, this->port);
		}
	}
	void deserialize(std::istream &stream) {
		::deserialize(stream, this->isLocal);
		if (this->isLocal) {
			::deserialize(stream, this->instanceId);
		} else {
			::deserialize(stream, this->host);
			::deserialize(stream, this->port);
		}
	}

	std::string instanceId;
	std::string host, port;
protected:
	bool isLocal;
};

template<> struct std::less<PolicyHost> {
public:
	bool operator()(const PolicyHost &lhs, const PolicyHost &rhs) const {
	    return lhs.host < rhs.host || (lhs.host == rhs.host && lhs.port < rhs.port);
	}
};

struct PolicyStats {
	uint64_t remaining;
	uint64_t completed;
};

struct PolicyFile {
	Relpath path;
	std::string targetPath;  // for symlinks only
	FileRecord::Type type;

	void serialize(std::ostream &stream) const {
		::serialize(stream, this->path);
		::serialize(stream, this->targetPath);
		::serialize(stream, this->type);
	}
	void deserialize(std::istream &stream) {
		::deserialize(stream, this->path);
		::deserialize(stream, this->targetPath);
		::deserialize(stream, this->type);
	}
	std::string debugString() const {
		std::stringstream stream;
		stream << "PolicyFile[" << this->path << " | " << this->targetPath << " | " << this->type << "]";
		return stream.str();
	}
};

template<> struct std::less<PolicyFile> {
public:
	bool operator()(const PolicyFile &lhs, const PolicyFile &rhs) const {
	    return lhs.path < rhs.path;
	}
};

template <typename T>
struct Tree {
	T value;
	std::vector<Tree<T>> children;

	friend std::ostream& operator<<(std::ostream &stream, const Tree &h) {
		stream << "Tree[" << h.value;
		for (const Tree<T> &child : h.children) {
			stream << " | " << child;
		}
		stream << "]";
		return stream;
	}

	void serialize(std::ostream &stream) const {
		::serialize(stream, this->value);
		::serialize(stream, this->children);
	}
	void deserialize(std::istream &stream) {
		::deserialize(stream, this->value);
		::deserialize(stream, this->children);
	}
};

struct PolicyPlan {
	PolicyFile file;
	Tree<PolicyHost> steps;

	void serialize(std::ostream &stream) const {
		::serialize(stream, this->file);
		::serialize(stream, this->steps);
	}
	void deserialize(std::istream &stream) {
		::deserialize(stream, this->file);
		::deserialize(stream, this->steps);
	}
	std::string debugString() const {
		std::stringstream stream;
		stream << "PolicyPlan[file=" << file.debugString() << "]";
		return stream.str();
	}
};

class Policy {
public:
	Policy(const PolicyHost &us) : us(us) {}
	virtual void push(const PolicyHost &host, const PolicyFile &file) = 0;
	virtual PolicyPlan pop(const PolicyHost &host) = 0;
	virtual PolicyStats stats(const PolicyHost &host) = 0;
	const PolicyHost us;
};

#endif
