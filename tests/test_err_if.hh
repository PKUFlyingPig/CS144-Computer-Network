#ifndef SPONGE_TESTS_TEST_ERR_IF_HH
#define SPONGE_TESTS_TEST_ERR_IF_HH

#include <stdexcept>
#include <string>

static int err_num = 1;

#define test_err_if(c, s) _test_err_if(c, s, __LINE__)

static void _test_err_if(const bool err_condition, const std::string &err_string, const int lineno) {
    if (err_condition) {
        throw std::runtime_error(err_string + " (at line " + std::to_string(lineno) + ")");
    }
    ++err_num;
}

#endif  // SPONGE_TESTS_TEST_ERR_IF_HH
