#ifndef SPONGE_LIBSPONGE_TCP_HEADER_HH
#define SPONGE_LIBSPONGE_TCP_HEADER_HH

#include "parser.hh"
#include "wrapping_integers.hh"

//! \brief [TCP](\ref rfc::rfc793) segment header
//! \note TCP options are not supported
struct TCPHeader {
    static constexpr size_t LENGTH = 20;  //!< [TCP](\ref rfc::rfc793) header length, not including options

    //! \struct TCPHeader
    //! ~~~{.txt}
    //!   0                   1                   2                   3
    //!   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |          Source Port          |       Destination Port        |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |                        Sequence Number                        |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |                    Acknowledgment Number                      |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |  Data |           |U|A|P|R|S|F|                               |
    //!  | Offset| Reserved  |R|C|S|S|Y|I|            Window             |
    //!  |       |           |G|K|H|T|N|N|                               |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |           Checksum            |         Urgent Pointer        |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |                    Options                    |    Padding    |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |                             data                              |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //! ~~~

    //! \name TCP Header fields
    //!@{
    uint16_t sport = 0;         //!< source port
    uint16_t dport = 0;         //!< destination port
    WrappingInt32 seqno{0};     //!< sequence number
    WrappingInt32 ackno{0};     //!< ack number
    uint8_t doff = LENGTH / 4;  //!< data offset
    bool urg = false;           //!< urgent flag
    bool ack = false;           //!< ack flag
    bool psh = false;           //!< push flag
    bool rst = false;           //!< rst flag
    bool syn = false;           //!< syn flag
    bool fin = false;           //!< fin flag
    uint16_t win = 0;           //!< window size
    uint16_t cksum = 0;         //!< checksum
    uint16_t uptr = 0;          //!< urgent pointer
    //!@}

    //! Parse the TCP fields from the provided NetParser
    ParseResult parse(NetParser &p);

    //! Serialize the TCP fields
    std::string serialize() const;

    //! Return a string containing a header in human-readable format
    std::string to_string() const;

    //! Return a string containing a human-readable summary of the header
    std::string summary() const;

    bool operator==(const TCPHeader &other) const;
};

#endif  // SPONGE_LIBSPONGE_TCP_HEADER_HH
