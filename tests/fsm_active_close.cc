#include "tcp_config.hh"
#include "tcp_expectation.hh"
#include "tcp_fsm_test_harness.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;
using State = TCPTestHarness::State;

int main() {
    try {
        TCPConfig cfg{};

        // test #1: start in TIME_WAIT, timeout
        {
            TCPTestHarness test_1 = TCPTestHarness::in_time_wait(cfg);

            test_1.execute(Tick(10 * cfg.rt_timeout - 1));

            test_1.execute(ExpectState{State::TIME_WAIT});

            test_1.execute(Tick(1));

            test_1.execute(ExpectNotInState{State::TIME_WAIT});

            test_1.execute(Tick(10 * cfg.rt_timeout));

            test_1.execute(ExpectState{State::CLOSED});
        }

        // test #2: start in CLOSING, send ack, time out
        {
            TCPTestHarness test_2 = TCPTestHarness::in_closing(cfg);

            test_2.execute(Tick(4 * cfg.rt_timeout));
            test_2.execute(ExpectOneSegment{}.with_fin(true));

            test_2.execute(ExpectState{State::CLOSING});
            test_2.send_ack(WrappingInt32{2}, WrappingInt32{2});
            test_2.execute(ExpectNoSegment{});

            test_2.execute(ExpectState{State::TIME_WAIT});

            test_2.execute(Tick(10 * cfg.rt_timeout - 1));

            test_2.execute(ExpectState{State::TIME_WAIT});

            test_2.execute(Tick(2));

            test_2.execute(ExpectState{State::CLOSED});
        }

        // test #3: start in FIN_WAIT_2, send FIN, time out
        {
            TCPTestHarness test_3 = TCPTestHarness::in_fin_wait_2(cfg);

            test_3.execute(Tick(4 * cfg.rt_timeout));

            test_3.execute(ExpectState{State::FIN_WAIT_2});

            const WrappingInt32 rx_seqno{1};
            test_3.send_fin(rx_seqno, WrappingInt32{2});
            const auto ack_expect = rx_seqno + 1;
            test_3.execute(Tick(1));

            test_3.execute(ExpectOneSegment{}.with_ack(true).with_ackno(ack_expect),
                           "test 3 failed: wrong ACK for FIN");

            test_3.execute(ExpectState{State::TIME_WAIT});

            test_3.execute(Tick(10 * cfg.rt_timeout));

            test_3.execute(ExpectState{State::CLOSED});
        }

        // test #4: start in FIN_WAIT_1, ack, FIN, time out
        {
            TCPTestHarness test_4 = TCPTestHarness::in_fin_wait_1(cfg);

            // Expect retransmission of FIN
            test_4.execute(Tick(4 * cfg.rt_timeout));
            test_4.execute(ExpectOneSegment{}.with_fin(true));

            // ACK the FIN
            const WrappingInt32 rx_seqno{1};
            test_4.send_ack(rx_seqno, WrappingInt32{2});
            test_4.execute(Tick(5));

            // Send our own FIN
            test_4.send_fin(rx_seqno, WrappingInt32{2});
            const auto ack_expect = rx_seqno + 1;
            test_4.execute(Tick(1));

            test_4.execute(ExpectOneSegment{}.with_no_flags().with_ack(true).with_ackno(ack_expect));

            test_4.execute(Tick(10 * cfg.rt_timeout));

            test_4.execute(ExpectState{State::CLOSED});
        }

        // test 5: start in FIN_WAIT_1, ack, FIN, FIN again, time out
        {
            TCPTestHarness test_5 = TCPTestHarness::in_fin_wait_1(cfg);

            // ACK the FIN
            const WrappingInt32 rx_seqno{1};
            test_5.send_ack(rx_seqno, WrappingInt32{2});
            test_5.execute(ExpectState{State::FIN_WAIT_2});
            test_5.execute(Tick(5));

            test_5.send_fin(rx_seqno, WrappingInt32{2});
            test_5.execute(ExpectState{State::TIME_WAIT});
            test_5.execute(ExpectLingerTimer{0ul});
            const auto ack_expect = rx_seqno + 1;
            test_5.execute(Tick(1));
            test_5.execute(ExpectLingerTimer{1ul});
            test_5.execute(ExpectOneSegment{}.with_no_flags().with_ack(true).with_ackno(ack_expect));

            test_5.execute(Tick(10 * cfg.rt_timeout - 10));
            test_5.execute(ExpectLingerTimer{uint64_t(10 * cfg.rt_timeout - 9)});

            test_5.send_fin(rx_seqno, WrappingInt32{2});
            test_5.execute(ExpectLingerTimer{0ul});
            test_5.execute(Tick(1));

            test_5.execute(ExpectOneSegment{}.with_ack(true).with_ackno(ack_expect),
                           "test 5 failed: no ACK for 2nd FIN");

            test_5.execute(ExpectState{State::TIME_WAIT});

            // tick the timer and see what happens---a 2nd FIN in TIME_WAIT should reset the wait timer!
            // (this is an edge case of "throw it away and send another ack" for out-of-window segs)
            test_5.execute(Tick(10 * cfg.rt_timeout - 10));

            test_5.execute(ExpectLingerTimer{uint64_t(10 * cfg.rt_timeout - 9)},
                           "test 5 failed: time_since_last_segment_received() should reset after 2nd FIN");

            test_5.execute(ExpectNoSegment{});

            test_5.execute(Tick(10));
            test_5.execute(ExpectState{State::CLOSED});
        }

        // test 6: start in ESTABLISHED, get FIN, get FIN re-tx, send FIN, get ACK, send ACK, time out
        {
            TCPTestHarness test_6 = TCPTestHarness::in_established(cfg);

            test_6.execute(Close{});
            test_6.execute(Tick(1));

            TCPSegment seg1 = test_6.expect_seg(ExpectOneSegment{}.with_fin(true),

                                                "test 6 failed: bad FIN after close()");

            auto &seg1_hdr = seg1.header();

            test_6.execute(Tick(cfg.rt_timeout - 2));

            test_6.execute(ExpectNoSegment{}, "test 6 failed: FIN re-tx was too fast");

            test_6.execute(Tick(2));

            TCPSegment seg2 = test_6.expect_seg(ExpectOneSegment{}.with_fin(true).with_seqno(seg1_hdr.seqno),

                                                "test 6 failed: bad re-tx FIN");
            auto &seg2_hdr = seg2.header();

            const WrappingInt32 rx_seqno{1};
            test_6.send_fin(rx_seqno, WrappingInt32{0});
            const auto ack_expect = rx_seqno + 1;
            test_6.execute(Tick(1));

            test_6.execute(ExpectState{State::CLOSING});
            test_6.execute(ExpectOneSegment{}.with_ack(true).with_ackno(ack_expect), "test 6 failed: bad ACK for FIN");

            test_6.send_ack(ack_expect, seg2_hdr.seqno + 1);
            test_6.execute(Tick(1));

            test_6.execute(ExpectState{State::TIME_WAIT});

            test_6.execute(Tick(10 * cfg.rt_timeout));

            test_6.execute(ExpectState{State::CLOSED});
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
