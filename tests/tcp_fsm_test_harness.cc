#include "tcp_fsm_test_harness.hh"

#include "tcp_expectation.hh"
#include "tcp_header.hh"
#include "util.hh"

#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <utility>

const unsigned int DEFAULT_TEST_WINDOW = 137;

using namespace std;

TestFD::TestFD()
    : TestFD([&]() {
        int fds[2];
        SystemCall("socketpair", socketpair(AF_UNIX, SOCK_SEQPACKET, 0, &fds[0]));
        return make_pair(FileDescriptor(fds[0]), TestRFD(fds[1]));
    }()) {}

TestFD::TestFD(pair<FileDescriptor, TestRFD> fd_pair)
    : FileDescriptor(move(fd_pair.first)), _recv_fd(move(fd_pair.second)) {}

//! \param[in] buffer is the content to write to the TestFD
void TestFD::write(const BufferViewList &buffer) {
    auto iovecs = buffer.as_iovecs();

    msghdr message{};
    message.msg_iov = iovecs.data();
    message.msg_iovlen = iovecs.size();

    SystemCall("sendmsg", ::sendmsg(fd_num(), &message, MSG_EOR));
}

//! \returns `true` if there is a packet available for reading from TestRFD
bool TestFD::TestRFD::can_read() const {
    uint8_t tmp;
    return SystemCall("recv", ::recv(fd_num(), &tmp, 1, MSG_PEEK | MSG_DONTWAIT), EAGAIN) >= 0;
}

//! \returns a vector of bytes received on the TestRFD
string TestFD::TestRFD::read() {
    string ret(MAX_RECV, 0);
    ssize_t ret_read = SystemCall("recv", ::recv(fd_num(), ret.data(), ret.size(), MSG_TRUNC));
    if (ret_read > static_cast<ssize_t>(ret.size())) {
        throw runtime_error("TestFD unexpectedly got truncated packet.");
    }

    ret.resize(ret_read);
    register_read();

    return ret;
}

//! \param[in] seg is the TCPSegment to configure
void TestFdAdapter::config_segment(TCPSegment &seg) {
    const auto &cfg = config();
    auto &tcp_hdr = seg.header();
    tcp_hdr.sport = cfg.source.port();
    tcp_hdr.dport = cfg.destination.port();
}

//! \param[in] seg is the TCPSegment to write
void TestFdAdapter::write(TCPSegment &seg) {
    config_segment(seg);
    TestFD::write(seg.serialize());
}

//! \param[in] seqno is the sequence number of the segment
//! \param[in] ackno is the optional acknowledgment number of the segment; if no value, ACK flag is not set
void TCPTestHarness::send_fin(const WrappingInt32 seqno, const optional<WrappingInt32> ackno) {
    SendSegment step{};
    if (ackno.has_value()) {
        step.with_ack(true).with_ackno(ackno.value());
    }
    step.with_fin(true).with_seqno(seqno).with_win(DEFAULT_TEST_WINDOW);
    execute(step);
}

//! \param[in] seqno is the sequence number of the segment
//! \param[in] ackno is the acknowledgment number of the segment
//! \param[in] swin is the optional window size for the segment; if no value, uses default value (137 bytes)
void TCPTestHarness::send_ack(const WrappingInt32 seqno, const WrappingInt32 ackno, const optional<uint16_t> swin) {
    uint16_t win = DEFAULT_TEST_WINDOW;
    if (swin.has_value()) {
        win = swin.value();
    }
    execute(SendSegment{}.with_ack(true).with_ackno(ackno).with_seqno(seqno).with_win(win));
}

//! \param[in] seqno is the sequence number of the segment
//! \param[in] ackno is the optional acknowledgment number of the segment; if no value, ACK flag is not set
void TCPTestHarness::send_rst(const WrappingInt32 seqno, const optional<WrappingInt32> ackno) {
    SendSegment step{};
    if (ackno.has_value()) {
        step.with_ack(true).with_ackno(ackno.value());
    }
    step.with_rst(true).with_seqno(seqno).with_win(DEFAULT_TEST_WINDOW);
    execute(step);
}

//! \param[in] seqno is the sequence number of the segment
//! \param[in] ackno is the optional acknowledgment number of the segment; if no value, ACK flag is not set
void TCPTestHarness::send_syn(const WrappingInt32 seqno, const optional<WrappingInt32> ackno) {
    SendSegment step{};
    if (ackno.has_value()) {
        step.with_ack(true).with_ackno(ackno.value());
    }
    step.with_syn(true).with_seqno(seqno).with_win(DEFAULT_TEST_WINDOW);
    execute(step);
}

