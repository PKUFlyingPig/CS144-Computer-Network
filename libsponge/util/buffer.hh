#ifndef SPONGE_LIBSPONGE_BUFFER_HH
#define SPONGE_LIBSPONGE_BUFFER_HH

#include <algorithm>
#include <deque>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <sys/uio.h>
#include <vector>

//! \brief A reference-counted read-only string that can discard bytes from the front
class Buffer {
  private:
    std::shared_ptr<std::string> _storage{};
    size_t _starting_offset{};

  public:
    Buffer() = default;

    //! \brief Construct by taking ownership of a string
    Buffer(std::string &&str) noexcept : _storage(std::make_shared<std::string>(std::move(str))) {}

    //! \name Expose contents as a std::string_view
    //!@{
    std::string_view str() const {
        if (not _storage) {
            return {};
        }
        return {_storage->data() + _starting_offset, _storage->size() - _starting_offset};
    }

    operator std::string_view() const { return str(); }
    //!@}

    //! \brief Get character at location `n`
    uint8_t at(const size_t n) const { return str().at(n); }

    //! \brief Size of the string
    size_t size() const { return str().size(); }

    //! \brief Make a copy to a new std::string
    std::string copy() const { return std::string(str()); }

    //! \brief Discard the first `n` bytes of the string (does not require a copy or move)
    //! \note Doesn't free any memory until the whole string has been discarded in all copies of the Buffer.
    void remove_prefix(const size_t n);
};

//! \brief A reference-counted discontiguous string that can discard bytes from the front
//! \note Used to model packets that contain multiple sets of headers
//! + a payload. This allows us to prepend headers (e.g., to
//! encapsulate a TCP payload in a TCPSegment, and then encapsulate
//! the TCPSegment in an IPv4Datagram) without copying the payload.
class BufferList {
  private:
    std::deque<Buffer> _buffers{};

  public:
    //! \name Constructors
    //!@{

    BufferList() = default;

    //! \brief Construct from a Buffer
    BufferList(Buffer buffer) : _buffers{buffer} {}

    //! \brief Construct by taking ownership of a std::string
    BufferList(std::string &&str) noexcept {
        Buffer buf{std::move(str)};
        append(buf);
    }
    //!@}

    //! \brief Access the underlying queue of Buffers
    const std::deque<Buffer> &buffers() const { return _buffers; }

    //! \brief Append a BufferList
    void append(const BufferList &other);

    //! \brief Transform to a Buffer
    //! \note Throws an exception unless BufferList is contiguous
    operator Buffer() const;

    //! \brief Discard the first `n` bytes of the string (does not require a copy or move)
    void remove_prefix(size_t n);

    //! \brief Size of the string
    size_t size() const;

    //! \brief Make a copy to a new std::string
    std::string concatenate() const;
};

//! \brief A non-owning temporary view (similar to std::string_view) of a discontiguous string
class BufferViewList {
    std::deque<std::string_view> _views{};

  public:
    //! \name Constructors
    //!@{

    //! \brief Construct from a std::string
    BufferViewList(const std::string &str) : BufferViewList(std::string_view(str)) {}

    //! \brief Construct from a C string (must be NULL-terminated)
    BufferViewList(const char *s) : BufferViewList(std::string_view(s)) {}

    //! \brief Construct from a BufferList
    BufferViewList(const BufferList &buffers);

    //! \brief Construct from a std::string_view
    BufferViewList(std::string_view str) { _views.push_back({const_cast<char *>(str.data()), str.size()}); }
    //!@}

    //! \brief Discard the first `n` bytes of the string (does not require a copy or move)
    void remove_prefix(size_t n);

    //! \brief Size of the string
    size_t size() const;

    //! \brief Convert to a vector of `iovec` structures
    //! \note used for system calls that write discontiguous buffers,
    //! e.g. [writev(2)](\ref man2::writev) and [sendmsg(2)](\ref man2::sendmsg)
    std::vector<iovec> as_iovecs() const;
};

#endif  // SPONGE_LIBSPONGE_BUFFER_HH
