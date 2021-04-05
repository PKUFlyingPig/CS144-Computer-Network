#include "fd_adapter.hh"

#include <iostream>
#include <stdexcept>
#include <utility>

using namespace std;

//! \details This function first attempts to parse a TCP segment from the next UDP
//! payload recv()d from the socket.
//!
//! If this succeeds, it then checks that the received segment is related to the
//! current connection. When a TCP connection has been established, this means
//! checking that the source and destination ports in the TCP header are correct.
//!
//! If the TCP FSM is listening (i.e., TCPOverUDPSocketAdapter::_listen is `true`)
//! and the TCP segment read from the wire includes a SYN, this function clears the
//! `_listen` flag and calls calls connect() on the underlying UDP socket, with
//! the result that future outgoing segments go to the sender of the SYN segment.
//! \returns a std::optional<TCPSegment> that is empty if the segment was invalid or unrelated
optional<TCPSegment> TCPOverUDPSocketAdapter::read() {
    auto datagram = _sock.recv();

    // is it for us?
    if (not listening() and (datagram.source_address != config().destination)) {
        return {};
    }

    // is the payload a valid TCP segment?
    TCPSegment seg;
    if (ParseResult::NoError != seg.parse(move(datagram.payload), 0)) {
        return {};
    }

    // should we target this source in all future replies?
    if (listening()) {
        if (seg.header().syn and not seg.header().rst) {
            config_mutable().destination = datagram.source_address;
            set_listening(false);
        } else {
            return {};
        }
    }

    return seg;
}

//! Serialize a TCP segment and send it as the payload of a UDP datagram.
//! \param[in] seg is the TCP segment to write
void TCPOverUDPSocketAdapter::write(TCPSegment &seg) {
    seg.header().sport = config().source.port();
    seg.header().dport = config().destination.port();
    _sock.sendto(config().destination, seg.serialize(0));
}

//! Specialize LossyFdAdapter to TCPOverUDPSocketAdapter
template class LossyFdAdapter<TCPOverUDPSocketAdapter>;
