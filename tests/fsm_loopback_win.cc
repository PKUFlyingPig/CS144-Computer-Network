#include "tcp_config.hh"
#include "tcp_expectation.hh"
#include "tcp_fsm_test_harness.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"
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

int main() {
    try {
        TCPConfig cfg{};
        cfg.recv_capacity = 65000;
        auto rd = get_random_generator();

        // loop segments back in a different order
        for (unsigned rep_no = 0; rep_no < NREPS; ++rep_no) {
            const WrappingInt32 rx_offset(rd());
            TCPTestHarness test_2 = TCPTestHarness::in_established(cfg, rx_offset - 1, rx_offset - 1);
            test_2.send_ack(rx_offset, rx_offset, 65000);

            string d(cfg.recv_capacity, 0);
            generate(d.begin(), d.end(), [&] { return rd(); });

            vector<TCPSegment> segs;
            size_t sendoff = 0;
            while (sendoff < d.size()) {
                const size_t len = min(d.size() - sendoff, static_cast<size_t>(rd()) % 8192);
                if (len == 0) {
                    continue;
                }
                test_2.execute(Write{d.substr(sendoff, len)});
                test_2.execute(Tick(1));

                test_2.execute(ExpectSegmentAvailable{}, "test 2 failed: cannot read after write()");
                while (test_2.can_read()) {
                    segs.emplace_back(test_2.expect_seg(ExpectSegment{}));
                }
                sendoff += len;
            }

            // resend them in shuffled order
            vector<size_t> seg_idx(segs.size(), 0);
            iota(seg_idx.begin(), seg_idx.end(), 0);
            shuffle(seg_idx.begin(), seg_idx.end(), rd);
            vector<TCPSegment> acks;
            for (auto idx : seg_idx) {
                test_2.execute(SendSegment{std::move(segs[idx])});
                test_2.execute(Tick(1));
                TCPSegment s = test_2.expect_seg(ExpectOneSegment{}.with_ack(true), "test 2 failed: no ACK after rcvd");
                acks.emplace_back(std::move(s));
                test_2.execute(ExpectNoSegment{}, "test 2 failed: double ACK?");
            }

            // send just the final ack
            test_2.execute(SendSegment{std::move(acks.back())});
            test_2.execute(ExpectNoSegment{}, "test 2 failed: ACK for ACK?");

            test_2.execute(ExpectData{}.with_data(d), "test 2 failed: wrong data after loopback");
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
