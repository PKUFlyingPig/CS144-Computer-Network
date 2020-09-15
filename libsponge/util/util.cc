#include "util.hh"

#include <array>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/socket.h>

using namespace std;

//! \returns the number of milliseconds since the program started
uint64_t timestamp_ms() {
    using time_point = std::chrono::steady_clock::time_point;
    static const time_point program_start = std::chrono::steady_clock::now();
    const time_point now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - program_start).count();
}

//! \param[in] attempt is the name of the syscall to try (for error reporting)
//! \param[in] return_value is the return value of the syscall
//! \param[in] errno_mask is any errno value that is acceptable, e.g., `EAGAIN` when reading a non-blocking fd
//! \details This function works for any syscall that returns less than 0 on error and sets errno:
//!
//! For example, to wrap a call to [open(2)](\ref man2::open), you might say:
//!
//! ~~~{.cc}
//! const int foo_fd = SystemCall("open", ::open("/tmp/foo", O_RDWR));
//! ~~~
//!
//! If you don't have permission to open the file, SystemCall will throw a std::runtime_error.
//! If you don't want to throw in that case, you can pass `EACCESS` in `errno_mask`:
//!
//! ~~~{.cc}
//! // open a file, or print an error if permission was denied
//! const int foo_fd = SystemCall("open", ::open("/tmp/foo", O_RDWR), EACCESS);
//! if (foo_fd < 0) {
//!     std::cerr << "Access to /tmp/foo was denied." << std::endl;
//! }
//! ~~~
int SystemCall(const char *attempt, const int return_value, const int errno_mask) {
    if (return_value >= 0 || errno == errno_mask) {
        return return_value;
    }

    throw unix_error(attempt);
}

//! \param[in] attempt is the name of the syscall to try (for error reporting)
//! \param[in] return_value is the return value of the syscall
//! \param[in] errno_mask is any errno value that is acceptable, e.g., `EAGAIN` when reading a non-blocking fd
//! \details see the other SystemCall() documentation for more details
int SystemCall(const string &attempt, const int return_value, const int errno_mask) {
    return SystemCall(attempt.c_str(), return_value, errno_mask);
}

//! \details A properly seeded mt19937 generator takes a lot of entropy!
//!
//! This code borrows from the following:
//!
//! - https://kristerw.blogspot.com/2017/05/seeding-stdmt19937-random-number-engine.html
//! - http://www.pcg-random.org/posts/cpps-random_device.html
mt19937 get_random_generator() {
    auto rd = random_device();
    array<uint32_t, mt19937::state_size> seed_data{};
    generate(seed_data.begin(), seed_data.end(), [&] { return rd(); });
    seed_seq seed(seed_data.begin(), seed_data.end());
    return mt19937(seed);
}

//! \note This class returns the checksum in host byte order.
//!       See https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html for rationale
//! \details This class can be used to either check or compute an Internet checksum
//! (e.g., for an IP datagram header or a TCP segment).
//!
//! The Internet checksum is defined such that evaluating inet_cksum() on a TCP segment (IP datagram, etc)
//! containing a correct checksum header will return zero. In other words, if you read a correct TCP segment
//! off the wire and pass it untouched to inet_cksum(), the return value will be 0.
//!
//! Meanwhile, to compute the checksum for an outgoing TCP segment (IP datagram, etc.), you must first set
//! the checksum header to zero, then call inet_cksum(), and finally set the checksum header to the return
//! value.
//!
//! For more information, see the [Wikipedia page](https://en.wikipedia.org/wiki/IPv4_header_checksum)
//! on the Internet checksum, and consult the [IP](\ref rfc::rfc791) and [TCP](\ref rfc::rfc793) RFCs.
InternetChecksum::InternetChecksum(const uint32_t initial_sum) : _sum(initial_sum) {}

void InternetChecksum::add(std::string_view data) {
    for (size_t i = 0; i < data.size(); i++) {
        uint16_t val = uint8_t(data[i]);
        if (not _parity) {
            val <<= 8;
        }
        _sum += val;
        _parity = !_parity;
    }
}

uint16_t InternetChecksum::value() const {
    uint32_t ret = _sum;

    while (ret > 0xffff) {
        ret = (ret >> 16) + (ret & 0xffff);
    }

    return ~ret;
}

//! \param[in] data is a pointer to the bytes to show
//! \param[in] len is the number of bytes to show
//! \param[in] indent is the number of spaces to indent
void hexdump(const uint8_t *data, const size_t len, const size_t indent) {
    const auto flags(cout.flags());
    const string indent_string(indent, ' ');
    int printed = 0;
    stringstream pchars(" ");
    cout << hex << setfill('0');
    for (unsigned i = 0; i < len; ++i) {
        if ((printed & 0xf) == 0) {
            if (printed != 0) {
                cout << "    " << pchars.str() << "\n";
                pchars.str(" ");
            }
            cout << indent_string << setw(8) << printed << ":    ";
        } else if ((printed & 1) == 0) {
            cout << ' ';
        }
        cout << setw(2) << +data[i];
        pchars << (static_cast<bool>(isprint(data[i])) ? static_cast<char>(data[i]) : '.');
        printed += 1;
    }
    const int print_rem = (16 - (printed & 0xf)) % 16;  // extra spacing before final chars
    cout << string(2 * print_rem + print_rem / 2 + 4, ' ') << pchars.str();
    cout << '\n' << endl;
    cout.flags(flags);
}

//! \param[in] data is a pointer to the bytes to show
//! \param[in] len is the number of bytes to show
//! \param[in] indent is the number of spaces to indent
void hexdump(const char *data, const size_t len, const size_t indent) {
    hexdump(reinterpret_cast<const uint8_t *>(data), len, indent);
}
