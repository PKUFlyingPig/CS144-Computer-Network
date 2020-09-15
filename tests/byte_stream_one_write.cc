#include "byte_stream.hh"
#include "byte_stream_test_harness.hh"

#include <exception>
#include <iostream>

using namespace std;

int main() {
    try {
        {
            ByteStreamTestHarness test{"write-end-pop", 15};

            test.execute(Write{"cat"});

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{12});
            test.execute(BufferSize{3});
            test.execute(Peek{"cat"});

            test.execute(EndInput{});

            test.execute(InputEnded{true});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{12});
            test.execute(BufferSize{3});
            test.execute(Peek{"cat"});

            test.execute(Pop{3});

            test.execute(InputEnded{true});
            test.execute(BufferEmpty{true});
            test.execute(Eof{true});
            test.execute(BytesRead{3});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{15});
            test.execute(BufferSize{0});
        }

        {
            ByteStreamTestHarness test{"write-pop-end", 15};

            test.execute(Write{"cat"});

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{12});
            test.execute(BufferSize{3});
            test.execute(Peek{"cat"});

            test.execute(Pop{3});

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{true});
            test.execute(Eof{false});
            test.execute(BytesRead{3});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{15});
            test.execute(BufferSize{0});

            test.execute(EndInput{});

            test.execute(InputEnded{true});
            test.execute(BufferEmpty{true});
            test.execute(Eof{true});
            test.execute(BytesRead{3});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{15});
            test.execute(BufferSize{0});
        }

        {
            ByteStreamTestHarness test{"write-pop2-end", 15};

            test.execute(Write{"cat"});

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{12});
            test.execute(BufferSize{3});
            test.execute(Peek{"cat"});

            test.execute(Pop{1});

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{1});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{13});
            test.execute(BufferSize{2});
            test.execute(Peek{"at"});

            test.execute(Pop{2});

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{true});
            test.execute(Eof{false});
            test.execute(BytesRead{3});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{15});
            test.execute(BufferSize{0});

            test.execute(EndInput{});

            test.execute(InputEnded{true});
            test.execute(BufferEmpty{true});
            test.execute(Eof{true});
            test.execute(BytesRead{3});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{15});
            test.execute(BufferSize{0});
        }

    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
