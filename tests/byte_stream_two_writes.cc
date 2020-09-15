#include "byte_stream.hh"
#include "byte_stream_test_harness.hh"

#include <exception>
#include <iostream>

using namespace std;

int main() {
    try {
        {
            ByteStreamTestHarness test{"write-write-end-pop-pop", 15};

            test.execute(Write{"cat"});

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{12});
            test.execute(BufferSize{3});
            test.execute(Peek{"cat"});

            test.execute(Write{"tac"});

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{6});
            test.execute(RemainingCapacity{9});
            test.execute(BufferSize{6});
            test.execute(Peek{"cattac"});

            test.execute(EndInput{});

            test.execute(InputEnded{true});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{6});
            test.execute(RemainingCapacity{9});
            test.execute(BufferSize{6});
            test.execute(Peek{"cattac"});

            test.execute(Pop{2});

            test.execute(InputEnded{true});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{2});
            test.execute(BytesWritten{6});
            test.execute(RemainingCapacity{11});
            test.execute(BufferSize{4});
            test.execute(Peek{"ttac"});

            test.execute(Pop{4});

            test.execute(InputEnded{true});
            test.execute(BufferEmpty{true});
            test.execute(Eof{true});
            test.execute(BytesRead{6});
            test.execute(BytesWritten{6});
            test.execute(RemainingCapacity{15});
            test.execute(BufferSize{0});
        }

        {
            ByteStreamTestHarness test{"write-pop-write-end-pop", 15};

            test.execute(Write{"cat"});

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{12});
            test.execute(BufferSize{3});
            test.execute(Peek{"cat"});

            test.execute(Pop{2});

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{2});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{14});
            test.execute(BufferSize{1});
            test.execute(Peek{"t"});

            test.execute(Write{"tac"});

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{2});
            test.execute(BytesWritten{6});
            test.execute(RemainingCapacity{11});
            test.execute(BufferSize{4});
            test.execute(Peek{"ttac"});

            test.execute(EndInput{});

            test.execute(InputEnded{true});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{2});
            test.execute(BytesWritten{6});
            test.execute(RemainingCapacity{11});
            test.execute(BufferSize{4});
            test.execute(Peek{"ttac"});

            test.execute(Pop{4});

            test.execute(InputEnded{true});
            test.execute(BufferEmpty{true});
            test.execute(Eof{true});
            test.execute(BytesRead{6});
            test.execute(BytesWritten{6});
            test.execute(RemainingCapacity{15});
            test.execute(BufferSize{0});
        }

    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
