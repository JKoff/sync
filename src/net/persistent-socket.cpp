#include "persistent-socket.h"

#include <cassert>

#include "../util/log.h"
#include "socket.h"

using namespace std;

PersistentSocket::PersistentSocket(function<Socket ()> constructorFn) {
    this->th = thread([this, &constructorFn] () {
        bool isBorrowed = false;
        // If we haven't used the socket in `expiry` seconds, close it.
        chrono::duration<uint64_t> expiry = chrono::seconds(10);
        unique_ptr<Socket> managedSocket;

        StatusLine status("PersistentSocket");

        function<void (Message&)> messageFn = [&status, &isBorrowed, &managedSocket] (Message &msg) {
            switch (msg.type) {
            case MT::BORROW_SOCKET:
                STATUS(status, "BORROW_SOCKET");
                break;
            case MT::RETURN_SOCKET:
                STATUS(status, "RETURN_SOCKET");
                break;
            case MT::INVALIDATE_SOCKET:
                STATUS(status, "INVALIDATE_SOCKET");
                break;
            case MT::TERMINATE:
                STATUS(status, "TERMINATE");
                break;
            }

            STATUSVAR(status, "borrowed", (isBorrowed ? "true" : "false"));
            STATUSVAR(status, "managedSocket", (managedSocket ? "true" : "false"));
        };

        bool terminated = false;

        while (!terminated) {
            if (isBorrowed) {
                // We'll never expire a borrowed socket. A borrowed socket MUST get returned.
                Message msg = this->peek();
                messageFn(msg);
                switch (msg.type) {
                case MT::TERMINATE:
                    terminated = true;
                    break;
                case MT::INVALIDATE_SOCKET:
                    // Never use it again. We still expect a RETURN_SOCKET after this.
                    managedSocket.reset();
                    messageFn(msg);
                    break;
                case MT::RETURN_SOCKET:
                    isBorrowed = false;
                    messageFn(msg);
                    break;
                case MT::BORROW_SOCKET:
                    // TODO: I'm at a loss as to why this case occurs.
                    isBorrowed = false;
                    messageFn(msg);
                    // Give it another try with isBorrowed reset.
                    continue;
                }
                this->consume();
            } else {
                Message msg;

                try {
                    // At this point, we either have no managed socket, or a non-expired socket.
                    // Non-expired under the assumption that there was no significant gap between
                    // socket return and execution of this line.
                    if (managedSocket) {
                        // socket exists, need to time it out at some point
                        msg = this->peek(expiry);
                    } else {
                        // no socket, can block indefinitely
                        msg = this->peek();
                    }
                    messageFn(msg);
                } catch (const timeout_error &e) {
                    // Our socket expired before we received a further attempt to borrow it.
                    // Start over with no managed socket.
                    managedSocket.reset();
                    messageFn(msg);
                    continue;
                }

                if (msg.type == MT::TERMINATE) {
                    break;
                }

                if (msg.type == MT::RETURN_SOCKET) {
                    // Maybe initialization of the socket failed and we're in the destructor.
                    managedSocket.reset();
                    messageFn(msg);
                    continue;
                }

                assert(msg.type == MT::BORROW_SOCKET);

                if (!managedSocket) {
                    try {
                        managedSocket.reset(new Socket(constructorFn()));
                        messageFn(msg);
                    } catch (const exception &e) {
                        // Most likely, we couldn't connect to the remote host.
                        // Wait a bit and then allow it to try again.
                        this_thread::sleep_for(chrono::seconds(2));
                        continue;
                    }
                }

                isBorrowed = true;
                messageFn(msg);

                this->reply(msg.refid, Any(managedSocket.get()));
                
                this->consume();
            }
        }
    });
}

PersistentSocket::~PersistentSocket() {
    Message msg;
    msg.type = MT::TERMINATE;
    this->cast(msg);

    this->th.join();
}
