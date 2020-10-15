#ifndef SPONGE_LIBSPONGE_IPV4_HEADER_HH
#define SPONGE_LIBSPONGE_IPV4_HEADER_HH

#include "parser.hh"

//! \brief [IPv4](\ref rfc::rfc791) Internet datagram header
//! \note IP options are not supported
struct IPv4Header {
    static constexpr size_t LENGTH = 20;         //!< [IPv4](\ref rfc::rfc791) header length, not including options
    static constexpr uint8_t DEFAULT_TTL = 128;  //!< A reasonable default TTL value
    static constexpr uint8_t PROTO_TCP = 6;      //!< Protocol number for [tcp](\ref rfc::rfc793)

    //! \struct IPv4Header
    //! ~~~{.txt}
    //!   0                   1                   2                   3
    //!   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |Version|  IHL  |Type of Service|          Total Length         |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |         Identification        |Flags|      Fragment Offset    |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |  Time to Live |    Protocol   |         Header Checksum       |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |                       Source Address                          |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |                    Destination Address                        |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |                    Options                    |    Padding    |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //! ~~~

    //! \name IPv4 Header fields
    //!@{
    uint8_t ver = 4;            //!< IP version
    uint8_t hlen = LENGTH / 4;  //!< header length (multiples of 32 bits)
    uint8_t tos = 0;            //!< type of service
    uint16_t len = 0;           //!< total length of packet
    uint16_t id = 0;            //!< identification number
    bool df = true;             //!< don't fragment flag
    bool mf = false;            //!< more fragments flag
    uint16_t offset = 0;        //!< fragment offset field
    uint8_t ttl = DEFAULT_TTL;  //!< time to live field
    uint8_t proto = PROTO_TCP;  //!< protocol field
    uint16_t cksum = 0;         //!< checksum field
    uint32_t src = 0;           //!< src address
    uint32_t dst = 0;           //!< dst address
    //!@}

    //! Parse the IP fields from the provided NetParser
    ParseResult parse(NetParser &p);

    //! Serialize the IP fields
    std::string serialize() const;

    //! Length of the payload
    uint16_t payload_length() const;

    //! [pseudo-header's](\ref rfc::rfc793) contribution to the TCP checksum
    uint32_t pseudo_cksum() const;

    //! Return a string containing a header in human-readable format
    std::string to_string() const;

    //! Return a string containing a human-readable summary of the header
    std::string summary() const;
};

//! \struct IPv4Header
//! This struct can be used to parse an existing IP header or to create a new one.

#endif  // SPONGE_LIBSPONGE_IPV4_HEADER_HH
