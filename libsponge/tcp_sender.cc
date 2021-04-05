#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

#include <algorithm>

#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _next_seqno(0)
    , _alarm(retx_timeout)
    , _rwindow(1)
    , _recx_windowsize(1)
    , _acknos(0)
    , _RTO(retx_timeout)
    , _conse_retrans(0)
    , _synSend(false)
    , _finSend(false){}

uint64_t TCPSender::bytes_in_flight() const {
    return _next_seqno - _acknos;
}

void TCPSender::send(const TCPSegment& segment) {
    _next_seqno += segment.length_in_sequence_space();
    _segments_out.push(segment);
    if (segment.length_in_sequence_space() > 0 ) {
        // only store segments which consume some space in the seqno space
        _outstanding.push(segment);
        // if the segment contains data and the alarm has not been started, then start it
        // it will expired after _RTO time
        if (!_alarm.isStarted())
            _alarm.start(_RTO);
    }
}

void TCPSender::send_SYN() {
    TCPSegment segment;
    segment.header().seqno = next_rela_seqno();
    segment.header().syn = true;
    _synSend = true;
    send(segment);
}

void TCPSender::fill_window() {
    if (_finSend) return;
    // no SYN sent
    if (_next_seqno == 0 && !_synSend) 
        send_SYN();
    
    int remainWindowSize = 0;
    if (_rwindow >= next_abs_seqno()) {
        remainWindowSize = _rwindow - next_abs_seqno();
    } else return;

    if (_recx_windowsize == 0 && remainWindowSize == 0) remainWindowSize = 1;
    while (remainWindowSize > 0) {
        TCPSegment segment;

        // set sequence number
        segment.header().seqno = next_rela_seqno();

        // prepare the payload
        size_t len_can_read = min(int(TCPConfig::MAX_PAYLOAD_SIZE), remainWindowSize); // can not overflow the window
        size_t payload_len = len_can_read - segment.header().syn; // SYN also consumes window space
        payload_len = min(_stream.buffer_size(), payload_len); // read at most all the data in _stream_in()
        remainWindowSize -= (payload_len + segment.header().syn);
        Buffer buffer(_stream.read(payload_len));
        segment.payload() = buffer;

        // send FIN
        if (_stream.eof() && !_finSend && remainWindowSize > 0) {
            remainWindowSize--;
            segment.header().fin = true;
            _finSend = true;
        }

        if (segment.length_in_sequence_space() > 0) 
            send(segment);
        else 
            break;
    }
}

void TCPSender::remove_ack(const uint64_t ackno) {
    while (!_outstanding.empty()) {
        TCPSegment earliest = _outstanding.front();
        uint64_t absseq = unwrap(earliest.header().seqno, _isn, _acknos);
        if (ackno >= absseq + earliest.length_in_sequence_space()) _outstanding.pop();
        else break;
    }
    // all outstanding data has been acknowledged, stop the alarm
    if (_outstanding.empty()) {
        _alarm.stop();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t absack = unwrap(ackno, _isn, _acknos);
    _recx_windowsize = window_size;
    if (absack > _acknos) {
        // new data has been acknowledged
        _acknos = absack;
        remove_ack(absack);
        _RTO = _initial_retransmission_timeout;
        if (!_outstanding.empty()) {
            _alarm.start(_RTO);
        }
        _conse_retrans = 0;
    }
    if (_rwindow < absack + window_size) {
        _rwindow = absack + window_size;
    } 
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _alarm.tick(ms_since_last_tick);
    if (_alarm.isExpired()) {
        // retransmit the earliest segment
        // since the alarm is alive, the _outstanding queue can not be empty
        TCPSegment segment = _outstanding.front();
        // resend the TCP segment
        _segments_out.push(segment);
        if (_recx_windowsize > 0) {
            _conse_retrans++;
            _RTO *= 2;
        }
        _alarm.start(_RTO);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _conse_retrans;
}

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_rela_seqno();
    _segments_out.push(segment);
}
