#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <system_error>

#include "protocol-interface.h"
#include "../util/chrono.h"
#include "../util/log.h"
#include "../util/serialize.h"
#include <stdio.h>

using namespace std;

static const int IV_SIZE = 12;
static const int TAG_SIZE = 16;

struct SocketCrypto {
	SocketCrypto();
	~SocketCrypto();
	void encrypt(const unsigned char *buffer, size_t length, unsigned char *outBuffer, int *outBufLen);
	void decrypt(const unsigned char *buffer, size_t length, unsigned char *outBuffer, int *outBufLen);
	void setKey(const std::string &key);
	void incrIV();
	std::string key;
	unsigned char iv[IV_SIZE];
};

class Socket {
	const size_t BUF_SIZE = 512*1024;

	struct EncryptedHeader {
		// Number of bytes in message, including Header itself.
		int64_t size;

		size_t wireSize() const {
			return sizeof(this->size);
		}

		void serialize(std::ostream &stream) const {
			::serialize(stream, this->size);
		}
		void deserialize(std::istream &stream) {
			::deserialize(stream, this->size);
		}
		string toString() const {
			ostringstream ss(stringstream::binary);
			this->serialize(ss);
			string str = ss.str();
			assert(str.size() == this->wireSize());
			return str;
		}
	};

	// Convenience struct for writing
	struct EncryptedPacket {
		const char *data;
		size_t len;

		string toString() const {
			// Make packet header
			EncryptedHeader hdr;
			hdr.size = static_cast<int64_t>(hdr.wireSize() + this->len);
			string hdrStr = hdr.toString();

			// Combine everything into a string
			ostringstream ss(stringstream::binary);
			ss.write(hdrStr.data(), hdrStr.size());
			ss.write(this->data, this->len);
			return ss.str();
		}
	};

	struct Header {
		// Number of bytes in message, including Header itself.
		int64_t size;
		// Type of message included.
		MSG::Type type;

		size_t wireSize() const {
			return sizeof(this->size) + sizeof(this->type);
		}

		void serialize(std::ostream &stream) const {
			::serialize(stream, this->type);
			::serialize(stream, this->size);
		}
		void deserialize(std::istream &stream) {
			::deserialize(stream, this->type);
			::deserialize(stream, this->size);
		}
		string toString() const {
			ostringstream ss(stringstream::binary);
			this->serialize(ss);
			string str = ss.str();
			assert(str.size() == this->wireSize());
			return str;
		}
	};

	// Convenience struct for writing
	struct Packet {
		MSG::Type type;
		const char *data;
		size_t len;

		string toString() const {
			// Make packet header
			Header hdr;
			hdr.type = this->type;
			hdr.size = static_cast<int64_t>(hdr.wireSize() + this->len);
			string hdrStr = hdr.toString();

			// Combine everything into a string
			ostringstream ss(stringstream::binary);
			ss.write(hdrStr.data(), hdrStr.size());
			ss.write(this->data, this->len);
			return ss.str();
		}
	};


	// Uncompressed, unencrypted packet for returning/passing around.
	struct Frame {
		Header header;
		unique_ptr<MSG::Base> message;
	};

public:
	Socket();
	Socket(const Socket &other) = delete;
	Socket(Socket &&other) {
		this->sock = other.sock;
		other.sock = 0;

		this->buf = std::move(other.buf);
		this->buf2 = std::move(other.buf2);
	}
	Socket& operator=(const Socket &other) = delete;
	Socket& operator=(Socket &&other) {
		swap(this->sock, other.sock);
		swap(this->buf, other.buf);
		swap(this->buf2, other.buf2);
		return *this;
	}
	virtual ~Socket();

	template <typename T>
	void send(const T &msg) const {
		ostringstream msgSs(stringstream::binary);
		msg.serialize(msgSs);
		string message = msgSs.str();

		Packet packet = { MSG::Factory::EnumType<T>(), message.data(), message.size() };
		string packetStr = packet.toString();
		this->sendBuffer(packetStr.data(), packetStr.size());
	}

	void awaitWithHandler(
		std::function<void (MSG::Type type, MSG::Base *msg)> handler,
		chrono::duration<uint64_t> timeout=chrono::seconds(0)
	);

	// Packets can be pretty big——need to be allocated on heap, and can't be copied around.
	template <typename T>
	std::unique_ptr<T> awaitWithType(
		MSG::Type type,
		chrono::duration<uint64_t> timeout=chrono::seconds(30)
	) {
		Frame frame = this->receive(timeout);
		T *castedSub = dynamic_cast<T*>(frame.message.release());
		return unique_ptr<T>(castedSub);
	}

	static void CryptoInit(const string &key);

protected:
	static SocketCrypto socketCrypto;

	void receiveSome(char *buf, size_t neededIn);

	// Receives a message to the internal buffer.
	Frame receive(std::chrono::duration<uint64_t> timeout=std::chrono::seconds(30));

	void sendBuffer(const void *buffer, size_t length) const;

	unsigned int sock;
	std::unique_ptr<char> buf, buf2;
};

#endif
