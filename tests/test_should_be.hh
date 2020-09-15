#ifndef SPONGE_TESTS_TEST_SHOULD_BE_HH
#define SPONGE_TESTS_TEST_SHOULD_BE_HH

#include "string_conversions.hh"

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#define test_should_be(act, exp) _test_should_be(act, exp, #act, #exp, __LINE__)

template <typename T>
static void _test_should_be(const T &actual,
                            const T &expected,
                            const char *actual_s,
                            const char *expected_s,
                            const int lineno) {
    if (actual != expected) {
        std::ostringstream ss;
        ss << "`" << actual_s << "` should have been `" << expected_s << "`, but the former is\n\t" << to_string(actual)
           << "\nand the latter is\n\t" << to_string(expected) << "\n"
           << " (at line " << lineno << ")\n";
        throw std::runtime_error(ss.str());
    }
}

#endif  // SPONGE_TESTS_TEST_SHOULD_BE_HH
