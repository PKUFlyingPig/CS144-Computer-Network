#include "tcp_config.hh"
#include "tcp_expectation.hh"
#include "tcp_fsm_test_harness.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>

using namespace std;
using State = TCPTestHarness::State;

int main() {
    try {
        TCPConfig cfg{};
        const WrappingInt32 base_seq(1 << 31);

        // test #1: in ESTABLISHED, send unacceptable segments and ACKs
        {
            TCPTestHarness test_1 = TCPTestHarness::in_established(cfg, base_seq - 1, base_seq - 1);

            // acceptable ack---no response
            test_1.send_ack(base_seq, base_seq);

            test_1.execute(ExpectNoSegment{}, "test 1 failed: ACK after acceptable ACK");

            // ack in the past---no response
            test_1.send_ack(base_seq, base_seq - 1);

            test_1.execute(ExpectNoSegment{}, "test 1 failed: ACK after past ACK");

            /* remove corrective ACKs
                // ack in the future---should get ACK back
                test_1.send_ack(base_seq, base_seq + 1);

                test_1.execute(ExpectOneSegment{}.with_ackno(base_seq), "test 1 failed: bad ACK after future ACK");
            */

            // segment out of the window---should get an ACK
            test_1.send_byte(base_seq - 1, base_seq, 1);

            test_1.execute(ExpectUnassembledBytes{0}, "test 1 failed: seg queued on early seqno");
            test_1.execute(ExpectOneSegment{}.with_ackno(base_seq), "test 1 failed: no ack on early seqno");

            // segment out of the window---should get an ACK
            test_1.send_byte(base_seq + cfg.recv_capacity, base_seq, 1);

            test_1.execute(ExpectUnassembledBytes{0}, "test 1 failed: seg queued on late seqno");
            test_1.execute(ExpectOneSegment{}.with_ackno(base_seq), "test 1 failed: no ack on late seqno");

            // segment in the window but late---should get an ACK and seg should be queued
            test_1.send_byte(base_seq + cfg.recv_capacity - 1, base_seq, 1);

            test_1.execute(ExpectUnassembledBytes{1}, "seg not queued on end-of-window seqno");

            test_1.execute(ExpectOneSegment{}.with_ackno(base_seq), "test 1 failed: no ack on end-of-window seqno");
            test_1.execute(ExpectNoData{}, "test 1 failed: no ack on end-of-window seqno");

            // segment next byte in the window - ack should advance and data should be readable
            test_1.send_byte(base_seq, base_seq, 1);

            test_1.execute(ExpectUnassembledBytes{1}, "seg not processed on next seqno");
            test_1.execute(ExpectOneSegment{}.with_ackno(base_seq + 1), "test 1 failed: no ack on next seqno");
            test_1.execute(ExpectData{}, "test 1 failed: no ack on next seqno");

            test_1.send_rst(base_seq + 1);
            test_1.execute(ExpectState{State::RESET});
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
