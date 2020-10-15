#include "tcp_config.hh"
#include "tcp_expectation.hh"
#include "tcp_fsm_test_harness.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"
#include "test_err_if.hh"
#include "util.hh"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace std;
using State = TCPTestHarness::State;

static constexpr unsigned NREPS = 32;

bool wrapping_lt(WrappingInt32 a, WrappingInt32 b) { return static_cast<int32_t>(b.raw_value() - a.raw_value()) > 0; }
bool wrapping_gt(WrappingInt32 a, WrappingInt32 b) { return wrapping_lt(b, a); }

int main() {
    try {
        TCPConfig cfg{};
        cfg.recv_capacity = 65000;
        auto rd = get_random_generator();

        // non-overlapping out-of-order segments
        for (unsigned rep_no = 0; rep_no < NREPS; ++rep_no) {
            const WrappingInt32 rx_isn(rd());
            const WrappingInt32 tx_isn(rd());
            TCPTestHarness test_1 = TCPTestHarness::in_established(cfg, tx_isn, rx_isn);
            vector<tuple<size_t, size_t>> seq_size;
            size_t datalen = 0;
            while (datalen < cfg.recv_capacity) {
                const size_t size = min(1 + (static_cast<size_t>(rd()) % (TCPConfig::MAX_PAYLOAD_SIZE - 1)),
                                        cfg.recv_capacity - datalen);
                seq_size.emplace_back(datalen, size);
                datalen += size;
            }
            shuffle(seq_size.begin(), seq_size.end(), rd);

            string d(datalen, 0);
            generate(d.begin(), d.end(), [&] { return rd(); });

            WrappingInt32 min_expect_ackno = rx_isn + 1;
            WrappingInt32 max_expect_ackno = rx_isn + 1;
            for (auto [off, sz] : seq_size) {
                test_1.send_data(rx_isn + 1 + off, tx_isn + 1, d.cbegin() + off, d.cbegin() + off + sz);
                if (off == min_expect_ackno.raw_value()) {
                    min_expect_ackno = min_expect_ackno + sz;
                }
                max_expect_ackno = max_expect_ackno + sz;

                TCPSegment seg =
                    test_1.expect_seg(ExpectSegment{}.with_ack(true), "test 1 failed: no ACK for datagram");

                auto &seg_hdr = seg.header();

                test_err_if(wrapping_lt(seg_hdr.ackno, min_expect_ackno) ||
                                wrapping_gt(seg_hdr.ackno, max_expect_ackno),
                            "test 1 failed: no ack or out of expected range");
            }

            test_1.execute(Tick(1));
            test_1.execute(ExpectData{}.with_data(d), "test 1 failed: got back the wrong data");
        }

        // overlapping out-of-order segments
        for (unsigned rep_no = 0; rep_no < NREPS; ++rep_no) {
            WrappingInt32 rx_isn(rd());
            WrappingInt32 tx_isn(rd());
            TCPTestHarness test_2 = TCPTestHarness::in_established(cfg, tx_isn, rx_isn);

            vector<tuple<size_t, size_t>> seq_size;
            size_t datalen = 0;
            while (datalen < cfg.recv_capacity) {
                const size_t size = min(1 + (static_cast<size_t>(rd()) % (TCPConfig::MAX_PAYLOAD_SIZE - 1)),
                                        cfg.recv_capacity - datalen);
                const size_t rem = TCPConfig::MAX_PAYLOAD_SIZE - size;
                size_t offs;
                if (rem == 0) {
                    offs = 0;
                } else if (rem == 1) {
                    offs = min(size_t(1), datalen);
                } else {
                    offs = min(min(datalen, rem), 1 + (static_cast<size_t>(rd()) % (rem - 1)));
                }
                test_err_if(size + offs > TCPConfig::MAX_PAYLOAD_SIZE, "test 2 internal error: bad payload size");
                seq_size.emplace_back(datalen - offs, size + offs);
                datalen += size;
            }
            test_err_if(datalen > cfg.recv_capacity, "test 2 internal error: bad offset sequence");
            shuffle(seq_size.begin(), seq_size.end(), rd);

            string d(datalen, 0);
            generate(d.begin(), d.end(), [&] { return rd(); });

            WrappingInt32 min_expect_ackno = rx_isn + 1;
            WrappingInt32 max_expect_ackno = rx_isn + 1;
            for (auto [off, sz] : seq_size) {
                test_2.send_data(rx_isn + 1 + off, tx_isn + 1, d.cbegin() + off, d.cbegin() + off + sz);
                if (off <= min_expect_ackno.raw_value() && off + sz > min_expect_ackno.raw_value()) {
                    min_expect_ackno = WrappingInt32(sz + off);
                }
                max_expect_ackno = max_expect_ackno + sz;  // really loose because of overlap

                TCPSegment seg =
                    test_2.expect_seg(ExpectSegment{}.with_ack(true), "test 2 failed: no ACK for datagram");
                auto &seg_hdr = seg.header();

                test_err_if(wrapping_lt(seg_hdr.ackno, min_expect_ackno) ||
                                wrapping_gt(seg_hdr.ackno, max_expect_ackno),
                            "test 2 failed: no ack or out of expected range");
            }

            test_2.execute(Tick(1));
            test_2.execute(ExpectData{}.with_data(d), "test 1 failed: got back the wrong data");
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return err_num;
    }

    return EXIT_SUCCESS;
}
