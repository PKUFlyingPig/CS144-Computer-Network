#include "tcp_state.hh"

using namespace std;

bool TCPState::operator==(const TCPState &other) const {
    return _active == other._active and _linger_after_streams_finish == other._linger_after_streams_finish and
           _sender == other._sender and _receiver == other._receiver;
}

bool TCPState::operator!=(const TCPState &other) const { return not operator==(other); }

string TCPState::name() const {
    return "sender=`" + _sender + "`, receiver=`" + _receiver + "`, active=" + to_string(_active) +
           ", linger_after_streams_finish=" + to_string(_linger_after_streams_finish);
}

TCPState::TCPState(const TCPState::State state) {
    switch (state) {
        case TCPState::State::LISTEN:
            _receiver = TCPReceiverStateSummary::LISTEN;
            _sender = TCPSenderStateSummary::CLOSED;
            break;
        case TCPState::State::SYN_RCVD:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::SYN_SENT;
            break;
        case TCPState::State::SYN_SENT:
            _receiver = TCPReceiverStateSummary::LISTEN;
            _sender = TCPSenderStateSummary::SYN_SENT;
            break;
        case TCPState::State::ESTABLISHED:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::SYN_ACKED;
            break;
        case TCPState::State::CLOSE_WAIT:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::SYN_ACKED;
            _linger_after_streams_finish = false;
            break;
        case TCPState::State::LAST_ACK:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_SENT;
            _linger_after_streams_finish = false;
            break;
        case TCPState::State::CLOSING:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_SENT;
            break;
        case TCPState::State::FIN_WAIT_1:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::FIN_SENT;
            break;
        case TCPState::State::FIN_WAIT_2:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::FIN_ACKED;
            break;
        case TCPState::State::TIME_WAIT:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_ACKED;
            break;
        case TCPState::State::RESET:
            _receiver = TCPReceiverStateSummary::ERROR;
            _sender = TCPSenderStateSummary::ERROR;
            _linger_after_streams_finish = false;
            _active = false;
            break;
        case TCPState::State::CLOSED:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_ACKED;
            _linger_after_streams_finish = false;
            _active = false;
            break;
    }
}

TCPState::TCPState(const TCPSender &sender, const TCPReceiver &receiver, const bool active, const bool linger)
    : _sender(state_summary(sender))
    , _receiver(state_summary(receiver))
    , _active(active)
    , _linger_after_streams_finish(active ? linger : false) {}

string TCPState::state_summary(const TCPReceiver &receiver) {
    if (receiver.stream_out().error()) {
        return TCPReceiverStateSummary::ERROR;
    } else if (not receiver.ackno().has_value()) {
        return TCPReceiverStateSummary::LISTEN;
    } else if (receiver.stream_out().input_ended()) {
        return TCPReceiverStateSummary::FIN_RECV;
    } else {
        return TCPReceiverStateSummary::SYN_RECV;
    }
}

string TCPState::state_summary(const TCPSender &sender) {
    if (sender.stream_in().error()) {
        return TCPSenderStateSummary::ERROR;
    } else if (sender.next_seqno_absolute() == 0) {
        return TCPSenderStateSummary::CLOSED;
    } else if (sender.next_seqno_absolute() == sender.bytes_in_flight()) {
        return TCPSenderStateSummary::SYN_SENT;
    } else if (not sender.stream_in().eof()) {
        return TCPSenderStateSummary::SYN_ACKED;
    } else if (sender.next_seqno_absolute() < sender.stream_in().bytes_written() + 2) {
        return TCPSenderStateSummary::SYN_ACKED;
    } else if (sender.bytes_in_flight()) {
        return TCPSenderStateSummary::FIN_SENT;
    } else {
        return TCPSenderStateSummary::FIN_ACKED;
    }
}
