#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received_counter; }

bool TCPConnection::real_send() {
    bool isSend = false;
    while (!_sender.segments_out().empty()) {
        isSend = true;
        TCPSegment segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ack_and_windowsize(segment);
        _segments_out.push(segment);
    }
    return isSend;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received_counter = 0;
    // check if the RST has been set
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }

    // give the segment to receiver
    _receiver.segment_received(seg);

    // check if need to linger
    if (check_inbound_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    // check if the ACK has been set
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        real_send();
    }

    // send ack
    if (seg.length_in_sequence_space() > 0) {
        // handle the SYN/ACK case
        _sender.fill_window();
        bool isSend = real_send();
        // send at least one ack message
        if (!isSend) {
            _sender.send_empty_segment();
            TCPSegment ACKSeg = _sender.segments_out().front();
            _sender.segments_out().pop();
            set_ack_and_windowsize(ACKSeg);
            _segments_out.push(ACKSeg);
        }
    }

    return;
}

bool TCPConnection::active() const { return _active; }

void TCPConnection::set_ack_and_windowsize(TCPSegment &segment) {
    // ask receiver for ack and window size
    optional<WrappingInt32> ackno = _receiver.ackno();
    if (ackno.has_value()) {
        segment.header().ack = true;
        segment.header().ackno = ackno.value();
    }
    size_t window_size = _receiver.window_size();
    segment.header().win = static_cast<uint16_t>(window_size);
    return;
}

void TCPConnection::connect() {
    // send SYN
    _sender.fill_window();
    real_send();
}

size_t TCPConnection::write(const string &data) {
    if (!data.size()) return 0;
    size_t actually_write = _sender.stream_in().write(data);
    _sender.fill_window();
    real_send();
    return actually_write;
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    // cout<<"!!!!!!!!!! end input !!!!!!!!!!"<<endl;
    // cout<<"stream_in : "<< _sender.stream_in().input_ended()<< " " << _sender.stream_in().buffer_empty() << endl;
    // may send FIN
    _sender.fill_window();
    real_send();
}

void TCPConnection::send_RST() {
    _sender.send_empty_segment();
    TCPSegment RSTSeg = _sender.segments_out().front();
    _sender.segments_out().pop();
    set_ack_and_windowsize(RSTSeg);
    RSTSeg.header().rst = true;
    _segments_out.push(RSTSeg);
}

// prereqs1 : The inbound stream has been fully assembled and has ended.
bool TCPConnection::check_inbound_ended() {
    return _receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended();
}
// prereqs2 : The outbound stream has been ended by the local application and fully sent (including
// the fact that it ended, i.e. a segment with fin ) to the remote peer.
// prereqs3 : The outbound stream has been fully acknowledged by the remote peer.
bool TCPConnection::check_outbound_ended() {
    return _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
           _sender.bytes_in_flight() == 0;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received_counter += ms_since_last_tick;
    // tick the sender to do the retransmit
    _sender.tick(ms_since_last_tick);
    // if new retransmit segment generated, send it
    if (_sender.segments_out().size() > 0) {
        TCPSegment retxSeg = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ack_and_windowsize(retxSeg);
        // abort the connection
        if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            retxSeg.header().rst = true;
            _active = false;
        }
        _segments_out.push(retxSeg);
    }

    // // check if need to linger
    // if (check_inbound_ended() && !_sender.stream_in().eof()) {
    //     _linger_after_streams_finish = false;
    // }

    // check if done
    if (check_inbound_ended() && check_outbound_ended()) {
        if (!_linger_after_streams_finish) {
            _active = false;
        } else if (_time_since_last_segment_received_counter >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            send_RST();
            _active = false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
