#include "util.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;

void check_roundtrip(const WrappingInt32 isn, const uint64_t value, const uint64_t checkpoint) {
    if (unwrap(wrap(value, isn), isn, checkpoint) != value) {
        ostringstream ss;

        ss << "Expected unwrap(wrap()) to recover same value, and it didn't!\n";
        ss << "  unwrap(wrap(value, isn), isn, checkpoint) did not equal value\n";
        ss << "  where value = " << value << ", isn = " << isn << ", and checkpoint = " << checkpoint << "\n";
        ss << "  (Difference between value and checkpoint is " << value - checkpoint << ".)\n";
        throw runtime_error(ss.str());
    }
}

int main() {
    try {
        auto rd = get_random_generator();
        uniform_int_distribution<uint32_t> dist31minus1{0, (uint32_t{1} << 31) - 1};
        uniform_int_distribution<uint32_t> dist32{0, numeric_limits<uint32_t>::max()};
        uniform_int_distribution<uint64_t> dist63{0, uint64_t{1} << 63};

        const uint64_t big_offset = (uint64_t{1} << 31) - 1;

        for (unsigned int i = 0; i < 1000000; i++) {
            const WrappingInt32 isn{dist32(rd)};
            const uint64_t val{dist63(rd)};
            const uint64_t offset{dist31minus1(rd)};

            check_roundtrip(isn, val, val);
            check_roundtrip(isn, val + 1, val);
            check_roundtrip(isn, val - 1, val);
            check_roundtrip(isn, val + offset, val);
            check_roundtrip(isn, val - offset, val);
            check_roundtrip(isn, val + big_offset, val);
            check_roundtrip(isn, val - big_offset, val);
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
