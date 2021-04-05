#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

class Alarm {
  private:
    //! whether the alarm has been started
    bool alive;

    //! the passed time from the start of this alarm
    size_t counter;

    //! after time_limit, the alarm will expire 
    unsigned int time_limit;

  public:
    //! initilizer
    Alarm (unsigned int RTO):alive(false), counter(0), time_limit(RTO){};

    //! start the alarm
    void start(unsigned int RTO) {
      time_limit = RTO;
      alive = true;
      counter = 0;
    }

    //! tick the alarm
    void tick(const size_t ms_since_last_tick) {
      if (!alive) return;
      counter += ms_since_last_tick;
    }

    //! stop the alarm
    void stop() {
      alive = false;
      counter = 0;
    }

    bool isStarted() {
      return alive;
    }
    bool isExpired() {
      return alive && (counter >= time_limit);
    }
};


//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:

    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};


    //! the retransmission timier
    Alarm _alarm;

    //! recorded right edge of the receiver's window (absolute seqno)
    uint16_t _rwindow;

    //! receiver's window size
    uint16_t _recx_windowsize;

    //! received acknos from the receiver
    uint16_t _acknos;

    //! outstanding segments waiting for acknowledgements
    std::queue<TCPSegment> _outstanding{};

    //! current retransmission timeout number (RTO)
    unsigned int _RTO;

    //! consecutive retransmissions
    unsigned int _conse_retrans;

    //! SYN sent or not
    bool _synSend;

    //! FIN sent or not
    bool _finSend;

    //! remove the acknowledged segments
    void remove_ack(const uint64_t ackno);

    //! send given TCP segment
    void send(const TCPSegment& segment); 

    //! next absolute sequence number
    uint64_t next_abs_seqno() {
      return _next_seqno;
    }

    //! next relative sequence number
    WrappingInt32 next_rela_seqno() {
      return wrap(_next_seqno, _isn);
    }
    
    public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
