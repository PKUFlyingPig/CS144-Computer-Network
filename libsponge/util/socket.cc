#include "socket.hh"

#include "util.hh"

#include <cstddef>
#include <stdexcept>
#include <unistd.h>

using namespace std;

// default constructor for socket of (subclassed) domain and type
//! \param[in] domain is as described in [socket(7)](\ref man7::socket), probably `AF_INET` or `AF_UNIX`
//! \param[in] type is as described in [socket(7)](\ref man7::socket)
Socket::Socket(const int domain, const int type) : FileDescriptor(SystemCall("socket", socket(domain, type, 0))) {}

// construct from file descriptor
//! \param[in] fd is the FileDescriptor from which to construct
//! \param[in] domain is `fd`'s domain; throws std::runtime_error if wrong value is supplied
//! \param[in] type is `fd`'s type; throws std::runtime_error if wrong value is supplied
Socket::Socket(FileDescriptor &&fd, const int domain, const int type) : FileDescriptor(move(fd)) {
    int actual_value;
    socklen_t len;

    // verify domain
    len = sizeof(actual_value);
    SystemCall("getsockopt", getsockopt(fd_num(), SOL_SOCKET, SO_DOMAIN, &actual_value, &len));
    if ((len != sizeof(actual_value)) or (actual_value != domain)) {
        throw runtime_error("socket domain mismatch");
    }

    // verify type
    len = sizeof(actual_value);
    SystemCall("getsockopt", getsockopt(fd_num(), SOL_SOCKET, SO_TYPE, &actual_value, &len));
    if ((len != sizeof(actual_value)) or (actual_value != type)) {
        throw runtime_error("socket type mismatch");
    }
}

// get the local or peer address the socket is connected to
//! \param[in] name_of_function is the function to call (string passed to SystemCall())
//! \param[in] function is a pointer to the function
//! \returns the requested Address
Address Socket::get_address(const string &name_of_function,
                            const function<int(int, sockaddr *, socklen_t *)> &function) const {
    Address::Raw address;
    socklen_t size = sizeof(address);

    SystemCall(name_of_function, function(fd_num(), address, &size));

    return {address, size};
}

//! \returns the local Address of the socket
Address Socket::local_address() const { return get_address("getsockname", getsockname); }

//! \returns the socket's peer's Address
Address Socket::peer_address() const { return get_address("getpeername", getpeername); }

// bind socket to a specified local address (usually to listen/accept)
//! \param[in] address is a local Address to bind
void Socket::bind(const Address &address) { SystemCall("bind", ::bind(fd_num(), address, address.size())); }

// connect socket to a specified peer address
//! \param[in] address is the peer's Address
void Socket::connect(const Address &address) { SystemCall("connect", ::connect(fd_num(), address, address.size())); }

// shut down a socket in the specified way
//! \param[in] how can be `SHUT_RD`, `SHUT_WR`, or `SHUT_RDWR`; see [shutdown(2)](\ref man2::shutdown)
void Socket::shutdown(const int how) {
    SystemCall("shutdown", ::shutdown(fd_num(), how));
    switch (how) {
        case SHUT_RD:
            register_read();
            break;
        case SHUT_WR:
            register_write();
            break;
        case SHUT_RDWR:
            register_read();
            register_write();
            break;
        default:
            throw runtime_error("Socket::shutdown() called with invalid `how`");
    }
}

//! \note If `mtu` is too small to hold the received datagram, this method throws a std::runtime_error
void UDPSocket::recv(received_datagram &datagram, const size_t mtu) {
    // receive source address and payload
    Address::Raw datagram_source_address;
    datagram.payload.resize(mtu);

    socklen_t fromlen = sizeof(datagram_source_address);

    const ssize_t recv_len = SystemCall(
        "recvfrom",
        ::recvfrom(
            fd_num(), datagram.payload.data(), datagram.payload.size(), MSG_TRUNC, datagram_source_address, &fromlen));

    if (recv_len > ssize_t(mtu)) {
        throw runtime_error("recvfrom (oversized datagram)");
    }

    register_read();
    datagram.source_address = {datagram_source_address, fromlen};
    datagram.payload.resize(recv_len);
}

UDPSocket::received_datagram UDPSocket::recv(const size_t mtu) {
    received_datagram ret{{nullptr, 0}, ""};
    recv(ret, mtu);
    return ret;
}

void sendmsg_helper(const int fd_num,
                    const sockaddr *destination_address,
                    const socklen_t destination_address_len,
                    const BufferViewList &payload) {
    auto iovecs = payload.as_iovecs();

    msghdr message{};
    message.msg_name = const_cast<sockaddr *>(destination_address);
    message.msg_namelen = destination_address_len;
    message.msg_iov = iovecs.data();
    message.msg_iovlen = iovecs.size();

    const ssize_t bytes_sent = SystemCall("sendmsg", ::sendmsg(fd_num, &message, 0));

    if (size_t(bytes_sent) != payload.size()) {
        throw runtime_error("datagram payload too big for sendmsg()");
    }
}

void UDPSocket::sendto(const Address &destination, const BufferViewList &payload) {
    sendmsg_helper(fd_num(), destination, destination.size(), payload);
    register_write();
}

void UDPSocket::send(const BufferViewList &payload) {
    sendmsg_helper(fd_num(), nullptr, 0, payload);
    register_write();
}

// mark the socket as listening for incoming connections
//! \param[in] backlog is the number of waiting connections to queue (see [listen(2)](\ref man2::listen))
void TCPSocket::listen(const int backlog) { SystemCall("listen", ::listen(fd_num(), backlog)); }

// accept a new incoming connection
//! \returns a new TCPSocket connected to the peer.
//! \note This function blocks until a new connection is available
TCPSocket TCPSocket::accept() {
    register_read();
    return TCPSocket(FileDescriptor(SystemCall("accept", ::accept(fd_num(), nullptr, nullptr))));
}

// set socket option
//! \param[in] level The protocol level at which the argument resides
//! \param[in] option A single option to set
//! \param[in] option_value The value to set
//! \details See [setsockopt(2)](\ref man2::setsockopt) for details.
template <typename option_type>
void Socket::setsockopt(const int level, const int option, const option_type &option_value) {
    SystemCall("setsockopt", ::setsockopt(fd_num(), level, option, &option_value, sizeof(option_value)));
}

// allow local address to be reused sooner, at the cost of some robustness
//! \note Using `SO_REUSEADDR` may reduce the robustness of your application
void Socket::set_reuseaddr() { setsockopt(SOL_SOCKET, SO_REUSEADDR, int(true)); }
