#include "socket.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <system_error>
#include "snappy/snappy.h"
#include "../util/log.h"
#include <stdio.h>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

using namespace std;

// http://www.lists.apple.com/archives/macnetworkprog/2002/Dec/msg00091.html
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

class CipherCtx {
public:
	CipherCtx() {
		EVP_CIPHER_CTX_init(&(this->ctx));
	}

	void printError() {
		char buf[120];
		unsigned long err;
		ERR("openssl error:");
		while ((err = ERR_get_error())) {
			ERR_error_string(err, buf);
			ERR("  " << buf);
		}
	}

	void initEncrypt(const unsigned char *key, const unsigned char *iv) {
		if (1 != EVP_EncryptInit_ex(&(this->ctx), EVP_aes_256_gcm(), NULL, NULL, NULL)) {
			this->printError();
			throw runtime_error("Failed to initialize encryption operation.");
		}

		if (1 != EVP_EncryptInit_ex(&(this->ctx), NULL, NULL, key, iv)) {
			this->printError();
			throw runtime_error("Failed to initialize encryption key and IV.");
		}
	}

	int encrypt(const unsigned char *plaintext, int plaintext_len, unsigned char *ciphertext) {
		int len, ciphertext_len;

		if (1 != EVP_EncryptUpdate(&(this->ctx), ciphertext, &len, plaintext, plaintext_len)) {
			this->printError();
			throw runtime_error("Failed to encrypt.");
		}
		ciphertext_len = len;
		// LOG("len was " << len << " for plaintext_len = " << plaintext_len);
		// return 0;

		if (1 != EVP_EncryptFinal_ex(&(this->ctx), ciphertext + len, &len)) {
			this->printError();
			throw runtime_error("Failed to finalize encryption.");
		}
		ciphertext_len += len;

		return ciphertext_len;
	}

	void tag(unsigned char *tag) {
		if (1 != EVP_CIPHER_CTX_ctrl(&(this->ctx), EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag)) {
			this->printError();
			throw runtime_error("Failed to get encryption tag.");
		}
	}

	void nonce(unsigned char *iv) {
		if (1 != RAND_bytes(iv, IV_SIZE)) {
			this->printError();
			throw runtime_error("Failed to generate nonce.");
		}
	}

	void initDecrypt(const unsigned char *key, const unsigned char *iv) {
		if (1 != EVP_DecryptInit_ex(&(this->ctx), EVP_aes_256_gcm(), NULL, NULL, NULL)) {
			this->printError();
			throw runtime_error("Failed to initialize decryption operation.");
		}

		if (1 != EVP_DecryptInit_ex(&(this->ctx), NULL, NULL, key, iv)) {
			this->printError();
			throw runtime_error("Failed to initialize decryption key and IV.");
		}
	}

	int decrypt(const unsigned char *ciphertext, int ciphertext_len, const unsigned char *tag, unsigned char *plaintext) {
		int len, plaintext_len;

		if (1 != EVP_DecryptUpdate(&(this->ctx), plaintext, &len, ciphertext, ciphertext_len)) {
			this->printError();
			throw runtime_error("Failed to decrypt.");
		}
		plaintext_len = len;

		if (1 != EVP_CIPHER_CTX_ctrl(&(this->ctx), EVP_CTRL_GCM_SET_TAG, TAG_SIZE, const_cast<unsigned char*>(tag))) {
			this->printError();
			throw runtime_error("Failed to set decryption tag.");
		}

		if (1 != EVP_DecryptFinal_ex(&(this->ctx), plaintext + len, &len)) {
			this->printError();
			throw runtime_error("Failed to finalize decryption.");
		}
		plaintext_len += len;

		return plaintext_len;
	}

	~CipherCtx() {
		EVP_CIPHER_CTX_cleanup(&(this->ctx));
	}

private:
	EVP_CIPHER_CTX ctx;
};


//////////////////
// SocketCrypto //
//////////////////