//! \param[in] seqno is the sequence number of the segment
//! \param[in] ackno is the optional acknowledgment number of the segment; if no value, ACK flag is not set
//! \param[in] val is the value of the one-byte payload
void TCPTestHarness::send_byte(const WrappingInt32 seqno, const optional<WrappingInt32> ackno, const uint8_t val) {
    SendSegment step{};
    if (ackno.has_value()) {
        step.with_ack(true).with_ackno(ackno.value());
    }
    step.with_payload_size(1).with_data(string{&val, &val + 1}).with_seqno(seqno).with_win(DEFAULT_TEST_WINDOW);
    execute(step);
}

//! \param[in] seqno is the sequence number of the segment
//! \param[in] ackno is the acknowledgment number of the segment
//! \param[in] begin is an iterator to the start of the payload
//! \param[in] end is an iterator to the end of the payload
void TCPTestHarness::send_data(const WrappingInt32 seqno, const WrappingInt32 ackno, VecIterT begin, VecIterT end) {
    SendSegment step{};
    execute(SendSegment{}
                .with_ack(true)
                .with_ackno(ackno)
                .with_payload_size(1)
                .with_data(string{begin, end})
                .with_seqno(seqno)
                .with_win(DEFAULT_TEST_WINDOW));
}

void TCPTestHarness::execute(const TCPTestStep &step, std::string note) {
    try {
        step.execute(*this);
        while (not _fsm.segments_out().empty()) {
            _flt.write(_fsm.segments_out().front());
            _fsm.segments_out().pop();
        }
        _steps_executed.emplace_back(step.to_string());
    } catch (const TCPExpectationViolation &e) {
        cerr << "Test Failure on expectation:\n\t" << step.to_string();
        cerr << "\n\nFailure message:\n\t" << e.what();
        cerr << "\n\nList of steps that executed successfully:";
        for (const string &s : _steps_executed) {
            cerr << "\n\t" << s;
        }
        cerr << endl << endl;
        if (note.size() > 0) {
            cerr << "Note:\n\t" << note << endl << endl;
        }
        throw e;
    }
}

TCPSegment TCPTestHarness::expect_seg(const ExpectSegment &expectation, std::string note) {
    try {
        auto ret = expectation.expect_seg(*this);
        _steps_executed.emplace_back(expectation.to_string());
        return ret;
    } catch (const TCPExpectationViolation &e) {
        cerr << "Test Failure on expectation:\n\t" << expectation.description() << "\nFailure message:\n\t" << e.what();
        cerr << "\n\nList of steps that executed successfully:";
        for (const string &s : _steps_executed) {
            cerr << "\n\t" << s;
        }
        cerr << endl << endl;
        if (note.size() > 0) {
            cerr << "Note:\n\t" << note << endl << endl;
        }
        throw e;
    }
}

//! Create a FSM in the "LISTEN" state.
TCPTestHarness TCPTestHarness::in_listen(const TCPConfig &cfg) {
    TCPTestHarness h{cfg};
    h.execute(Listen{});
    return h;
}

//! \brief Create an FSM which has sent a SYN.
//! \details The SYN has been consumed, but not ACK'd by the test harness
//! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
//!            seqno for the SYN.
TCPTestHarness TCPTestHarness::in_syn_sent(const TCPConfig &cfg, const WrappingInt32 tx_isn) {
    TCPConfig c{cfg};
    c.fixed_isn = tx_isn;
    TCPTestHarness h{c};
    h.execute(Connect{});
    h.execute(ExpectOneSegment{}.with_no_flags().with_syn(true).with_seqno(tx_isn).with_payload_size(0));
    return h;
}

//! \brief Create an FSM with an established connection
//! \details The mahine has sent and received a SYN, and both SYNs have been ACK'd
//! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
//!            seqno for the SYN.
//! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
//!            seqno for the SYN.
TCPTestHarness TCPTestHarness::in_established(const TCPConfig &cfg,
                                              const WrappingInt32 tx_isn,
                                              const WrappingInt32 rx_isn) {
    TCPTestHarness h = in_syn_sent(cfg, tx_isn);
    // It has sent a SYN with nothing else, and that SYN has been consumed
    // We reply with ACK and SYN.
    h.send_syn(rx_isn, tx_isn + 1);
    h.execute(ExpectOneSegment{}.with_no_flags().with_ack(true).with_ackno(rx_isn + 1).with_payload_size(0));
    return h;
}

