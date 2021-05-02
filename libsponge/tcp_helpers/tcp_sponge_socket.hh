#ifndef SPONGE_LIBSPONGE_TCP_SPONGE_SOCKET_HH
#define SPONGE_LIBSPONGE_TCP_SPONGE_SOCKET_HH

#include "byte_stream.hh"
#include "eventloop.hh"
#include "fd_adapter.hh"
#include "file_descriptor.hh"
#include "network_interface.hh"
#include "tcp_config.hh"
#include "tcp_connection.hh"
#include "tuntap_adapter.hh"

#include <atomic>
#include <cstdint>
#include <optional>
#include <thread>
#include <vector>

//! Multithreaded wrapper around TCPConnection that approximates the Unix sockets API
template <typename AdaptT>
class TCPSpongeSocket : public LocalStreamSocket {
  private:
    //! Stream socket for reads and writes between owner and TCP thread
    LocalStreamSocket _thread_data;

  protected:
    //! Adapter to underlying datagram socket (e.g., UDP or IP)
    AdaptT _datagram_adapter;

  private:
    //! Set up the TCPConnection and the event loop
    void _initialize_TCP(const TCPConfig &config);

    //! TCP state machine
    std::optional<TCPConnection> _tcp{};

    //! eventloop that handles all the events (new inbound datagram, new outbound bytes, new inbound bytes)
    EventLoop _eventloop{};

    //! Process events while specified condition is true
    void _tcp_loop(const std::function<bool()> &condition);

    //! Main loop of TCPConnection thread
    void _tcp_main();

    //! Handle to the TCPConnection thread; owner thread calls join() in the destructor
    std::thread _tcp_thread{};

    //! Construct LocalStreamSocket fds from socket pair, initialize eventloop
    TCPSpongeSocket(std::pair<FileDescriptor, FileDescriptor> data_socket_pair, AdaptT &&datagram_interface);

    std::atomic_bool _abort{false};  //!< Flag used by the owner to force the TCPConnection thread to shut down

    bool _inbound_shutdown{false};  //!< Has TCPSpongeSocket shut down the incoming data to the owner?

    bool _outbound_shutdown{false};  //!< Has the owner shut down the outbound data to the TCP connection?

    bool _fully_acked{false};  //!< Has the outbound data been fully acknowledged by the peer?

  public:
    //! Construct from the interface that the TCPConnection thread will use to read and write datagrams
    explicit TCPSpongeSocket(AdaptT &&datagram_interface);

    //! Close socket, and wait for TCPConnection to finish
    //! \note Calling this function is only advisable if the socket has reached EOF,
    //! or else may wait foreever for remote peer to close the TCP connection.
    void wait_until_closed();

    //! Connect using the specified configurations; blocks until connect succeeds or fails
    void connect(const TCPConfig &c_tcp, const FdAdapterConfig &c_ad);

    //! Listen and accept using the specified configurations; blocks until accept succeeds or fails
    void listen_and_accept(const TCPConfig &c_tcp, const FdAdapterConfig &c_ad);

    //! When a connected socket is destructed, it will send a RST
    ~TCPSpongeSocket();

    //! \name
    //! This object cannot be safely moved or copied, since it is in use by two threads simultaneously

    //!@{
    TCPSpongeSocket(const TCPSpongeSocket &) = delete;
    TCPSpongeSocket(TCPSpongeSocket &&) = delete;
    TCPSpongeSocket &operator=(const TCPSpongeSocket &) = delete;
    TCPSpongeSocket &operator=(TCPSpongeSocket &&) = delete;
    //!@}

    //! \name
    //! Some methods of the parent Socket wouldn't work as expected on the TCP socket, so delete them

    //!@{
    void bind(const Address &address) = delete;
    Address local_address() const = delete;
    Address peer_address() const = delete;
    void set_reuseaddr() = delete;
    //!@}
};

using TCPOverUDPSpongeSocket = TCPSpongeSocket<TCPOverUDPSocketAdapter>;
using TCPOverIPv4SpongeSocket = TCPSpongeSocket<TCPOverIPv4OverTunFdAdapter>;
using TCPOverIPv4OverEthernetSpongeSocket = TCPSpongeSocket<TCPOverIPv4OverEthernetAdapter>;

using LossyTCPOverUDPSpongeSocket = TCPSpongeSocket<LossyTCPOverUDPSocketAdapter>;
using LossyTCPOverIPv4SpongeSocket = TCPSpongeSocket<LossyTCPOverIPv4OverTunFdAdapter>;

//! \class TCPSpongeSocket
//! This class involves the simultaneous operation of two threads.
//!
//! One, the "owner" or foreground thread, interacts with this class in much the
//! same way as one would interact with a TCPSocket: it connects or listens, writes to
//! and reads from a reliable data stream, etc. Only the owner thread calls public
//! methods of this class.
//!
//! The other, the "TCPConnection" thread, takes care of the back-end tasks that the kernel would
//! perform for a TCPSocket: reading and parsing datagrams from the wire, filtering out
//! segments unrelated to the connection, etc.
//!
//! There are a few notable differences between the TCPSpongeSocket and TCPSocket interfaces:
//!
//! - a TCPSpongeSocket can only accept a single connection
//! - listen_and_accept() is a blocking function call that acts as both [listen(2)](\ref man2::listen)
//!   and [accept(2)](\ref man2::accept)
//! - if TCPSpongeSocket is destructed while a TCP connection is open, the connection is
//!   immediately terminated with a RST (call `wait_until_closed` to avoid this)

//! Helper class that makes a TCPOverIPv4SpongeSocket behave more like a (kernel) TCPSocket
class CS144TCPSocket : public TCPOverIPv4SpongeSocket {
  public:
    CS144TCPSocket();
    void connect(const Address &address);
};

//! Helper class that makes a TCPOverIPv4overEthernetSpongeSocket behave more like a (kernel) TCPSocket
class FullStackSocket : public TCPOverIPv4OverEthernetSpongeSocket {
  public:
    //! Construct a TCP (stream) socket, using the CS144 TCPConnection object,
    //! that encapsulates TCP segments in IP datagrams, then encapsulates
    //! those IP datagrams in Ethernet frames sent to the Ethernet address of the next hop.
    FullStackSocket();
    void connect(const Address &address);
};

#endif  // SPONGE_LIBSPONGE_TCP_SPONGE_SOCKET_HH