SocketCrypto::SocketCrypto() {
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
	OPENSSL_config(NULL);

	// A (key, IV) pair must be unique.
	// In order to avoid using extreme amounts of entropy, we seed with a random 12-byte number
	// and then increment that number on each call to encrypt.
	CipherCtx ctx;
	ctx.nonce(this->iv);
}

SocketCrypto::~SocketCrypto() {
	EVP_cleanup();
	ERR_free_strings();
}

void SocketCrypto::setKey(const string &key) {
	assert(key.size() == 32 && "Key must be 32 bytes.");
	this->key = key;
}

void SocketCrypto::incrIV() {
	for (int byte=11; byte >= 0; byte--) {
		if (this->iv[byte] == 255) {
			this->iv[byte] = 0;
			// and continue iterating for carry
		} else {
			this->iv[byte] = this->iv[byte] + 1;
			break;
		}
	}
}

void SocketCrypto::encrypt(const unsigned char *buffer, size_t length, unsigned char *outBuffer, int *outBufLen) {
	// outBuffer layout is |  TAG  |   IV   | BUFFER |

	unsigned char *tagOut = outBuffer;
	unsigned char *ivOut = outBuffer + TAG_SIZE;
	unsigned char *bufOut = outBuffer + TAG_SIZE + IV_SIZE;

	CipherCtx ctx;
	memcpy(ivOut, this->iv, IV_SIZE);
	ctx.initEncrypt(
		reinterpret_cast<const unsigned char*>(this->key.data()),
		ivOut
	);
	int ciphertext_len = ctx.encrypt(buffer, length, bufOut);
	ctx.tag(tagOut);

	*outBufLen = ciphertext_len + TAG_SIZE + IV_SIZE;

	this->incrIV();
}

void SocketCrypto::decrypt(const unsigned char *buffer, size_t length, unsigned char *outBuffer, int *outBufLen) {
	CipherCtx ctx;
	
	const unsigned char *tagIn = buffer;
	const unsigned char *ivIn = buffer + TAG_SIZE;
	const unsigned char *bufIn = buffer + TAG_SIZE + IV_SIZE;

	ctx.initDecrypt(
		reinterpret_cast<const unsigned char*>(this->key.data()),
		ivIn
	);
	*outBufLen = ctx.decrypt(bufIn, length - IV_SIZE - TAG_SIZE, tagIn, outBuffer);
}


///////////////////
// Socket static //
///////////////////

void Socket::CryptoInit(const string &key) {
	Socket::socketCrypto.setKey(key);
}
SocketCrypto Socket::socketCrypto;


////////////
// Socket //
////////////

Socket::Socket()
: sock(0), buf(new char[this->BUF_SIZE]), buf2(new char[this->BUF_SIZE]) {
  	// empty
}

Socket::~Socket() {
    if (this->sock) {
        close(this->sock);
        this->sock = 0;
    }
}

void Socket::awaitWithHandler(
	std::function<void (MSG::Type type, MSG::Base *msg)> handler,
	chrono::duration<uint64_t> timeout
) {
	// zero means no timeout
	Frame frame = this->receive(timeout);
	handler(frame.header.type, frame.message.get());
}

void Socket::receiveSome(char *buf, size_t neededIn) {
	ssize_t needed = neededIn;
	ssize_t received = 0;

	while (needed > 0) {
		int len = recv(this->sock, buf + received, needed, MSG_WAITALL);

		if (len == 0) {
	    	// Connection closed by remote.
	    	throw runtime_error("recv: Connection closed by remote.");
	    } else if (len == -1) {
	    	// Error.
	        throw system_error(errno, system_category(), "recv");   
	    }

		received += len;
		needed -= len;

	    StatusLine::Add("inbound", received);
	}

	assert(needed == 0);
}

template <typename T>
void deserializeFromStream(const char *buf, const size_t size, T &result) {
    string str(buf, size);
    istringstream ss(str, istringstream::binary);
    ss.exceptions(istringstream::failbit | istringstream::badbit);
    result.deserialize(ss);
}

