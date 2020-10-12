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
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{"If already running, timer stays running when new segment sent", cfg};

            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(WriteBytes("abc"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1));
            test.execute(Tick{rto - 5});
            test.execute(ExpectNoSegment{});
            test.execute(WriteBytes("def"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("def"));
            test.execute(Tick{6});
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1));
            test.execute(ExpectNoSegment{});
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{"Retransmission still happens when expiration time not hit exactly", cfg};

            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(WriteBytes("abc"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1));
            test.execute(Tick{rto - 5});
            test.execute(ExpectNoSegment{});
            test.execute(WriteBytes("def"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("def"));
            test.execute(Tick{200});
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1));
            test.execute(ExpectNoSegment{});
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{"Timer restarts on ACK of new data", cfg};

            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(WriteBytes("abc"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1));
            test.execute(Tick{rto - 5});
            test.execute(WriteBytes("def"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("def").with_seqno(isn + 4));
            test.execute(AckReceived{WrappingInt32{isn + 4}}.with_win(1000));
            test.execute(Tick{rto - 1});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{2});
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("def").with_seqno(isn + 4));
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{"Timer doesn't restart without ACK of new data", cfg};

            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(WriteBytes("abc"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1));
            test.execute(Tick{rto - 5});
            test.execute(WriteBytes("def"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("def").with_seqno(isn + 4));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(Tick{6});
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1));
            test.execute(ExpectNoSegment{});
            test.execute(Tick{rto * 2 - 5});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{8});
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1));
            test.execute(ExpectNoSegment{});
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{"RTO resets on ACK of new data", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(WriteBytes("abc"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1));
            test.execute(Tick{rto - 5});
            test.execute(WriteBytes("def"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("def").with_seqno(isn + 4));
            test.execute(WriteBytes("ghi"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("ghi").with_seqno(isn + 7));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(Tick{6});
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1));
            test.execute(ExpectNoSegment{});
            test.execute(Tick{rto * 2 - 5});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{5});
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1));
            test.execute(ExpectNoSegment{});
            test.execute(Tick{rto * 4 - 5});
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 4}}.with_win(1000));
            test.execute(Tick{rto - 1});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{2});
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("def").with_seqno(isn + 4));
            test.execute(ExpectNoSegment{});
        }
        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;

            const string nicechars = "abcdefghijklmnopqrstuvwxyz";
            string bigstring;
            for (unsigned int i = 0; i < TCPConfig::DEFAULT_CAPACITY; i++) {
                bigstring.push_back(nicechars.at(rd() % nicechars.size()));
            }

            const size_t window_size = uniform_int_distribution<uint16_t>{50000, 63000}(rd);

            TCPSenderTestHarness test{"fill_window() correctly fills a big window", cfg};
            test.execute(WriteBytes(string(bigstring)));
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(window_size));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});

            for (unsigned int i = 0; i + TCPConfig::MAX_PAYLOAD_SIZE < min(bigstring.size(), window_size);
                 i += TCPConfig::MAX_PAYLOAD_SIZE) {
                const size_t expected_size = min(TCPConfig::MAX_PAYLOAD_SIZE, min(bigstring.size(), window_size) - i);
                test.execute(ExpectSegment{}
                                 .with_no_flags()
                                 .with_payload_size(expected_size)
                                 .with_data(bigstring.substr(i, expected_size))
                                 .with_seqno(isn + 1 + i));
            }
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{"Retransmit a FIN-containing segment same as any other", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(WriteBytes("abc").with_end_input(true));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1).with_fin(true));
            test.execute(Tick{rto - 1});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{2});
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1).with_fin(true));
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{"Retransmit a FIN-only segment same as any other", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(WriteBytes("abc"));
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1).with_no_flags());
            test.execute(Close{});
            test.execute(ExpectSegment{}.with_payload_size(0).with_seqno(isn + 4).with_fin(true));
            test.execute(Tick{rto - 1});
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{isn + 4}.with_win(1000));
            test.execute(Tick{rto - 1});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{2});
            test.execute(ExpectSegment{}.with_payload_size(0).with_seqno(isn + 4).with_fin(true));
            test.execute(Tick{2 * rto - 5});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{10});
            test.execute(ExpectSegment{}.with_payload_size(0).with_seqno(isn + 4).with_fin(true));
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{"Don't add FIN if this would make the segment exceed the receiver's window", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(WriteBytes("abc").with_end_input(true));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(3));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1).with_no_flags());
            test.execute(AckReceived{WrappingInt32{isn + 2}}.with_win(2));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 3}}.with_win(1));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 4}}.with_win(1));
            test.execute(ExpectSegment{}.with_payload_size(0).with_seqno(isn + 4).with_fin(true));
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{"Don't send FIN by itself if the window is full", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(WriteBytes("abc"));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(3));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectSegment{}.with_payload_size(3).with_data("abc").with_seqno(isn + 1).with_no_flags());
            test.execute(Close{});
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 2}}.with_win(2));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 3}}.with_win(1));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 4}}.with_win(1));
            test.execute(ExpectSegment{}.with_payload_size(0).with_seqno(isn + 4).with_fin(true));
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            const string nicechars = "abcdefghijklmnopqrstuvwxyz";
            string bigstring;
            for (unsigned int i = 0; i < TCPConfig::MAX_PAYLOAD_SIZE; i++) {
                bigstring.push_back(nicechars.at(rd() % nicechars.size()));
            }

            TCPSenderTestHarness test{"MAX_PAYLOAD_SIZE limits payload only", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(WriteBytes{string(bigstring)}.with_end_input(true));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(40000));
            test.execute(ExpectSegment{}
                             .with_payload_size(TCPConfig::MAX_PAYLOAD_SIZE)
                             .with_data(string(bigstring))
                             .with_seqno(isn + 1)
                             .with_fin(true));
            test.execute(ExpectState{TCPSenderStateSummary::FIN_SENT});
            test.execute(AckReceived(isn + 2 + TCPConfig::MAX_PAYLOAD_SIZE));
            test.execute(ExpectState{TCPSenderStateSummary::FIN_ACKED});
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{
                "When filling window, treat a '0' window size as equal to '1' but don't back off RTO", cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(WriteBytes("abc"));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(0));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectSegment{}.with_payload_size(1).with_data("a").with_seqno(isn + 1).with_no_flags());
            test.execute(Close{});
            test.execute(ExpectNoSegment{});

            for (unsigned int i = 0; i < 5; i++) {
                test.execute(Tick{rto - 1});
                test.execute(ExpectNoSegment{});
                test.execute(Tick{1});
                test.execute(ExpectSegment{}.with_payload_size(1).with_data("a").with_seqno(isn + 1).with_no_flags());
            }

            test.execute(AckReceived{isn + 2}.with_win(0));
            test.execute(ExpectSegment{}.with_payload_size(1).with_data("b").with_seqno(isn + 2).with_no_flags());

            for (unsigned int i = 0; i < 5; i++) {
                test.execute(Tick{rto - 1});
                test.execute(ExpectNoSegment{});
                test.execute(Tick{1});
                test.execute(ExpectSegment{}.with_payload_size(1).with_data("b").with_seqno(isn + 2).with_no_flags());
            }

            test.execute(AckReceived{isn + 3}.with_win(0));
            test.execute(ExpectSegment{}.with_payload_size(1).with_data("c").with_seqno(isn + 3).with_no_flags());

            for (unsigned int i = 0; i < 5; i++) {
                test.execute(Tick{rto - 1});
                test.execute(ExpectNoSegment{});
                test.execute(Tick{1});
                test.execute(ExpectSegment{}.with_payload_size(1).with_data("c").with_seqno(isn + 3).with_no_flags());
            }

            test.execute(AckReceived{isn + 4}.with_win(0));
            test.execute(ExpectSegment{}.with_payload_size(0).with_data("").with_seqno(isn + 4).with_fin(true));

            for (unsigned int i = 0; i < 5; i++) {
                test.execute(Tick{rto - 1});
                test.execute(ExpectNoSegment{});
                test.execute(Tick{1});
                test.execute(ExpectSegment{}.with_payload_size(0).with_data("").with_seqno(isn + 4).with_fin(true));
            }
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{"Unlike a zero-size window, a full window of nonzero size should be respected",
                                      cfg};
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(WriteBytes("abc"));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectSegment{}.with_payload_size(1).with_data("a").with_seqno(isn + 1).with_no_flags());
            test.execute(Tick{rto - 1});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{1});
            test.execute(ExpectSegment{}.with_payload_size(1).with_data("a").with_seqno(isn + 1).with_no_flags());

            test.execute(Close{});

            test.execute(Tick{2 * rto - 1});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{1});
            test.execute(ExpectSegment{}.with_payload_size(1).with_data("a").with_seqno(isn + 1).with_no_flags());

            test.execute(Tick{4 * rto - 1});
            test.execute(ExpectNoSegment{});
            test.execute(Tick{1});
            test.execute(ExpectSegment{}.with_payload_size(1).with_data("a").with_seqno(isn + 1).with_no_flags());

            test.execute(AckReceived{WrappingInt32{isn + 2}}.with_win(3));
            test.execute(ExpectSegment{}.with_payload_size(2).with_data("bc").with_seqno(isn + 2).with_fin(true));
        }

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            const size_t rto = uniform_int_distribution<uint16_t>{30, 10000}(rd);
            cfg.fixed_isn = isn;
            cfg.rt_timeout = rto;

            TCPSenderTestHarness test{"Repeated ACKs and outdated ACKs are harmless", cfg};

            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(WriteBytes("abcdefg"));
            test.execute(ExpectSegment{}.with_payload_size(7).with_data("abcdefg").with_seqno(isn + 1));
            test.execute(AckReceived{WrappingInt32{isn + 8}}.with_win(1000));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 8}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 8}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 8}}.with_win(1000));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(ExpectNoSegment{});
            test.execute(WriteBytes("ijkl").with_end_input(true));
            test.execute(ExpectSegment{}.with_payload_size(4).with_data("ijkl").with_seqno(isn + 8).with_fin(true));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 8}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 8}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 8}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 12}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 12}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 12}}.with_win(1000));
            test.execute(AckReceived{WrappingInt32{isn + 1}}.with_win(1000));
            test.execute(Tick{5 * rto});
            test.execute(ExpectSegment{}.with_payload_size(4).with_data("ijkl").with_seqno(isn + 8).with_fin(true));
            test.execute(ExpectNoSegment{});
            test.execute(AckReceived(WrappingInt32{isn + 13}).with_win(1000));
            test.execute(AckReceived(WrappingInt32{isn + 1}).with_win(1000));
            test.execute(Tick{5 * rto});
            test.execute(ExpectNoSegment{});
            test.execute(ExpectState{TCPSenderStateSummary::FIN_ACKED});
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
