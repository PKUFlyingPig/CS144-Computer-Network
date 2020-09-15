#include "address.hh"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    try {
#include "address_example_1.cc"
#include "address_example_2.cc"
#include "address_example_3.cc"
        if ((google_webserver.port() != 443) || (a_dns_server_numeric != 0x12'47'00'97)) {
            throw std::runtime_error("unexpected value");
        }
    } catch (const std::exception &e) {
        std::cerr << "This test requires Internet access and working DNS.\n";
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
