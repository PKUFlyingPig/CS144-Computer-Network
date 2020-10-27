#include "ethernet_frame.hh"

#include "parser.hh"
#include "util.hh"

#include <stdexcept>
#include <string>

using namespace std;

ParseResult EthernetFrame::parse(const Buffer buffer) {
    NetParser p{buffer};
    _header.parse(p);
    _payload = p.buffer();

    return p.get_error();
}

BufferList EthernetFrame::serialize() const {
    BufferList ret;
    ret.append(_header.serialize());
    ret.append(_payload);
    return ret;
}
