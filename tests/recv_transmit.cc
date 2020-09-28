#include "receiver_harness.hh"
#include "util.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>

using namespace std;

int main() {
    try {
        auto rd = get_random_generator();

        {
            TCPReceiverTestHarness test{4000};
            test.execute(SegmentArrives{}.with_syn().with_seqno(0).with_result(SegmentArrives::Result::OK));
            test.execute(SegmentArrives{}.with_seqno(1).with_data("abcd").with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{5}});
            test.execute(ExpectBytes{"abcd"});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{4});
        }

        {
            uint32_t isn = 384678;
            TCPReceiverTestHarness test{4000};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(
                SegmentArrives{}.with_seqno(isn + 1).with_data("abcd").with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 5}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{4});
            test.execute(ExpectBytes{"abcd"});
            test.execute(
                SegmentArrives{}.with_seqno(isn + 5).with_data("efgh").with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 9}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{8});
            test.execute(ExpectBytes{"efgh"});
        }

        {
            uint32_t isn = 5;
            TCPReceiverTestHarness test{4000};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(
                SegmentArrives{}.with_seqno(isn + 1).with_data("abcd").with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 5}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{4});
            test.execute(
                SegmentArrives{}.with_seqno(isn + 5).with_data("efgh").with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 9}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{8});
            test.execute(ExpectBytes{"abcdefgh"});
        }

        /* remove requirement for corrective ACK on out-of-window segment
            {
                TCPReceiverTestHarness test{4000};
                test.execute(SegmentArrives{}.with_syn().with_seqno(0).with_result(SegmentArrives::Result::OK));
                test.execute(SegmentArrives{}.with_seqno(1).with_data("abcd").with_result(SegmentArrives::Result::OK));
                test.execute(
                    SegmentArrives{}.with_seqno(1).with_data("efgh").with_result(SegmentArrives::Result::OUT_OF_WINDOW));
                test.execute(
                    SegmentArrives{}.with_seqno(4005).with_data("efgh").with_result(SegmentArrives::Result::OUT_OF_WINDOW));
            }
        */

        // Many (arrive/read)s
        {
            TCPReceiverTestHarness test{4000};
            uint32_t max_block_size = 10;
            uint32_t n_rounds = 10000;
            uint32_t isn = 893472;
            size_t bytes_sent = 0;
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            for (uint32_t i = 0; i < n_rounds; ++i) {
                string data;
                uint32_t block_size = uniform_int_distribution<uint32_t>{1, max_block_size}(rd);
                for (uint8_t j = 0; j < block_size; ++j) {
                    uint8_t c = 'a' + ((i + j) % 26);
                    data.push_back(c);
                }
                test.execute(ExpectAckno{WrappingInt32{isn + uint32_t(bytes_sent) + 1}});
                test.execute(ExpectTotalAssembledBytes{bytes_sent});
                test.execute(SegmentArrives{}
                                 .with_seqno(isn + bytes_sent + 1)
                                 .with_data(data)
                                 .with_result(SegmentArrives::Result::OK));
                bytes_sent += block_size;
                test.execute(ExpectBytes{std::move(data)});
            }
        }

        // Many arrives, one read
        {
            uint32_t max_block_size = 10;
            uint32_t n_rounds = 100;
            TCPReceiverTestHarness test{uint16_t(max_block_size * n_rounds)};
            uint32_t isn = 238;
            size_t bytes_sent = 0;
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            string all_data;
            for (uint32_t i = 0; i < n_rounds; ++i) {
                string data;
                uint32_t block_size = uniform_int_distribution<uint32_t>{1, max_block_size}(rd);
                for (uint8_t j = 0; j < block_size; ++j) {
                    uint8_t c = 'a' + ((i + j) % 26);
                    data.push_back(c);
                    all_data.push_back(c);
                }
                test.execute(ExpectAckno{WrappingInt32{isn + uint32_t(bytes_sent) + 1}});
                test.execute(ExpectTotalAssembledBytes{bytes_sent});
                test.execute(SegmentArrives{}
                                 .with_seqno(isn + bytes_sent + 1)
                                 .with_data(data)
                                 .with_result(SegmentArrives::Result::OK));
                bytes_sent += block_size;
            }
            test.execute(ExpectBytes{std::move(all_data)});
        }

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
