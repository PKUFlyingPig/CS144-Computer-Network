#include "test_should_be.hh"
#include "util.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace std;

int main() {
    try {
        // Comparing low-number adjacent seqnos
        test_should_be(WrappingInt32(3) != WrappingInt32(1), true);
        test_should_be(WrappingInt32(3) == WrappingInt32(1), false);

        size_t N_REPS = 4096;

        auto rd = get_random_generator();

        for (size_t i = 0; i < N_REPS; i++) {
            uint32_t n = rd();
            uint8_t diff = rd();
            uint32_t m = n + diff;
            test_should_be(WrappingInt32(n) == WrappingInt32(m), n == m);
            test_should_be(WrappingInt32(n) != WrappingInt32(m), n != m);
        }

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
