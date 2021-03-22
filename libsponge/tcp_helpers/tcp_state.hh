#ifndef SPONGE_LIBSPONGE_TCP_STATE
#define SPONGE_LIBSPONGE_TCP_STATE

#include "tcp_receiver.hh"

#include <string>

//! \brief Summary of a TCPConnection's internal state
//!
//! Most TCP implementations have a global per-connection state
//! machine, as described in the [TCP](\ref rfc::rfc793)
//! specification. Sponge is a bit different: we have factored the
//! connection into two independent parts (the sender and the
//! receiver). The TCPSender and TCPReceiver maintain their interval
//! state variables independently (e.g. next_seqno, number of bytes in
//! flight, or whether each stream has ended). There is no notion of a
//! discrete state machine or much overarching state outside the
//! sender and receiver. To test that Sponge follows the TCP spec, we
//! use this class to compare the "official" states with Sponge's
//! sender/receiver states and two variables that belong to the
//! overarching TCPConnection object.
class TCPState {
  public:
    //! \brief Summarize the state of a TCPReceiver in a string
    static std::string state_summary(const TCPReceiver &receiver);
};

namespace TCPReceiverStateSummary {
const std::string ERROR = "error (connection was reset)";
const std::string LISTEN = "waiting for SYN: ackno is empty";
const std::string SYN_RECV = "SYN received (ackno exists), and input to stream hasn't ended";
const std::string FIN_RECV = "input to stream has ended";
}  // namespace TCPReceiverStateSummary

#endif  // SPONGE_LIBSPONGE_TCP_STATE
