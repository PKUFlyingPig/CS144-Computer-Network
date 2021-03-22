#include "test_should_be.hh"
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
        test_should_be(wrap(3 * (1ll << 32), WrappingInt32(0)), WrappingInt32(0));
        test_should_be(wrap(3 * (1ll << 32) + 17, WrappingInt32(15)), WrappingInt32(32));
        test_should_be(wrap(7 * (1ll << 32) - 2, WrappingInt32(15)), WrappingInt32(13));
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
