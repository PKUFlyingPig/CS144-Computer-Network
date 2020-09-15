#include "file_descriptor.hh"

#include "util.hh"

#include <algorithm>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <sys/uio.h>
#include <unistd.h>

using namespace std;

//! \param[in] fd is the file descriptor number returned by [open(2)](\ref man2::open) or similar
FileDescriptor::FDWrapper::FDWrapper(const int fd) : _fd(fd) {
    if (fd < 0) {
        throw runtime_error("invalid fd number:" + to_string(fd));
    }
}

void FileDescriptor::FDWrapper::close() {
    SystemCall("close", ::close(_fd));
    _eof = _closed = true;
}

FileDescriptor::FDWrapper::~FDWrapper() {
    try {
        if (_closed) {
            return;
        }
        close();
    } catch (const exception &e) {
        // don't throw an exception from the destructor
        std::cerr << "Exception destructing FDWrapper: " << e.what() << std::endl;
    }
}

//! \param[in] fd is the file descriptor number returned by [open(2)](\ref man2::open) or similar
FileDescriptor::FileDescriptor(const int fd) : _internal_fd(make_shared<FDWrapper>(fd)) {}

//! Private constructor used by duplicate()
FileDescriptor::FileDescriptor(shared_ptr<FDWrapper> other_shared_ptr) : _internal_fd(move(other_shared_ptr)) {}

//! \returns a copy of this FileDescriptor
FileDescriptor FileDescriptor::duplicate() const { return FileDescriptor(_internal_fd); }

//! \param[in] limit is the maximum number of bytes to read; fewer bytes may be returned
//! \param[out] str is the string to be read
void FileDescriptor::read(std::string &str, const size_t limit) {
    constexpr size_t BUFFER_SIZE = 1024 * 1024;  // maximum size of a read
    const size_t size_to_read = min(BUFFER_SIZE, limit);
    str.resize(size_to_read);

    ssize_t bytes_read = SystemCall("read", ::read(fd_num(), str.data(), size_to_read));
    if (limit > 0 && bytes_read == 0) {
        _internal_fd->_eof = true;
    }
    if (bytes_read > static_cast<ssize_t>(size_to_read)) {
        throw runtime_error("read() read more than requested");
    }
    str.resize(bytes_read);

    register_read();
}

//! \param[in] limit is the maximum number of bytes to read; fewer bytes may be returned
//! \returns a vector of bytes read
string FileDescriptor::read(const size_t limit) {
    string ret;

    read(ret, limit);

    return ret;
}

size_t FileDescriptor::write(BufferViewList buffer, const bool write_all) {
    size_t total_bytes_written = 0;

    do {
        auto iovecs = buffer.as_iovecs();

        const ssize_t bytes_written = SystemCall("writev", ::writev(fd_num(), iovecs.data(), iovecs.size()));
        if (bytes_written == 0 and buffer.size() != 0) {
            throw runtime_error("write returned 0 given non-empty input buffer");
        }

        if (bytes_written > ssize_t(buffer.size())) {
            throw runtime_error("write wrote more than length of input buffer");
        }

        register_write();

        buffer.remove_prefix(bytes_written);

        total_bytes_written += bytes_written;
    } while (write_all and buffer.size());

    return total_bytes_written;
}

void FileDescriptor::set_blocking(const bool blocking_state) {
    int flags = SystemCall("fcntl", fcntl(fd_num(), F_GETFL));
    if (blocking_state) {
        flags ^= (flags & O_NONBLOCK);
    } else {
        flags |= O_NONBLOCK;
    }

    SystemCall("fcntl", fcntl(fd_num(), F_SETFL, flags));
}
