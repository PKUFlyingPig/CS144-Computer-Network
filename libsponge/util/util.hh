#ifndef SPONGE_LIBSPONGE_UTIL_HH
#define SPONGE_LIBSPONGE_UTIL_HH

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <random>
#include <string>
#include <system_error>
#include <vector>

//! std::system_error plus the name of what was being attempted
class tagged_error : public std::system_error {
  private:
    std::string _attempt_and_error;  //!< What was attempted, and what happened

  public:
    //! \brief Construct from a category, an attempt, and an error code
    //! \param[in] category is the category of error
    //! \param[in] attempt is what was supposed to happen
    //! \param[in] error_code is the resulting error
    tagged_error(const std::error_category &category, const std::string &attempt, const int error_code)
        : system_error(error_code, category), _attempt_and_error(attempt + ": " + std::system_error::what()) {}

    //! Returns a C string describing the error
    const char *what() const noexcept override { return _attempt_and_error.c_str(); }
};

//! a tagged_error for syscalls
class unix_error : public tagged_error {
  public:
    //! brief Construct from a syscall name and the resulting errno
    //! \param[in] attempt is the name of the syscall attempted
    //! \param[in] error is the [errno(3)](\ref man3::errno) that resulted
    explicit unix_error(const std::string &attempt, const int error = errno)
        : tagged_error(std::system_category(), attempt, error) {}
};

//! Error-checking wrapper for most syscalls
int SystemCall(const char *attempt, const int return_value, const int errno_mask = 0);

//! Version of SystemCall that takes a C++ std::string
int SystemCall(const std::string &attempt, const int return_value, const int errno_mask = 0);

//! Seed a fast random generator
std::mt19937 get_random_generator();

//! Get the time in milliseconds since the program began.
uint64_t timestamp_ms();

//! The internet checksum algorithm
class InternetChecksum {
  private:
    uint32_t _sum;
    bool _parity{};

  public:
    InternetChecksum(const uint32_t initial_sum = 0);
    void add(std::string_view data);
    uint16_t value() const;
};

//! Hexdump the contents of a packet (or any other sequence of bytes)
void hexdump(const char *data, const size_t len, const size_t indent = 0);

//! Hexdump the contents of a packet (or any other sequence of bytes)
void hexdump(const uint8_t *data, const size_t len, const size_t indent = 0);

#endif  // SPONGE_LIBSPONGE_UTIL_HH
