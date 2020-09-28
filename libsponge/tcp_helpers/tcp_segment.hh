#ifndef SPONGE_LIBSPONGE_TCP_SEGMENT_HH
#define SPONGE_LIBSPONGE_TCP_SEGMENT_HH

#include "buffer.hh"
#include "tcp_header.hh"

#include <cstdint>

//! \brief [TCP](\ref rfc::rfc793) segment
class TCPSegment {
  private:
    TCPHeader _header{};
    Buffer _payload{};

  public:
    //! \brief Parse the segment from a string
    ParseResult parse(const Buffer buffer, const uint32_t datagram_layer_checksum = 0);

    //! \brief Serialize the segment to a string
    BufferList serialize(const uint32_t datagram_layer_checksum = 0) const;

    //! \name Accessors
    //!@{
    const TCPHeader &header() const { return _header; }
    TCPHeader &header() { return _header; }

    const Buffer &payload() const { return _payload; }
    Buffer &payload() { return _payload; }
    //!@}

    //! \brief Segment's length in sequence space
    //! \note Equal to payload length plus one byte if SYN is set, plus one byte if FIN is set
    size_t length_in_sequence_space() const;
};

#endif  // SPONGE_LIBSPONGE_TCP_SEGMENT_HH
