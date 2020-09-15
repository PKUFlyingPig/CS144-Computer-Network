#include "byte_stream.hh"
#include "byte_stream_test_harness.hh"

#include <exception>
#include <iostream>

using namespace std;

int main() {
    try {
        {
            ByteStreamTestHarness test{"construction", 15};
            test.execute(InputEnded{false});
            test.execute(BufferEmpty{true});
            test.execute(Eof{false});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{0});
            test.execute(RemainingCapacity{15});
            test.execute(BufferSize{0});
        }

        {
            ByteStreamTestHarness test{"construction-end", 15};
            test.execute(EndInput{});
            test.execute(InputEnded{true});
            test.execute(BufferEmpty{true});
            test.execute(Eof{true});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{0});
            test.execute(RemainingCapacity{15});
            test.execute(BufferSize{0});
        }

    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
