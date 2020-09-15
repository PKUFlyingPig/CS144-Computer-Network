#include "byte_stream.hh"
#include "byte_stream_test_harness.hh"
#include "util.hh"

#include <exception>
#include <iostream>

using namespace std;

int main() {
    try {
        auto rd = get_random_generator();
        const size_t NREPS = 1000;
        const size_t MIN_WRITE = 10;
        const size_t MAX_WRITE = 200;
        const size_t CAPACITY = MAX_WRITE * NREPS;

        {
            ByteStreamTestHarness test{"many writes", CAPACITY};

            size_t acc = 0;
            for (size_t i = 0; i < NREPS; ++i) {
                const size_t size = MIN_WRITE + (rd() % (MAX_WRITE - MIN_WRITE));
                string d(size, 0);
                generate(d.begin(), d.end(), [&] { return 'a' + (rd() % 26); });

                test.execute(Write{d}.with_bytes_written(size));
                acc += size;

                test.execute(InputEnded{false});
                test.execute(BufferEmpty{false});
                test.execute(Eof{false});
                test.execute(BytesRead{0});
                test.execute(BytesWritten{acc});
                test.execute(RemainingCapacity{CAPACITY - acc});
                test.execute(BufferSize{acc});
            }
        }

    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
