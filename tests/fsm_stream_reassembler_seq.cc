#include "byte_stream.hh"
#include "fsm_stream_reassembler_harness.hh"
#include "stream_reassembler.hh"
#include "util.hh"

#include <exception>
#include <iostream>

using namespace std;

int main() {
    try {
        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"abcd", 0});
            test.execute(BytesAssembled(4));
            test.execute(BytesAvailable("abcd"));
            test.execute(NotAtEof{});

            test.execute(SubmitSegment{"efgh", 4});
            test.execute(BytesAssembled(8));
            test.execute(BytesAvailable("efgh"));
            test.execute(NotAtEof{});
        }

        {
            ReassemblerTestHarness test{65000};

            test.execute(SubmitSegment{"abcd", 0});
            test.execute(BytesAssembled(4));
            test.execute(NotAtEof{});
            test.execute(SubmitSegment{"efgh", 4});
            test.execute(BytesAssembled(8));

            test.execute(BytesAvailable("abcdefgh"));
            test.execute(NotAtEof{});
        }

        {
            ReassemblerTestHarness test{65000};
            std::ostringstream ss;

            for (size_t i = 0; i < 100; ++i) {
                test.execute(BytesAssembled(4 * i));
                test.execute(SubmitSegment{"abcd", 4 * i});
                test.execute(NotAtEof{});

                ss << "abcd";
            }

            test.execute(BytesAvailable(ss.str()));
            test.execute(NotAtEof{});
        }

        {
            ReassemblerTestHarness test{65000};
            std::ostringstream ss;

            for (size_t i = 0; i < 100; ++i) {
                test.execute(BytesAssembled(4 * i));
                test.execute(SubmitSegment{"abcd", 4 * i});
                test.execute(NotAtEof{});

                test.execute(BytesAvailable("abcd"));
            }
        }

    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
