#include "tcp_state.hh"

using namespace std;

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
