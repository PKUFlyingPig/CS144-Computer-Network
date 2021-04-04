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
            uint16_t retx_timeout = uniform_int_distribution<uint16_t>{10, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = retx_timeout;

            TCPSenderTestHarness test{"Retx SYN twice at the right times, then ack", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectNoSegment{});
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            test.execute(Tick{retx_timeout - 1u});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{1});
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            test.execute(ExpectBytesInFlight{1});
            // Wait twice as long b/c exponential back-off
            test.execute(Tick{2 * retx_timeout - 1u});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{1});
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            test.execute(ExpectBytesInFlight{1});
            test.execute(AckReceived{WrappingInt32{isn + 1}});
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectBytesInFlight{0});
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            uint16_t retx_timeout = uniform_int_distribution<uint16_t>{10, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = retx_timeout;

            TCPSenderTestHarness test{"Retx SYN until too many retransmissions", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectNoSegment{});
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            for (size_t attempt_no = 0; attempt_no < TCPConfig::MAX_RETX_ATTEMPTS; attempt_no++) {
                test.execute(Tick{(retx_timeout << attempt_no) - 1u}.with_max_retx_exceeded(false));
                test.execute(ExpectNoSegment{});
                test.execute(Tick{1}.with_max_retx_exceeded(false));
                test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
                test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
                test.execute(ExpectBytesInFlight{1});
            }
            test.execute(Tick{(retx_timeout << TCPConfig::MAX_RETX_ATTEMPTS) - 1u}.with_max_retx_exceeded(false));
            test.execute(Tick{1}.with_max_retx_exceeded(true));
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            uint16_t retx_timeout = uniform_int_distribution<uint16_t>{10, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = retx_timeout;

            TCPSenderTestHarness test{"Send some data, the retx and succeed, then retx till limit", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 1}});
            test.execute(WriteBytes{"abcd"});
            test.execute(ExpectSegment{}.with_payload_size(4));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 5}});
            test.execute(ExpectBytesInFlight{0});
            test.execute(WriteBytes{"efgh"});
            test.execute(ExpectSegment{}.with_payload_size(4));
            test.execute(ExpectNoSegment{});
            test.execute(Tick{retx_timeout}.with_max_retx_exceeded(false));
            test.execute(ExpectSegment{}.with_payload_size(4));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 9}});
            test.execute(ExpectBytesInFlight{0});
            test.execute(WriteBytes{"ijkl"});
            test.execute(ExpectSegment{}.with_payload_size(4).with_seqno(isn + 9));
            for (size_t attempt_no = 0; attempt_no < TCPConfig::MAX_RETX_ATTEMPTS; attempt_no++) {
                test.execute(Tick{(retx_timeout << attempt_no) - 1u}.with_max_retx_exceeded(false));
                test.execute(ExpectNoSegment{});
                test.execute(Tick{1}.with_max_retx_exceeded(false));
                test.execute(ExpectSegment{}.with_payload_size(4).with_seqno(isn + 9));
                test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
                test.execute(ExpectBytesInFlight{4});
            }
            test.execute(Tick{(retx_timeout << TCPConfig::MAX_RETX_ATTEMPTS) - 1u}.with_max_retx_exceeded(false));
            test.execute(Tick{1}.with_max_retx_exceeded(true));
        }

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