Socket::Frame Socket::receive(chrono::duration<uint64_t> timeout) {
	char *buf = this->buf.get();
	char *buf2 = this->buf2.get();

	timeval tv = chronoToTimeval(timeout);
	setsockopt(this->sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(timeval));

	int bufSize = 0;

	//////////////////////
	// Encrypted packet //
	//////////////////////

	{
		EncryptedHeader enchdr;

		// Receive and deserialize header

		this->receiveSome(buf, enchdr.wireSize());
		deserializeFromStream(buf, enchdr.wireSize(), enchdr);
		assert(enchdr.size <= this->BUF_SIZE);

		// Receive and decrypt body

		this->receiveSome(buf, enchdr.size - enchdr.wireSize());
		Socket::socketCrypto.decrypt(
			reinterpret_cast<const unsigned char *>(buf),
			enchdr.size - enchdr.wireSize(),
			reinterpret_cast<unsigned char *>(buf2),
			&bufSize
		);
		assert(bufSize <= this->BUF_SIZE);
	}

	// Out: Decrypted packet in buf2
	// Out: Decrypted packet size in bufSize

	///////////////////////
	// Compressed packet //
	///////////////////////

	{
		Header hdr;

		// Deserialize header

		deserializeFromStream(buf2, hdr.wireSize(), hdr);
		assert(hdr.type == MSG::Type::COMPRESSED);
		assert(hdr.size <= this->BUF_SIZE);

		// Decompress body

		string uncompressed;
		bool success = snappy::Uncompress(buf2 + hdr.wireSize(), hdr.size - hdr.wireSize(), &uncompressed);
		if (!success) {
			throw runtime_error("snappy decompression of packet failed.");
		}
	    assert(uncompressed.capacity() <= this->BUF_SIZE);
		memmove(buf, uncompressed.data(), uncompressed.capacity());

		bufSize = uncompressed.capacity();
	}

	// Out: Uncompressed packet in buf
	// Out: Uncompressed packet size in bufSize

	/////////////////////////
	// Uncompressed packet //
	/////////////////////////

	{
		Header hdr;

		// Deserialize header
		{
			deserializeFromStream(buf, hdr.wireSize(), hdr);
			assert(hdr.type != MSG::Type::UNSET);
			assert(hdr.size <= this->BUF_SIZE);
		}

		// Deserialize body
		{
			unique_ptr<MSG::Base> sub = MSG::Factory::Create(hdr.type);
			deserializeFromStream(buf + hdr.wireSize(), hdr.size - hdr.wireSize(), *sub);
			return Frame{hdr, move(sub)};
		}
	}
}

void Socket::sendBuffer(const void *buffer, size_t length) const {
	// buffer contains {header,message}

	// Now we compress.

	string compressed;
	size_t compressedSize = snappy::Compress((const char *)buffer, length, &compressed);

	Packet compressedPacket = { MSG::Type::COMPRESSED, compressed.data(), compressedSize };
	string packetStr = compressedPacket.toString();

	// packetStr contains {header(COMPRESSED, compressedSize), compressed({header,message})}

	char *buf = this->buf.get();
	int len;
	Socket::socketCrypto.encrypt(
		reinterpret_cast<const unsigned char *>(packetStr.data()),
		packetStr.size(),
		reinterpret_cast<unsigned char*>(buf),
		&len
	);

	EncryptedPacket encryptedPacket = { buf, static_cast<size_t>(len) };
	string encPacketStr = encryptedPacket.toString();

	// encPacketStr contains {header(encryptedSize), encrypted(...)}

	if (::send(this->sock, encPacketStr.data(), encPacketStr.size(), MSG_NOSIGNAL) != encPacketStr.size()) {
        throw system_error(errno, system_category(), "send");
    }

    StatusLine::Add("outbound", encPacketStr.size());
}
