#include "byte_stream.hh"
#include "fsm_stream_reassembler_harness.hh"
#include "stream_reassembler.hh"
#include "util.hh"

#include <exception>
#include <iostream>

using namespace std;

int main() {
    try {
        auto rd = get_random_generator();

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"abcd", 0});
            test.execute(BytesAssembled(4));
            test.execute(BytesAvailable("abcd"));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"abcd", 0});
            test.execute(BytesAssembled(4));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});
        }

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"abcd", 0});
            test.execute(BytesAssembled(4));
            test.execute(BytesAvailable("abcd"));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"abcd", 4});
            test.execute(BytesAssembled(8));
            test.execute(BytesAvailable("abcd"));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"abcd", 0});
            test.execute(BytesAssembled(8));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"abcd", 4});
            test.execute(BytesAssembled(8));
            test.execute(BytesAvailable(""));
            test.execute(NotAtEof{});
        }

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"abcdefgh", 0});
            test.execute(BytesAssembled(8));
            test.execute(BytesAvailable("abcdefgh"));
            test.execute(NotAtEof{});
            string data = "abcdefgh";

            for (size_t i = 0; i < 1000; ++i) {
                size_t start_i = uniform_int_distribution<size_t>{0, 8}(rd);
                auto start = data.begin();
                std::advance(start, start_i);

                size_t end_i = uniform_int_distribution<size_t>{start_i, 8}(rd);
                auto end = data.begin();
                std::advance(end, end_i);

                test.execute(SubmitSegment{string{start, end}, start_i});
                test.execute(BytesAssembled(8));
                test.execute(BytesAvailable(""));
                test.execute(NotAtEof{});
            }
        }

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"abcd", 0});
            test.execute(BytesAssembled(4));
            test.execute(BytesAvailable("abcd"));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"abcdef", 0});
            test.execute(BytesAssembled(6));
            test.execute(BytesAvailable("ef"));
            test.execute(NotAtEof{});
        }

    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
