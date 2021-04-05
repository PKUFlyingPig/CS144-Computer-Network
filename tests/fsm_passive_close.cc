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

        // test #1: start in LAST_ACK, ack
        {
            TCPTestHarness test_1 = TCPTestHarness::in_last_ack(cfg);

            test_1.execute(Tick(4 * cfg.rt_timeout));

            test_1.execute(ExpectState{State::LAST_ACK});

            test_1.send_ack(WrappingInt32{2}, WrappingInt32{2});
            test_1.execute(Tick(1));

            test_1.execute(ExpectState{State::CLOSED});
        }

        // test #2: start in CLOSE_WAIT, close(), throw away first FIN, ack re-tx FIN
        {
            TCPTestHarness test_2 = TCPTestHarness::in_close_wait(cfg);

            test_2.execute(Tick(4 * cfg.rt_timeout));

            test_2.execute(ExpectState{State::CLOSE_WAIT});

            test_2.execute(Close{});
            test_2.execute(Tick(1));

            test_2.execute(ExpectState{State::LAST_ACK});

            TCPSegment seg1 = test_2.expect_seg(ExpectOneSegment{}.with_fin(true), "test 2 falied: bad seg or no FIN");

            test_2.execute(Tick(cfg.rt_timeout - 2));

            test_2.execute(ExpectNoSegment{}, "test 2 failed: FIN re-tx was too fast");

            test_2.execute(Tick(2));

            TCPSegment seg2 = test_2.expect_seg(ExpectOneSegment{}.with_fin(true).with_seqno(seg1.header().seqno),
                                                "test 2 failed: bad re-tx FIN");

            const WrappingInt32 rx_seqno{2};
            const auto ack_expect = rx_seqno;
            test_2.send_ack(ack_expect, seg2.header().seqno - 1);  // wrong ackno!
            test_2.execute(Tick(1));

            test_2.execute(ExpectState{State::LAST_ACK});

            test_2.send_ack(ack_expect, seg2.header().seqno + 1);
            test_2.execute(Tick(1));

            test_2.execute(ExpectState{State::CLOSED});
        }

        // test #3: start in ESTABLSHED, send FIN, recv ACK, check for CLOSE_WAIT
        {
            TCPTestHarness test_3 = TCPTestHarness::in_established(cfg);

            test_3.execute(Tick(4 * cfg.rt_timeout));

            test_3.execute(ExpectState{State::ESTABLISHED});

            const WrappingInt32 rx_seqno{1};
            const auto ack_expect = rx_seqno + 1;
            test_3.send_fin(rx_seqno, WrappingInt32{0});
            test_3.execute(Tick(1));

            test_3.execute(ExpectOneSegment{}.with_ack(true).with_ackno(ack_expect),
                           "test 3 failed: bad seg, no ACK, or wrong ackno");

            test_3.execute(ExpectState{State::CLOSE_WAIT});

            test_3.send_fin(rx_seqno, WrappingInt32{0});
            test_3.execute(Tick(1));

            test_3.execute(ExpectOneSegment{}.with_ack(true).with_ackno(ack_expect),
                           "test 3 falied: bad response to 2nd FIN");

            test_3.execute(ExpectState{State::CLOSE_WAIT});

            test_3.execute(Tick(1));
            test_3.execute(Close{});
            test_3.execute(Tick(1));

            test_3.execute(ExpectState{State::LAST_ACK});

            TCPSegment seg3 = test_3.expect_seg(ExpectOneSegment{}.with_fin(true), "test 3 failed: bad seg or no FIN");

            test_3.send_ack(ack_expect, seg3.header().seqno + 1);
            test_3.execute(Tick(1));

            test_3.execute(ExpectState{State::CLOSED});
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
