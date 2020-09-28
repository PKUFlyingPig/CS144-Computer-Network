#include "receiver_harness.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace std;

int main() {
    try {
        auto rd = get_random_generator();

        /* segment before SYN */
        {
            uint32_t isn = uniform_int_distribution<uint32_t>{0, UINT32_MAX}(rd);
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            test.execute(
                SegmentArrives{}.with_seqno(isn + 1).with_data("hello").with_result(SegmentArrives::Result::NOT_SYN));
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectBytes{""});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::SYN_RECV});
            test.execute(ExpectAckno{WrappingInt32{isn + 1}});
        }

        /* segment with SYN + data */
        {
            uint32_t isn = uniform_int_distribution<uint32_t>{0, UINT32_MAX}(rd);
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            test.execute(SegmentArrives{}
                             .with_syn()
                             .with_seqno(isn)
                             .with_data("Hello, CS144!")
                             .with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::SYN_RECV});
            test.execute(ExpectAckno{WrappingInt32{isn + 14}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectBytes{"Hello, CS144!"});
        }

        /* empty segment */
        {
            uint32_t isn = uniform_int_distribution<uint32_t>{0, UINT32_MAX}(rd);
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::SYN_RECV});
            test.execute(ExpectAckno{WrappingInt32{isn + 1}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn + 1).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(ExpectInputNotEnded{});
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn + 5).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(ExpectInputNotEnded{});
        }

        /* segment with null byte */
        {
            uint32_t isn = uniform_int_distribution<uint32_t>{0, UINT32_MAX}(rd);
            TCPReceiverTestHarness test{4000};
            const string text = "Here's a null byte:"s + '\0' + "and it's gone."s;
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::SYN_RECV});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_seqno(isn + 1).with_data(text).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectBytes{string(text)});
            test.execute(ExpectAckno{WrappingInt32{isn + 35}});
            test.execute(ExpectInputNotEnded{});
        }

        /* segment with data + FIN */
        {
            uint32_t isn = uniform_int_distribution<uint32_t>{0, UINT32_MAX}(rd);
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::SYN_RECV});
            test.execute(SegmentArrives{}
                             .with_fin()
                             .with_data("Goodbye, CS144!")
                             .with_seqno(isn + 1)
                             .with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::FIN_RECV});
            test.execute(ExpectBytes{"Goodbye, CS144!"});
            test.execute(ExpectAckno{WrappingInt32{isn + 17}});
            test.execute(ExpectEof{});
        }

        /* segment with FIN (but can't be assembled yet) */
        {
            uint32_t isn = uniform_int_distribution<uint32_t>{0, UINT32_MAX}(rd);
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::SYN_RECV});
            test.execute(SegmentArrives{}
                             .with_fin()
                             .with_data("oodbye, CS144!")
                             .with_seqno(isn + 2)
                             .with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::SYN_RECV});
            test.execute(ExpectBytes{""});
            test.execute(ExpectAckno{WrappingInt32{isn + 1}});
            test.execute(ExpectInputNotEnded{});
            test.execute(SegmentArrives{}.with_data("G").with_seqno(isn + 1).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::FIN_RECV});
            test.execute(ExpectBytes{"Goodbye, CS144!"});
            test.execute(ExpectAckno{WrappingInt32{isn + 17}});
            test.execute(ExpectEof{});
        }

        /* segment with SYN + data + FIN */
        {
            uint32_t isn = uniform_int_distribution<uint32_t>{0, UINT32_MAX}(rd);
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            test.execute(SegmentArrives{}
                             .with_syn()
                             .with_seqno(isn)
                             .with_data("Hello and goodbye, CS144!")
                             .with_fin()
                             .with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::FIN_RECV});
            test.execute(ExpectAckno{WrappingInt32{isn + 27}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectBytes{"Hello and goodbye, CS144!"});
            test.execute(ExpectEof{});
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
