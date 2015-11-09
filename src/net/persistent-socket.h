#ifndef NET_PERSISTENT_SOCKET_H
#define NET_PERSISTENT_SOCKET_H

#include <chrono>
#include <functional>
#include <stdexcept>
#include <thread>
#include "../net/protocol-interface.h"
#include "../net/socket.h"
#include "../process/process.h"

/**
 * A PersistentSocket is simply a socket that avoids inactivity exceeding a given
 * duration. This enables effective use of timeouts on the server side.
 */

enum class PersistentSocketMessageType {
    BORROW_SOCKET,
    RETURN_SOCKET,
    INVALIDATE_SOCKET,
    TERMINATE
};

class PersistentSocket : public Process<PersistentSocketMessageType> {
	// RAII to make borrowing safe despite the possibility of a failed send.
	template <typename T>
	struct PerformWithBorrowedSocket {
	    PerformWithBorrowedSocket(PersistentSocket *that, std::function<void (Socket &sock)> fn) : that(that) {
	        {
	            Message msg;
	            msg.type = MT::BORROW_SOCKET;
	            Any a = that->call(msg, std::chrono::seconds(5));
	            this->sock = a.cast<Socket*>();  // will not throw if typing bug does not exist
	        }

	        try {
	            fn(*(this->sock));
	        } catch (const std::exception &e) {
	            // Exception can bubble up, but first we invalidate the socket.
	            Message msg;
	            msg.type = MT::INVALIDATE_SOCKET;
	            that->cast(msg);
	            throw;
	        }
	    }
	    ~PerformWithBorrowedSocket() {
            Message msg;
	        msg.type = MT::RETURN_SOCKET;
            that->cast(msg);
	    }

	private:
	    PersistentSocket *that;
	    Socket *sock = nullptr;
	};

public:
    PersistentSocket(std::function<Socket ()> constructorFn);
    ~PersistentSocket();

    //////////////
    // External //
    //////////////

    template <typename T>
    void send(const T &msg) {
    	PerformWithBorrowedSocket<T> send(this, [&msg] (Socket &sock) {
    		sock.send(msg);
    	});
    }

	// Packets can be pretty big——need to be allocated on heap, and can't be copied around.
	template <typename T>
	std::unique_ptr<T> awaitWithType(
		MSG::Type type,
		std::chrono::duration<uint64_t> timeout=std::chrono::seconds(30)
	) {
		std::unique_ptr<T> result;
		PerformWithBorrowedSocket<T> receive(this, [&type, &timeout, &result] (Socket &sock) {
    		result = sock.awaitWithType<T>(type, timeout);
    	});
		return result;
	}

private:
    std::thread th;
};

#endif