//! \brief Create an FSM in CLOSE_WAIT
//! \details SYNs have been traded, and then the machine received and ACK'd FIN.
//! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
//!            seqno for the SYN.
//! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
//!            seqno for the SYN.
TCPTestHarness TCPTestHarness::in_close_wait(const TCPConfig &cfg,
                                             const WrappingInt32 tx_isn,
                                             const WrappingInt32 rx_isn) {
    TCPTestHarness h = in_established(cfg, tx_isn, rx_isn);
    h.send_fin(rx_isn + 1, tx_isn + 1);
    h.execute(ExpectOneSegment{}.with_no_flags().with_ack(true).with_ackno(rx_isn + 2));
    return h;
}

//! \brief Create an FSM in LAST_ACK
//! \details SYNs have been traded, then the machine received and ACK'd FIN, and then it sent its own FIN.
//! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
//!            seqno for the SYN.
//! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
//!            seqno for the SYN.
TCPTestHarness TCPTestHarness::in_last_ack(const TCPConfig &cfg,
                                           const WrappingInt32 tx_isn,
                                           const WrappingInt32 rx_isn) {
    TCPTestHarness h = in_close_wait(cfg, tx_isn, rx_isn);
    h.execute(Close{});
    h.execute(
        ExpectOneSegment{}.with_no_flags().with_fin(true).with_ack(true).with_seqno(tx_isn + 1).with_ackno(rx_isn + 2));
    return h;
}

//! \brief Create an FSM in FIN_WAIT
//! \details SYNs have been traded, then the TCP sent FIN.
//!          No payload was exchanged.
//! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
//!            seqno for the SYN.
//! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
//!            seqno for the SYN.
TCPTestHarness TCPTestHarness::in_fin_wait_1(const TCPConfig &cfg,
                                             const WrappingInt32 tx_isn,
                                             const WrappingInt32 rx_isn) {
    TCPTestHarness h = in_established(cfg, tx_isn, rx_isn);
    h.execute(Close{});
    h.execute(
        ExpectOneSegment{}.with_no_flags().with_fin(true).with_ack(true).with_ackno(rx_isn + 1).with_seqno(tx_isn + 1));
    return h;
}

//! \brief Create an FSM in FIN_WAIT_2
//! \details SYNs have been traded, then the TCP sent FIN, which was ACK'd
//!          No payload was exchanged.
//! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
//!            seqno for the SYN.
//! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
//!            seqno for the SYN.
TCPTestHarness TCPTestHarness::in_fin_wait_2(const TCPConfig &cfg,
                                             const WrappingInt32 tx_isn,
                                             const WrappingInt32 rx_isn) {
    TCPTestHarness h = in_fin_wait_1(cfg, tx_isn, rx_isn);
    h.send_ack(rx_isn + 1, tx_isn + 2);
    return h;
}

//! \brief Create an FSM in CLOSING
//! \details SYNs have been traded, then the TCP sent FIN, then received FIN
//!          No payload was exchanged.
//! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
//!            seqno for the SYN.
//! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
//!            seqno for the SYN.
TCPTestHarness TCPTestHarness::in_closing(const TCPConfig &cfg,
                                          const WrappingInt32 tx_isn,
                                          const WrappingInt32 rx_isn) {
    TCPTestHarness h = in_fin_wait_1(cfg, tx_isn, rx_isn);
    h.send_fin(rx_isn + 1, tx_isn + 1);
    h.execute(ExpectOneSegment{}.with_no_flags().with_ack(true).with_ackno(rx_isn + 2));
    return h;
}

//! \brief Create an FSM in TIME_WAIT
//! \details SYNs have been traded, then the TCP sent FIN, then received FIN/ACK, and ACK'd.
//!          No payload was exchanged.
//! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
//!            seqno for the SYN.
//! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
//!            seqno for the SYN.
TCPTestHarness TCPTestHarness::in_time_wait(const TCPConfig &cfg,
                                            const WrappingInt32 tx_isn,
                                            const WrappingInt32 rx_isn) {
    TCPTestHarness h = in_fin_wait_1(cfg, tx_isn, rx_isn);
    h.send_fin(rx_isn + 1, tx_isn + 2);
    h.execute(ExpectOneSegment{}.with_no_flags().with_ack(true).with_ackno(rx_isn + 2));
    return h;
}
