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

static constexpr unsigned NREPS = 64;

int main() {
    try {
        TCPConfig cfg{};
        cfg.recv_capacity = 65000;
        auto rd = get_random_generator();

        // loop segments back into the same FSM
        for (unsigned rep_no = 0; rep_no < NREPS; ++rep_no) {
            const WrappingInt32 rx_offset(rd());
            TCPTestHarness test_1 = TCPTestHarness::in_established(cfg, rx_offset - 1, rx_offset - 1);
            test_1.send_ack(rx_offset, rx_offset, 65000);

            string d(cfg.recv_capacity, 0);
            generate(d.begin(), d.end(), [&] { return rd(); });

            size_t sendoff = 0;
            while (sendoff < d.size()) {
                const size_t len = min(d.size() - sendoff, static_cast<size_t>(rd()) % 8192);
                if (len == 0) {
                    continue;
                }
                test_1.execute(Write{d.substr(sendoff, len)});
                test_1.execute(Tick(1));
                test_1.execute(ExpectBytesInFlight{len});

                test_1.execute(ExpectSegmentAvailable{}, "test 1 failed: cannot read after write()");

                size_t n_segments = ceil(double(len) / TCPConfig::MAX_PAYLOAD_SIZE);
                size_t bytes_remaining = len;

                // Transfer the data segments
                for (size_t i = 0; i < n_segments; ++i) {
                    size_t expected_size = min(bytes_remaining, TCPConfig::MAX_PAYLOAD_SIZE);
                    auto seg = test_1.expect_seg(ExpectSegment{}.with_payload_size(expected_size));
                    bytes_remaining -= expected_size;
                    test_1.execute(SendSegment{move(seg)});
                    test_1.execute(Tick(1));
                }

                // Transfer the (bare) ack segments
                for (size_t i = 0; i < n_segments; ++i) {
                    auto seg = test_1.expect_seg(ExpectSegment{}.with_ack(true).with_payload_size(0));
                    test_1.execute(SendSegment{move(seg)});
                    test_1.execute(Tick(1));
                }
                test_1.execute(ExpectNoSegment{});

                test_1.execute(ExpectBytesInFlight{0});

                sendoff += len;
            }

            test_1.execute(ExpectData{}.with_data(d), "test 1 falied: got back the wrong data");
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
