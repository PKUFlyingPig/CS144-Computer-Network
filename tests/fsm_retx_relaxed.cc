#include "fsm_retx.hh"
#include "tcp_config.hh"
#include "tcp_expectation.hh"
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

        // NOTE: the timeouts in this test are carefully adjusted to work whether the tcp_state_machine sends
        // immediately upon write() or only after the next tick(). If you edit this test, make sure that
        // it passes both ways (i.e., with and without calling _try_send() in TCPConnection::write())

        // NOTE 2: ACK -- I think I was successful, although given unrelated
        // refactor to template code it will be more challenging now
        // to wait until tick() to send an outgoing segment.

        // single segment re-transmit
        {
            WrappingInt32 tx_ackno(rd());
            TCPTestHarness test_1 = TCPTestHarness::in_established(cfg, tx_ackno - 1, tx_ackno - 1);

            string data = "asdf";
            test_1.execute(Write{data});
            test_1.execute(Tick(1));

            check_segment(test_1, data, false, __LINE__);

            test_1.execute(Tick(cfg.rt_timeout - 2));
            test_1.execute(ExpectNoSegment{}, "test 1 failed: re-tx too fast");

            test_1.execute(Tick(2));
            check_segment(test_1, data, false, __LINE__);

            test_1.execute(Tick(10 * cfg.rt_timeout + 100));
            check_segment(test_1, data, false, __LINE__);

            for (unsigned i = 2; i < TCPConfig::MAX_RETX_ATTEMPTS; ++i) {
                test_1.execute(Tick((cfg.rt_timeout << i) - i));  // exponentially increasing delay length
                test_1.execute(ExpectNoSegment{}, "test 1 failed: re-tx too fast after timeout");
                test_1.execute(Tick(i));
                check_segment(test_1, data, false, __LINE__);
            }

            test_1.execute(ExpectState{State::ESTABLISHED});

            test_1.execute(Tick(1 + (cfg.rt_timeout << TCPConfig::MAX_RETX_ATTEMPTS)));
            test_1.execute(ExpectState{State::RESET});
            test_1.execute(ExpectOneSegment{}.with_rst(true), "test 1 failed: RST on re-tx failure was malformed");
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
