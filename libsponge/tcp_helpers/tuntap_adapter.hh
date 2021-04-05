#ifndef SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH
#define SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH

#include "tcp_over_ip.hh"
#include "tun.hh"

#include <optional>
#include <unordered_map>
#include <utility>

//! \brief A FD adapter for IPv4 datagrams read from and written to a TUN device
class TCPOverIPv4OverTunFdAdapter : public TCPOverIPv4Adapter {
  private:
    TunFD _tun;

  public:
    //! Construct from a TunFD
    explicit TCPOverIPv4OverTunFdAdapter(TunFD &&tun) : _tun(std::move(tun)) {}

    //! Attempts to read and parse an IPv4 datagram containing a TCP segment related to the current connection
    std::optional<TCPSegment> read() {
        InternetDatagram ip_dgram;
        if (ip_dgram.parse(_tun.read()) != ParseResult::NoError) {
            return {};
        }
        return unwrap_tcp_in_ip(ip_dgram);
    }

    //! Creates an IPv4 datagram from a TCP segment and writes it to the TUN device
    void write(TCPSegment &seg) { _tun.write(wrap_tcp_in_ip(seg).serialize()); }

    //! Access the underlying TUN device
    operator TunFD &() { return _tun; }

    //! Access the underlying TUN device
    operator const TunFD &() const { return _tun; }
};

//! Typedef for TCPOverIPv4OverTunFdAdapter
using LossyTCPOverIPv4OverTunFdAdapter = LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>;

#endif  // SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH
