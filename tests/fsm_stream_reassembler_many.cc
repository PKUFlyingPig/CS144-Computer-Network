#include "byte_stream.hh"
#include "stream_reassembler.hh"
#include "util.hh"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

using namespace std;

static constexpr unsigned NREPS = 32;
static constexpr unsigned NSEGS = 128;
static constexpr unsigned MAX_SEG_LEN = 2048;

string read(StreamReassembler &reassembler) {
    return reassembler.stream_out().read(reassembler.stream_out().buffer_size());
}

int main() {
    try {
        auto rd = get_random_generator();

        // buffer a bunch of bytes, make sure we can empty and re-fill before calling close()
        for (unsigned rep_no = 0; rep_no < NREPS; ++rep_no) {
            StreamReassembler buf{MAX_SEG_LEN * NSEGS};

            vector<tuple<size_t, size_t>> seq_size;
            size_t offset = 0;
            for (unsigned i = 0; i < NSEGS; ++i) {
                const size_t size = 1 + (rd() % (MAX_SEG_LEN - 1));
                seq_size.emplace_back(offset, size);
                offset += size;
            }
            shuffle(seq_size.begin(), seq_size.end(), rd);

            string d(offset, 0);
            generate(d.begin(), d.end(), [&] { return rd(); });

            for (auto [off, sz] : seq_size) {
                string dd(d.cbegin() + off, d.cbegin() + off + sz);
                buf.push_substring(move(dd), off, off + sz == offset);
            }

            auto result = read(buf);
            if (buf.stream_out().bytes_written() != offset) {  // read bytes
                throw runtime_error("test 1 - number of bytes RX is incorrect");
            }
            if (!equal(result.cbegin(), result.cend(), d.cbegin())) {
                throw runtime_error("test 1 - content of RX bytes is incorrect");
            }
        }

        // insert EOF into a hole in the buffer
        for (unsigned rep_no = 0; rep_no < NREPS; ++rep_no) {
            StreamReassembler buf{65'000};

            const size_t size = 1024;
            string d(size, 0);
            generate(d.begin(), d.end(), [&] { return rd(); });

            buf.push_substring(d, 0, false);
            buf.push_substring(d.substr(10), size + 10, false);

            auto res1 = read(buf);
            if (buf.stream_out().bytes_written() != size) {
                throw runtime_error("test 3 - number of RX bytes is incorrect");
            }
            if (!equal(res1.cbegin(), res1.cend(), d.cbegin())) {
                throw runtime_error("test 3 - content of RX bytes is incorrect");
            }

            buf.push_substring(string(d.cbegin(), d.cbegin() + 7), size, false);
            buf.push_substring(string(d.cbegin() + 7, d.cbegin() + 8), size + 7, true);

            auto res2 = read(buf);
            if (buf.stream_out().bytes_written() != size + 8) {  // rx bytes
                throw runtime_error("test 3 - number of RX bytes is incorrect after 2nd read");
            }
            if (!equal(res2.cbegin(), res2.cend(), d.cbegin())) {
                throw runtime_error("test 3 - content of RX bytes is incorrect after 2nd read");
            }
        }

        // insert EOF over previously queued data, require one of two possible correct actions
        for (unsigned rep_no = 0; rep_no < NREPS; ++rep_no) {
            StreamReassembler buf{65'000};

            const size_t size = 1024;
            string d(size, 0);
            generate(d.begin(), d.end(), [&] { return rd(); });

            buf.push_substring(d, 0, false);
            buf.push_substring(d.substr(10), size + 10, false);

            auto res1 = read(buf);
            if (buf.stream_out().bytes_written() != size) {
                throw runtime_error("test 4 - number of RX bytes is incorrect");
            }
            if (!equal(res1.cbegin(), res1.cend(), d.cbegin())) {
                throw runtime_error("test 4 - content of RX bytes is incorrect");
            }

            buf.push_substring(string(d.cbegin(), d.cbegin() + 15), size, true);

            auto res2 = read(buf);
            if (buf.stream_out().bytes_written() != 2 * size && buf.stream_out().bytes_written() != size + 15) {
                throw runtime_error("test 4 - number of RX bytes is incorrect after 2nd read");
            }
            if (!equal(res2.cbegin(), res2.cend(), d.cbegin())) {
                throw runtime_error("test 4 - content of RX bytes is incorrect after 2nd read");
            }
        }
    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
