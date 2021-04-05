#include "sender_harness.hh"
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

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;

            TCPSenderTestHarness test{"Repeat ACK is ignored", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 1}});
            test.execute(WriteBytes{"a"});
            test.execute(ExpectSegment{}.with_no_flags().with_data("a"));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 1}});
            test.execute(ExpectNoSegment{});
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;

            TCPSenderTestHarness test{"Old ACK is ignored", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 1}});
            test.execute(WriteBytes{"a"});
            test.execute(ExpectSegment{}.with_no_flags().with_data("a"));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 2}});
            test.execute(ExpectNoSegment{});
            test.execute(WriteBytes{"b"});
            test.execute(ExpectSegment{}.with_no_flags().with_data("b"));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 1}});
            test.execute(ExpectNoSegment{});
        }

        // credit for test: Jared Wasserman (2020)
        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;

            TCPSenderTestHarness test{"Impossible ackno (beyond next seqno) is ignored", cfg};
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(AckReceived{WrappingInt32{isn + 2}}.with_win(1000));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
        }

        /* remove requirement to send corrective ACK for bad ACK
            {
                TCPConfig cfg;
                WrappingInt32 isn(rd());
                cfg.fixed_isn = isn;

                TCPSenderTestHarness test{"Early ACK results in bare ACK", cfg};
                test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
                test.execute(ExpectNoSegment{});
                test.execute(AckReceived{WrappingInt32{isn + 1}});
                test.execute(WriteBytes{"a"});
                test.execute(ExpectSegment{}.with_no_flags().with_data("a"));
                test.execute(ExpectNoSegment{});
                test.execute(AckReceived{WrappingInt32{isn + 17}});
                test.execute(ExpectSegment{}.with_seqno(isn + 2));
                test.execute(ExpectNoSegment{});
            }
        */

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
