#ifndef SPONGE_LIBSPONGE_SOCKET_HH
#define SPONGE_LIBSPONGE_SOCKET_HH

#include "address.hh"
#include "file_descriptor.hh"

#include <cstdint>
#include <functional>
#include <string>
#include <sys/socket.h>

//! \brief Base class for network sockets (TCP, UDP, etc.)
//! \details Socket is generally used via a subclass. See TCPSocket and UDPSocket for usage examples.
class Socket : public FileDescriptor {
  private:
    //! Get the local or peer address the socket is connected to
    Address get_address(const std::string &name_of_function,
                        const std::function<int(int, sockaddr *, socklen_t *)> &function) const;

  protected:
    //! Construct via [socket(2)](\ref man2::socket)
    Socket(const int domain, const int type);

    //! Construct from a file descriptor.
    Socket(FileDescriptor &&fd, const int domain, const int type);

    //! Wrapper around [setsockopt(2)](\ref man2::setsockopt)
    template <typename option_type>
    void setsockopt(const int level, const int option, const option_type &option_value);

  public:
    //! Bind a socket to a specified address with [bind(2)](\ref man2::bind), usually for listen/accept
    void bind(const Address &address);

    //! Connect a socket to a specified peer address with [connect(2)](\ref man2::connect)
    void connect(const Address &address);

    //! Shut down a socket via [shutdown(2)](\ref man2::shutdown)
    void shutdown(const int how);

    //! Get local address of socket with [getsockname(2)](\ref man2::getsockname)
    Address local_address() const;
    //! Get peer address of socket with [getpeername(2)](\ref man2::getpeername)
    Address peer_address() const;

    //! Allow local address to be reused sooner via [SO_REUSEADDR](\ref man7::socket)
    void set_reuseaddr();
};

//! A wrapper around [UDP sockets](\ref man7::udp)
class UDPSocket : public Socket {
  protected:
    //! \brief Construct from FileDescriptor (used by TCPOverUDPSocketAdapter)
    //! \param[in] fd is the FileDescriptor from which to construct
    explicit UDPSocket(FileDescriptor &&fd) : Socket(std::move(fd), AF_INET, SOCK_DGRAM) {}

  public:
    //! Default: construct an unbound, unconnected UDP socket
    UDPSocket() : Socket(AF_INET, SOCK_DGRAM) {}

    //! Returned by UDPSocket::recv; carries received data and information about the sender
    struct received_datagram {
        Address source_address;  //!< Address from which this datagram was received
        std::string payload;     //!< UDP datagram payload
    };

    //! Receive a datagram and the Address of its sender
    received_datagram recv(const size_t mtu = 65536);

    //! Receive a datagram and the Address of its sender (caller can allocate storage)
    void recv(received_datagram &datagram, const size_t mtu = 65536);

    //! Send a datagram to specified Address
    void sendto(const Address &destination, const BufferViewList &payload);

    //! Send datagram to the socket's connected address (must call connect() first)
    void send(const BufferViewList &payload);
};

//! \class UDPSocket
//! Functions in this class are essentially wrappers over their POSIX eponyms.
//!
//! Example:
//!
//! \include socket_example_1.cc

//! A wrapper around [TCP sockets](\ref man7::tcp)
class TCPSocket : public Socket {
  private:
    //! \brief Construct from FileDescriptor (used by accept())
    //! \param[in] fd is the FileDescriptor from which to construct
    explicit TCPSocket(FileDescriptor &&fd) : Socket(std::move(fd), AF_INET, SOCK_STREAM) {}

  public:
    //! Default: construct an unbound, unconnected TCP socket
    TCPSocket() : Socket(AF_INET, SOCK_STREAM) {}

    //! Mark a socket as listening for incoming connections
    void listen(const int backlog = 16);

    //! Accept a new incoming connection
    TCPSocket accept();
};

//! \class TCPSocket
//! Functions in this class are essentially wrappers over their POSIX eponyms.
//!
//! Example:
//!
//! \include socket_example_2.cc

//! A wrapper around [Unix-domain stream sockets](\ref man7::unix)
class LocalStreamSocket : public Socket {
  public:
    //! Construct from a file descriptor
    explicit LocalStreamSocket(FileDescriptor &&fd) : Socket(std::move(fd), AF_UNIX, SOCK_STREAM) {}
};

//! \class LocalStreamSocket
//! Example:
//!
//! \include socket_example_3.cc

#endif  // SPONGE_LIBSPONGE_SOCKET_HH
