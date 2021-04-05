#ifndef SPONGE_LIBSPONGE_TCP_STATE
#define SPONGE_LIBSPONGE_TCP_STATE

#include "tcp_receiver.hh"
#include "tcp_sender.hh"

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
  private:
    std::string _sender{};
    std::string _receiver{};
    bool _active{true};
    bool _linger_after_streams_finish{true};

  public:
    bool operator==(const TCPState &other) const;
    bool operator!=(const TCPState &other) const;

    //! \brief Official state names from the [TCP](\ref rfc::rfc793) specification
    enum class State {
        LISTEN = 0,   //!< Listening for a peer to connect
        SYN_RCVD,     //!< Got the peer's SYN
        SYN_SENT,     //!< Sent a SYN to initiate a connection
        ESTABLISHED,  //!< Three-way handshake complete
        CLOSE_WAIT,   //!< Remote side has sent a FIN, connection is half-open
        LAST_ACK,     //!< Local side sent a FIN from CLOSE_WAIT, waiting for ACK
        FIN_WAIT_1,   //!< Sent a FIN to the remote side, not yet ACK'd
        FIN_WAIT_2,   //!< Received an ACK for previously-sent FIN
        CLOSING,      //!< Received a FIN just after we sent one
        TIME_WAIT,    //!< Both sides have sent FIN and ACK'd, waiting for 2 MSL
        CLOSED,       //!< A connection that has terminated normally
        RESET,        //!< A connection that terminated abnormally
    };

    //! \brief Summarize the TCPState in a string
    std::string name() const;

    //! \brief Construct a TCPState given a sender, a receiver, and the TCPConnection's active and linger bits
    TCPState(const TCPSender &sender, const TCPReceiver &receiver, const bool active, const bool linger);

    //! \brief Construct a TCPState that corresponds to one of the "official" TCP state names
    TCPState(const TCPState::State state);

    //! \brief Summarize the state of a TCPReceiver in a string
    static std::string state_summary(const TCPReceiver &receiver);

    //! \brief Summarize the state of a TCPSender in a string
    static std::string state_summary(const TCPSender &receiver);
};

namespace TCPReceiverStateSummary {
const std::string ERROR = "error (connection was reset)";
const std::string LISTEN = "waiting for SYN: ackno is empty";
const std::string SYN_RECV = "SYN received (ackno exists), and input to stream hasn't ended";
const std::string FIN_RECV = "input to stream has ended";
}  // namespace TCPReceiverStateSummary

namespace TCPSenderStateSummary {
const std::string ERROR = "error (connection was reset)";
const std::string CLOSED = "waiting for stream to begin (no SYN sent)";
const std::string SYN_SENT = "stream started but nothing acknowledged";
const std::string SYN_ACKED = "stream ongoing";
const std::string FIN_SENT = "stream finished (FIN sent) but not fully acknowledged";
const std::string FIN_ACKED = "stream finished and fully acknowledged";
}  // namespace TCPSenderStateSummary

#endif  // SPONGE_LIBSPONGE_TCP_STATE
