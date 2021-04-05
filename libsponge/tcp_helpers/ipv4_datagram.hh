#ifndef SPONGE_LIBSPONGE_IPV4_DATAGRAM_HH
#define SPONGE_LIBSPONGE_IPV4_DATAGRAM_HH

#include "buffer.hh"
#include "ipv4_header.hh"

//! \brief [IPv4](\ref rfc::rfc791) Internet datagram
class IPv4Datagram {
  private:
    IPv4Header _header{};
    BufferList _payload{};

  public:
    //! \brief Parse the segment from a string
    ParseResult parse(const Buffer buffer);

    //! \brief Serialize the segment to a string
    BufferList serialize() const;

    //! \name Accessors
    //!@{
    const IPv4Header &header() const { return _header; }
    IPv4Header &header() { return _header; }

    const BufferList &payload() const { return _payload; }
    BufferList &payload() { return _payload; }
    //!@}
};

using InternetDatagram = IPv4Datagram;

#endif  // SPONGE_LIBSPONGE_IPV4_DATAGRAM_HH
