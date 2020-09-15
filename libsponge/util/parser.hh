#ifndef SPONGE_LIBSPONGE_PARSER_HH
#define SPONGE_LIBSPONGE_PARSER_HH

#include "buffer.hh"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

//! The result of parsing or unparsing an IP datagram, TCP segment, Ethernet frame, or ARP message
enum class ParseResult {
    NoError = 0,      //!< Success
    BadChecksum,      //!< Bad checksum
    PacketTooShort,   //!< Not enough data to finish parsing
    WrongIPVersion,   //!< Got a version of IP other than 4
    HeaderTooShort,   //!< Header length is shorter than minimum required
    TruncatedPacket,  //!< Packet length is shorter than header claims
    Unsupported       //!< Packet uses unsupported features
};

//! Output a string representation of a ParseResult
std::string as_string(const ParseResult r);

class NetParser {
  private:
    Buffer _buffer;
    ParseResult _error = ParseResult::NoError;  //!< Result of parsing so far

    //! Check that there is sufficient data to parse the next token
    void _check_size(const size_t size);

    //! Generic integer parsing method (used by u32, u16, u8)
    template <typename T>
    T _parse_int();

  public:
    NetParser(Buffer buffer) : _buffer(buffer) {}

    Buffer buffer() const { return _buffer; }

    //! Get the current value stored in BaseParser::_error
    ParseResult get_error() const { return _error; }

    //! \brief Set BaseParser::_error
    //! \param[in] res is the value to store in BaseParser::_error
    void set_error(ParseResult res) { _error = res; }

    //! Returns `true` if there has been an error
    bool error() const { return get_error() != ParseResult::NoError; }

    //! Parse a 32-bit integer in network byte order from the data stream
    uint32_t u32();

    //! Parse a 16-bit integer in network byte order from the data stream
    uint16_t u16();

    //! Parse an 8-bit integer in network byte order from the data stream
    uint8_t u8();

    //! Remove n bytes from the buffer
    void remove_prefix(const size_t n);
};

struct NetUnparser {
    template <typename T>
    static void _unparse_int(std::string &s, T val);

    //! Write a 32-bit integer into the data stream in network byte order
    static void u32(std::string &s, const uint32_t val);

    //! Write a 16-bit integer into the data stream in network byte order
    static void u16(std::string &s, const uint16_t val);

    //! Write an 8-bit integer into the data stream in network byte order
    static void u8(std::string &s, const uint8_t val);
};

#endif  // SPONGE_LIBSPONGE_PARSER_HH
