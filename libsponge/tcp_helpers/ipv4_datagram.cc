#include "ipv4_datagram.hh"

#include "parser.hh"
#include "util.hh"

#include <stdexcept>
#include <string>

using namespace std;

ParseResult IPv4Datagram::parse(const Buffer buffer) {
    NetParser p{buffer};
    _header.parse(p);
    _payload = p.buffer();

    if (_payload.size() != _header.payload_length()) {
        return ParseResult::PacketTooShort;
    }

    return p.get_error();
}

BufferList IPv4Datagram::serialize() const {
    if (_payload.size() != _header.payload_length()) {
        throw runtime_error("IPv4Datagram::serialize: payload is wrong size");
    }

    IPv4Header header_out = _header;
    header_out.cksum = 0;
    const string header_zero_checksum = header_out.serialize();

    // calculate checksum -- taken over header only
    InternetChecksum check;
    check.add(header_zero_checksum);
    header_out.cksum = check.value();

    BufferList ret;
    ret.append(header_out.serialize());
    ret.append(_payload);
    return ret;
}
