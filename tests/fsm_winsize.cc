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
#include <vector>

using namespace std;
using State = TCPTestHarness::State;

static constexpr unsigned NREPS = 32;
static constexpr unsigned MIN_SWIN = 2048;
static constexpr unsigned MAX_SWIN = 34816;
static constexpr unsigned MIN_SWIN_MUL = 2;
static constexpr unsigned MAX_SWIN_MUL = 6;

int main() {
    try {
        auto rd = get_random_generator();
        TCPConfig cfg{};
        cfg.send_capacity = MAX_SWIN * MAX_SWIN_MUL;

        // test 1: listen -> established -> check advertised winsize -> check sent bytes before ACK
        for (unsigned rep_no = 0; rep_no < NREPS; ++rep_no) {
            cfg.recv_capacity = 2048 + (rd() % 32768);
            const WrappingInt32 seq_base(rd());
            TCPTestHarness test_1(cfg);

            // connect
            test_1.execute(Listen{});
            test_1.send_syn(seq_base);

            TCPSegment seg = test_1.expect_seg(
                ExpectOneSegment{}.with_ack(true).with_ackno(seq_base + 1).with_win(cfg.recv_capacity),
                "test 1 failed: SYN/ACK invalid");
            auto &seg_hdr = seg.header();

            const WrappingInt32 ack_base = seg_hdr.seqno;

            // ack
            const uint16_t swin = MIN_SWIN + (rd() % (MAX_SWIN - MIN_SWIN));
            test_1.send_ack(seq_base + 1, ack_base + 1, swin);  // adjust send window here

            test_1.execute(ExpectNoSegment{}, "test 1 failed: ACK after acceptable ACK");
            test_1.execute(ExpectState{State::ESTABLISHED});

            // write swin_mul * swin, make sure swin gets sent
            const unsigned swin_mul = MIN_SWIN_MUL + (rd() % (MAX_SWIN_MUL - MIN_SWIN_MUL));
            string d(swin_mul * swin, 0);
            generate(d.begin(), d.end(), [&] { return rd(); });
            test_1.execute(Write{d}.with_bytes_written(swin_mul * swin));
            test_1.execute(Tick(1));

            string d_out(swin_mul * swin, 0);
            size_t bytes_total = 0;
            while (bytes_total < swin_mul * swin) {
                test_1.execute(ExpectSegmentAvailable{}, "test 1 failed: nothing sent after write()");
                size_t bytes_read = 0;
                while (test_1.can_read()) {
                    TCPSegment seg2 = test_1.expect_seg(
                        ExpectSegment{}.with_ack(true).with_ackno(seq_base + 1).with_win(cfg.recv_capacity),
                        "test 1 failed: invalid datagrams carrying write() data");
                    auto &seg2_hdr = seg2.header();
                    bytes_read += seg2.payload().size();
                    size_t seg2_first = seg2_hdr.seqno - ack_base - 1;
                    copy(seg2.payload().str().cbegin(), seg2.payload().str().cend(), d_out.begin() + seg2_first);
                }
                test_err_if(  // correct number of bytes sent
                    bytes_read + TCPConfig::MAX_PAYLOAD_SIZE < swin,
                    "test 1 failed: sender did not fill window");
                test_1.execute(ExpectBytesInFlight{bytes_read}, "test 1 failed: sender wrong bytes_in_flight");

                bytes_total += bytes_read;
                // NOTE that we don't override send window here because cfg should have been updated
                test_1.send_ack(seq_base + 1, ack_base + 1 + bytes_total, swin);
                test_1.execute(Tick(1));
            }
            test_1.execute(ExpectBytesInFlight{0}, "test 1 failed: after acking, bytes still in flight?");
            test_err_if(!equal(d.cbegin(), d.cend(), d_out.cbegin()), "test 1 failed: data mismatch");
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return err_num;
    }

    return EXIT_SUCCESS;
}
