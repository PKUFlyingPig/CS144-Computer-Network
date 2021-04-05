#include "fsm_retx.hh"

#include "tcp_config.hh"
#include "util.hh"

#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace std;
using State = TCPTestHarness::State;

int main() {
    try {
        TCPConfig cfg{};
        cfg.recv_capacity = 65000;
        auto rd = get_random_generator();

        // multiple segments with intervening ack
        {
            WrappingInt32 tx_ackno(rd());
            TCPTestHarness test_2 = TCPTestHarness::in_established(cfg, tx_ackno - 1, tx_ackno - 1);

            string d1 = "asdf";
            string d2 = "qwer";
            test_2.execute(Write{d1});
            test_2.execute(Tick(1));
            test_2.execute(Tick(20));
            test_2.execute(Write{d2});
            test_2.execute(Tick(1));

            test_2.execute(ExpectSegmentAvailable{}, "test 2 failed: cannot read after write()s");

            check_segment(test_2, d1, true, __LINE__);
            check_segment(test_2, d2, false, __LINE__);

            test_2.execute(Tick(cfg.rt_timeout - 23));
            test_2.execute(ExpectNoSegment{}, "test 2 failed: re-tx too fast");

            test_2.execute(Tick(4));
            check_segment(test_2, d1, false, __LINE__);

            test_2.execute(Tick(2 * cfg.rt_timeout - 2));
            test_2.execute(ExpectNoSegment{}, "test 2 failed: re-tx too fast");

            test_2.send_ack(tx_ackno, tx_ackno + 4);  // make sure RTO timer restarts on successful ACK
            test_2.execute(Tick(cfg.rt_timeout - 2));
            test_2.execute(ExpectNoSegment{}, "test 2 failed: re-tx of 2nd seg after ack for 1st seg too fast");
            test_2.execute(Tick(3));
            check_segment(test_2, d2, false, __LINE__);
        }

        // multiple segments without intervening ack
        {
            WrappingInt32 tx_ackno(rd());
            TCPTestHarness test_3 = TCPTestHarness::in_established(cfg, tx_ackno - 1, tx_ackno - 1);

            string d1 = "asdf";
            string d2 = "qwer";
            test_3.execute(Write{d1});
            test_3.execute(Tick(1));
            test_3.execute(Tick(20));
            test_3.execute(Write{d2});
            test_3.execute(Tick(1));

            test_3.execute(ExpectSegmentAvailable{}, "test 3 failed: cannot read after write()s");
            check_segment(test_3, d1, true, __LINE__);
            check_segment(test_3, d2, false, __LINE__);

            test_3.execute(Tick(cfg.rt_timeout - 23));
            test_3.execute(ExpectNoSegment{}, "test 3 failed: re-tx too fast");

            test_3.execute(Tick(4));
            check_segment(test_3, d1, false, __LINE__);

            test_3.execute(Tick(2 * cfg.rt_timeout - 2));
            test_3.execute(ExpectNoSegment{}, "test 3 failed: re-tx of 2nd seg too fast");

            test_3.execute(Tick(3));
            check_segment(test_3, d1, false, __LINE__);
        }

        // check that ACK of new data resets exponential backoff and restarts timer
        auto backoff_test = [&](const unsigned int num_backoffs) {
            WrappingInt32 tx_ackno(rd());
            TCPTestHarness test_4 = TCPTestHarness::in_established(cfg, tx_ackno - 1, tx_ackno - 1);

            string d1 = "asdf";
            string d2 = "qwer";
            test_4.execute(Write{d1});
            test_4.execute(Tick(1));
            test_4.execute(Tick(20));
            test_4.execute(Write{d2});
            test_4.execute(Tick(1));

            test_4.execute(ExpectSegmentAvailable{}, "test 4 failed: cannot read after write()s");
            check_segment(test_4, d1, true, __LINE__);
            check_segment(test_4, d2, false, __LINE__);

            test_4.execute(Tick(cfg.rt_timeout - 23));
            test_4.execute(ExpectNoSegment{}, "test 4 failed: re-tx too fast");

            test_4.execute(Tick(4));
            check_segment(test_4, d1, false, __LINE__);

            for (unsigned i = 1; i < num_backoffs; ++i) {
                test_4.execute(Tick((cfg.rt_timeout << i) - i));  // exponentially increasing delay length
                test_4.execute(ExpectNoSegment{}, "test 4 failed: re-tx too fast after timeout");
                test_4.execute(Tick(i));
                check_segment(test_4, d1, false, __LINE__);
            }

            test_4.send_ack(tx_ackno, tx_ackno + 4);  // make sure RTO timer restarts on successful ACK
            test_4.execute(Tick(cfg.rt_timeout - 2));
            test_4.execute(ExpectNoSegment{},
                           "test 4 failed: re-tx of 2nd seg after ack for 1st seg too fast after " +
                               to_string(num_backoffs) + " backoffs");
            test_4.execute(Tick(3));
            check_segment(test_4, d2, false, __LINE__);
        };

        for (unsigned int i = 0; i < TCPConfig::MAX_RETX_ATTEMPTS; ++i) {
            backoff_test(i);
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
