#include "byte_stream.hh"
#include "byte_stream_test_harness.hh"

#include <exception>
#include <iostream>

using namespace std;

int main() {
    try {
        {
            ByteStreamTestHarness test{"overwrite", 2};

            test.execute(Write{"cat"}.with_bytes_written(2));

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{2});
            test.execute(RemainingCapacity{0});
            test.execute(BufferSize{2});
            test.execute(Peek{"ca"});

            test.execute(Write{"t"}.with_bytes_written(0));

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{0});
            test.execute(BytesWritten{2});
            test.execute(RemainingCapacity{0});
            test.execute(BufferSize{2});
            test.execute(Peek{"ca"});
        }

        {
            ByteStreamTestHarness test{"overwrite-clear-overwrite", 2};

            test.execute(Write{"cat"}.with_bytes_written(2));
            test.execute(Pop{2});
            test.execute(Write{"tac"}.with_bytes_written(2));

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{2});
            test.execute(BytesWritten{4});
            test.execute(RemainingCapacity{0});
            test.execute(BufferSize{2});
            test.execute(Peek{"ta"});
        }

        {
            ByteStreamTestHarness test{"overwrite-pop-overwrite", 2};

            test.execute(Write{"cat"}.with_bytes_written(2));
            test.execute(Pop{1});
            test.execute(Write{"tac"}.with_bytes_written(1));

            test.execute(InputEnded{false});
            test.execute(BufferEmpty{false});
            test.execute(Eof{false});
            test.execute(BytesRead{1});
            test.execute(BytesWritten{3});
            test.execute(RemainingCapacity{0});
            test.execute(BufferSize{2});
            test.execute(Peek{"at"});
        }

        {
            ByteStreamTestHarness test{"long-stream", 3};

            test.execute(Write{"abcdef"}.with_bytes_written(3));
            test.execute(Peek{"abc"});
            test.execute(Pop{1});

            for (unsigned int i = 0; i < 99997; i++) {
                test.execute(RemainingCapacity{1});
                test.execute(BufferSize{2});
                test.execute(Write{"abc"}.with_bytes_written(1));
                test.execute(RemainingCapacity{0});
                test.execute(Peek{"bca"});
                test.execute(Pop{1});

                test.execute(RemainingCapacity{1});
                test.execute(BufferSize{2});
                test.execute(Write{"bca"}.with_bytes_written(1));
                test.execute(RemainingCapacity{0});
                test.execute(Peek{"cab"});
                test.execute(Pop{1});

                test.execute(RemainingCapacity{1});
                test.execute(BufferSize{2});
                test.execute(Write{"cab"}.with_bytes_written(1));
                test.execute(RemainingCapacity{0});
                test.execute(Peek{"abc"});
                test.execute(Pop{1});
            }

            test.execute(EndInput{});
            test.execute(Peek{"bc"});
            test.execute(Pop{2});
            test.execute(Eof{true});
        }

    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
