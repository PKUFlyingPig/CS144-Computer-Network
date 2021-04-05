#ifndef SPONGE_LIBSPONGE_TCP_OVER_IP_HH
#define SPONGE_LIBSPONGE_TCP_OVER_IP_HH

#include "buffer.hh"
#include "fd_adapter.hh"
#include "ipv4_datagram.hh"
#include "tcp_segment.hh"

#include <optional>

//! \brief A converter from TCP segments to serialized IPv4 datagrams
class TCPOverIPv4Adapter : public FdAdapterBase {
  public:
    std::optional<TCPSegment> unwrap_tcp_in_ip(const InternetDatagram &ip_dgram);

    InternetDatagram wrap_tcp_in_ip(TCPSegment &seg);
};

#endif  // SPONGE_LIBSPONGE_TCP_OVER_IP_HH
