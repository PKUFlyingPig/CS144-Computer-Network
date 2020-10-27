#ifndef SPONGE_LIBSPONGE_ETHERNET_FRAME_HH
#define SPONGE_LIBSPONGE_ETHERNET_FRAME_HH

#include "buffer.hh"
#include "ethernet_header.hh"

//! \brief Ethernet frame
class EthernetFrame {
  private:
    EthernetHeader _header{};
    BufferList _payload{};

  public:
    //! \brief Parse the frame from a string
    ParseResult parse(const Buffer buffer);

    //! \brief Serialize the frame to a string
    BufferList serialize() const;

    //! \name Accessors
    //!@{
    const EthernetHeader &header() const { return _header; }
    EthernetHeader &header() { return _header; }

    const BufferList &payload() const { return _payload; }
    BufferList &payload() { return _payload; }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_ETHERNET_FRAME_HH
