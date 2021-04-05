#include "tcp_config.hh"
#include "tcp_expectation.hh"
#include "tcp_fsm_test_harness.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"

#include <cstdint>
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

        // test #1: START -> LISTEN -> SYN -> SYN/ACK -> ACK
        {
            TCPTestHarness test_1(cfg);

            // tell the FSM to connect, make sure we get a SYN
            test_1.execute(Listen{});
            test_1.execute(ExpectState{State::LISTEN});
            test_1.execute(Tick(1));
            test_1.execute(ExpectState{State::LISTEN});

            test_1.send_syn(WrappingInt32{0}, {});
            test_1.execute(Tick(1));

            TCPSegment seg = test_1.expect_seg(ExpectOneSegment{}.with_syn(true).with_ack(true).with_ackno(1),
                                               "test 1 failed: no SYN/ACK in response to SYN");
            test_1.execute(ExpectState{State::SYN_RCVD});

            /* remove requirement for corrective ACK in response to out-of-window seqno
                const WrappingInt32 syn_seqno = seg.header().seqno;
                test_1.send_ack(WrappingInt32{0}, syn_seqno + 1);
                test_1.execute(Tick(1));

                test_1.execute(
                    ExpectOneSegment{}.with_no_flags().with_ack(true).with_ackno(1).with_seqno(seg.header().seqno + 1),
                    "test 1 failed: wrong response to old seqno");

                test_1.send_ack(WrappingInt32(cfg.recv_capacity + 1), syn_seqno + 1);
                test_1.execute(Tick(1));

                test_1.execute(
                    ExpectOneSegment{}.with_no_flags().with_ack(true).with_ackno(1).with_seqno(seg.header().seqno + 1));
            */

            test_1.send_ack(WrappingInt32{1}, seg.header().seqno + 1);
            test_1.execute(Tick(1));
            test_1.execute(ExpectNoSegment{}, "test 1 failed: no need to ACK an ACK");
            test_1.execute(ExpectState{State::ESTABLISHED});
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
