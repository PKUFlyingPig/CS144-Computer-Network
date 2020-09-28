#include "tcp_header.hh"

#include <sstream>

using namespace std;

//! \param[in,out] p is a NetParser from which the TCP fields will be extracted
//! \returns a ParseResult indicating success or the reason for failure
//! \details It is important to check for (at least) the following potential errors
//!          (but note that NetParser inherently checks for certain errors;
//!          use that fact to your advantage!):
//!
//! - data stream inside the NetParser is too short to contain a header
//! - the header's `doff` field is shorter than the minimum allowed
//! - there is less data in the header than the `doff` field claims
//! - the checksum is bad
ParseResult TCPHeader::parse(NetParser &p) {
    sport = p.u16();                 // source port
    dport = p.u16();                 // destination port
    seqno = WrappingInt32{p.u32()};  // sequence number
    ackno = WrappingInt32{p.u32()};  // ack number
    doff = p.u8() >> 4;              // data offset

    const uint8_t fl_b = p.u8();                  // byte including flags
    urg = static_cast<bool>(fl_b & 0b0010'0000);  // binary literals and ' digit separator since C++14!!!
    ack = static_cast<bool>(fl_b & 0b0001'0000);
    psh = static_cast<bool>(fl_b & 0b0000'1000);
    rst = static_cast<bool>(fl_b & 0b0000'0100);
    syn = static_cast<bool>(fl_b & 0b0000'0010);
    fin = static_cast<bool>(fl_b & 0b0000'0001);

    win = p.u16();    // window size
    cksum = p.u16();  // checksum
    uptr = p.u16();   // urgent pointer

    if (doff < 5) {
        return ParseResult::HeaderTooShort;
    }

    // skip any options or anything extra in the header
    p.remove_prefix(doff * 4 - TCPHeader::LENGTH);

    if (p.error()) {
        return p.get_error();
    }

    return ParseResult::NoError;
}

//! Serialize the TCPHeader to a string (does not recompute the checksum)
string TCPHeader::serialize() const {
    // sanity check
    if (doff < 5) {
        throw runtime_error("TCP header too short");
    }

    string ret;
    ret.reserve(4 * doff);

    NetUnparser::u16(ret, sport);              // source port
    NetUnparser::u16(ret, dport);              // destination port
    NetUnparser::u32(ret, seqno.raw_value());  // sequence number
    NetUnparser::u32(ret, ackno.raw_value());  // ack number
    NetUnparser::u8(ret, doff << 4);           // data offset

    const uint8_t fl_b = (urg ? 0b0010'0000 : 0) | (ack ? 0b0001'0000 : 0) | (psh ? 0b0000'1000 : 0) |
                         (rst ? 0b0000'0100 : 0) | (syn ? 0b0000'0010 : 0) | (fin ? 0b0000'0001 : 0);
    NetUnparser::u8(ret, fl_b);  // flags
    NetUnparser::u16(ret, win);  // window size

    NetUnparser::u16(ret, cksum);  // checksum

    NetUnparser::u16(ret, uptr);  // urgent pointer

    ret.resize(4 * doff);  // expand header to advertised size

    return ret;
}

//! \returns A string with the header's contents
string TCPHeader::to_string() const {
    stringstream ss{};
    ss << hex << boolalpha << "TCP source port: " << +sport << '\n'
       << "TCP dest port: " << +dport << '\n'
       << "TCP seqno: " << seqno << '\n'
       << "TCP ackno: " << ackno << '\n'
       << "TCP doff: " << +doff << '\n'
       << "Flags: urg: " << urg << " ack: " << ack << " psh: " << psh << " rst: " << rst << " syn: " << syn
       << " fin: " << fin << '\n'
       << "TCP winsize: " << +win << '\n'
       << "TCP cksum: " << +cksum << '\n'
       << "TCP uptr: " << +uptr << '\n';
    return ss.str();
}

string TCPHeader::summary() const {
    stringstream ss{};
    ss << "Header(flags=" << (syn ? "S" : "") << (ack ? "A" : "") << (rst ? "R" : "") << (fin ? "F" : "")
       << ",seqno=" << seqno << ",ack=" << ackno << ",win=" << win << ")";
    return ss.str();
}

bool TCPHeader::operator==(const TCPHeader &other) const {
    // TODO(aozdemir) more complete check (right now we omit cksum, src, dst
    return seqno == other.seqno && ackno == other.ackno && doff == other.doff && urg == other.urg && ack == other.ack &&
           psh == other.psh && rst == other.rst && syn == other.syn && fin == other.fin && win == other.win &&
           uptr == other.uptr;
}
