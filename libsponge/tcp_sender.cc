#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
// #include <iostream>
#include <algorithm>

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
    , _rto{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    if (!_syn_sent) {
        _syn_sent = true;
        TCPSegment seg;
        seg.header().syn = true;
        _send_segment(seg);
        return;
    }
    // If SYN has not been acked, do nothing.
    if (!_segments_outstanding.empty() && _segments_outstanding.front().header().syn)
        return;
    // If _stream is empty but input has not ended, do nothing.
    if (!_stream.buffer_size() && !_stream.eof())
        // Lab4 behavior: if incoming_seg.length_in_sequence_space() is not zero, send ack.
        return;
    if (_fin_sent)
        return;

    if (_receiver_window_size) {
        while (_receiver_free_space) {
            TCPSegment seg;
            size_t payload_size = min({_stream.buffer_size(),
                                       static_cast<size_t>(_receiver_free_space),
                                       static_cast<size_t>(TCPConfig::MAX_PAYLOAD_SIZE)});
            seg.payload() = Buffer{_stream.read(payload_size)};
            if (_stream.eof() && static_cast<size_t>(_receiver_free_space) > payload_size) {
                seg.header().fin = true;
                _fin_sent = true;
            }
            _send_segment(seg);
            if (_stream.buffer_empty())
                break;
        }
    } else if (_receiver_free_space == 0) {
        // The zero-window-detect-segment should only be sent once (retransmition excute by tick function).
        // Before it is sent, _receiver_free_space is zero. Then it will be -1.
        TCPSegment seg;
        if (_stream.eof()) {
            seg.header().fin = true;
            _fin_sent = true;
            _send_segment(seg);
        } else if (!_stream.buffer_empty()) {
            seg.payload() = Buffer{_stream.read(1)};
            _send_segment(seg);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // pop seg from segments_outstanding
    // deduct bytes_inflight
    // reset rto, reset _consecutive_retransmissions
    // reset timer
    // stop timer if bytes_inflight == 0
    // fill window
    // update _receiver_window_size
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    if (!_ack_valid(abs_ackno)) {
        // cout << "invalid ackno!\n";
        return;
    }
    // cout << "ackno " << ackno << " windows_size " << window_size << "\n";
    _receiver_window_size = window_size;
    _receiver_free_space = window_size;
    while (!_segments_outstanding.empty()) {
        TCPSegment seg = _segments_outstanding.front();
        if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= abs_ackno) {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();
            // Do not do the following operations outside while loop.
            // Because if the ack is not corresponding to any segment in the segment_outstanding,
            // we should not restart the timer.
            _time_elapsed = 0;
            _rto = _initial_retransmission_timeout;
            _consecutive_retransmissions = 0;
        } else {
            break;
        }
    }
    if (!_segments_outstanding.empty()) {
        _receiver_free_space = static_cast<uint16_t>(
            abs_ackno + static_cast<uint64_t>(window_size) -
            unwrap(_segments_outstanding.front().header().seqno, _isn, _next_seqno) - _bytes_in_flight);
    }

    // if ((_segments_outstanding.empty() && _bytes_in_flight > 0) ||
    //     (!_segments_outstanding.empty() && _bytes_in_flight == 0)) {
    //     cout << "either bytes_in_flight is 0 or _segments_outstanding is empty, but not both!\n";
    //     return;
    // }
    if (!_bytes_in_flight)
        _timer_running = false;
    // Note that test code will call it again.
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer_running)
        return;
    _time_elapsed += ms_since_last_tick;
    // cout << "time_elapsed " << _time_elapsed << " rto " << _rto << " conti " << _consecutive_retransmissions << "\n";
    if (_time_elapsed >= _rto) {
        _segments_out.push(_segments_outstanding.front());
        if (_receiver_window_size || _segments_outstanding.front().header().syn) {
            ++_consecutive_retransmissions;
            _rto <<= 1;
        }
        _time_elapsed = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

// See test code send_window.cc line 113 why the commented code is wrong.
bool TCPSender::_ack_valid(uint64_t abs_ackno) {
    if (_segments_outstanding.empty())
        return abs_ackno <= _next_seqno;
    return abs_ackno <= _next_seqno &&
           //  abs_ackno >= unwrap(_segments_outstanding.front().header().seqno, _isn, _next_seqno) +
           //          _segments_outstanding.front().length_in_sequence_space();
           abs_ackno >= unwrap(_segments_outstanding.front().header().seqno, _isn, _next_seqno);
}

void TCPSender::_send_segment(TCPSegment &seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    if (_syn_sent)
        _receiver_free_space -= seg.length_in_sequence_space();
    _segments_out.push(seg);
    _segments_outstanding.push(seg);
    if (!_timer_running) {
        _timer_running = true;
        _time_elapsed = 0;
    }
    // cout << "seqno: " << seg.header().seqno;
    // cout << "payload " << seg.payload().str();
    // cout << " receiver_free_space " << _receiver_free_space;
    // cout << " seg_length " << seg.length_in_sequence_space() << "\n";
}