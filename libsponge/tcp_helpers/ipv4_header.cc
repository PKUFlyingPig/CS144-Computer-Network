#include "ipv4_header.hh"

#include "util.hh"

#include <arpa/inet.h>
#include <iomanip>
#include <sstream>

using namespace std;

//! \param[in,out] p is a NetParser from which the IP fields will be extracted
//! \returns a ParseResult indicating success or the reason for failure
//! \details It is important to check for (at least) the following potential errors
//!          (but note that NetParser inherently checks for certain errors;
//!          use that fact to your advantage!):
//!
//! - data stream is too short to contain a header
//! - wrong IP version number
//! - the header's `hlen` field is shorter than the minimum allowed
//! - there is less data in the header than the `doff` field claims
//! - there is less data in the full datagram than the `len` field claims
//! - the checksum is bad
ParseResult IPv4Header::parse(NetParser &p) {
    Buffer original_serialized_version = p.buffer();

    const size_t data_size = p.buffer().size();
    if (data_size < IPv4Header::LENGTH) {
        return ParseResult::PacketTooShort;
    }

    const uint8_t first_byte = p.u8();
    ver = first_byte >> 4;     // version
    hlen = first_byte & 0x0f;  // header length
    tos = p.u8();              // type of service
    len = p.u16();             // length
    id = p.u16();              // id

    const uint16_t fo_val = p.u16();
    df = static_cast<bool>(fo_val & 0x4000);  // don't fragment
    mf = static_cast<bool>(fo_val & 0x2000);  // more fragments
    offset = fo_val & 0x1fff;                 // offset

    ttl = p.u8();     // ttl
    proto = p.u8();   // proto
    cksum = p.u16();  // checksum
    src = p.u32();    // source address
    dst = p.u32();    // destination address

    if (data_size < 4 * hlen) {
        return ParseResult::PacketTooShort;
    }
    if (ver != 4) {
        return ParseResult::WrongIPVersion;
    }
    if (hlen < 5) {
        return ParseResult::HeaderTooShort;
    }
    if (data_size != len) {
        return ParseResult::TruncatedPacket;
    }

    p.remove_prefix(hlen * 4 - IPv4Header::LENGTH);

    if (p.error()) {
        return p.get_error();
    }

    InternetChecksum check;
    check.add({original_serialized_version.str().data(), size_t(4 * hlen)});
    if (check.value()) {
        return ParseResult::BadChecksum;
    }

    return ParseResult::NoError;
}

//! Serialize the IPv4Header to a string (does not recompute the checksum)
string IPv4Header::serialize() const {
    // sanity checks
    if (ver != 4) {
        throw runtime_error("wrong IP version");
    }
    if (4 * hlen < IPv4Header::LENGTH) {
        throw runtime_error("IP header too short");
    }

    string ret;
    ret.reserve(4 * hlen);

    const uint8_t first_byte = (ver << 4) | (hlen & 0xf);
    NetUnparser::u8(ret, first_byte);  // version and header length
    NetUnparser::u8(ret, tos);         // type of service
    NetUnparser::u16(ret, len);        // length
    NetUnparser::u16(ret, id);         // id

    const uint16_t fo_val = (df ? 0x4000 : 0) | (mf ? 0x2000 : 0) | (offset & 0x1fff);
    NetUnparser::u16(ret, fo_val);  // flags and offset

    NetUnparser::u8(ret, ttl);    // time to live
    NetUnparser::u8(ret, proto);  // protocol number

    NetUnparser::u16(ret, cksum);  // checksum

    NetUnparser::u32(ret, src);  // src address
    NetUnparser::u32(ret, dst);  // dst address

    ret.resize(4 * hlen);  // expand header to advertised size

    return ret;
}

uint16_t IPv4Header::payload_length() const { return len - 4 * hlen; }

//! \details This value is needed when computing the checksum of an encapsulated TCP segment.
//! ~~~{.txt}
//!   0      7 8     15 16    23 24    31
//!  +--------+--------+--------+--------+
//!  |          source address           |
//!  +--------+--------+--------+--------+
//!  |        destination address        |
//!  +--------+--------+--------+--------+
//!  |  zero  |protocol|  payload length |
//!  +--------+--------+--------+--------+
//! ~~~
uint32_t IPv4Header::pseudo_cksum() const {
    uint32_t pcksum = (src >> 16) + (src & 0xffff);  // source addr
    pcksum += (dst >> 16) + (dst & 0xffff);          // dest addr
    pcksum += proto;                                 // protocol
    pcksum += payload_length();                      // payload length
    return pcksum;
}

//! \returns A string with the header's contents
std::string IPv4Header::to_string() const {
    stringstream ss{};
    ss << hex << boolalpha << "IP version: " << +ver << '\n'
       << "IP hdr len: " << +hlen << '\n'
       << "IP tos: " << +tos << '\n'
       << "IP dgram len: " << +len << '\n'
       << "IP id: " << +id << '\n'
       << "Flags: df: " << df << " mf: " << mf << '\n'
       << "Offset: " << +offset << '\n'
       << "TTL: " << +ttl << '\n'
       << "Protocol: " << +proto << '\n'
       << "Checksum: " << +cksum << '\n'
       << "Src addr: " << +src << '\n'
       << "Dst addr: " << +dst << '\n';
    return ss.str();
}

std::string IPv4Header::summary() const {
    stringstream ss{};
    ss << hex << boolalpha << "IPv" << +ver << ", "
       << "len=" << +len << ", "
       << "protocol=" << +proto << ", " << (ttl >= 10 ? "" : "ttl=" + ::to_string(ttl) + ", ")
       << "src=" << inet_ntoa({htobe32(src)}) << ", "
       << "dst=" << inet_ntoa({htobe32(dst)});
    return ss.str();
}
