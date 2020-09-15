#include "parser.hh"

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

int main() {
    try {
#include "parser_example.cc"
    } catch (...) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
