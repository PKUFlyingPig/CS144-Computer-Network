#ifndef SPONGE_LIBSPONGE_FILE_DESCRIPTOR_HH
#define SPONGE_LIBSPONGE_FILE_DESCRIPTOR_HH

#include "buffer.hh"

#include <array>
#include <cstddef>
#include <limits>
#include <memory>

//! A reference-counted handle to a file descriptor
class FileDescriptor {
    //! \brief A handle on a kernel file descriptor.
    //! \details FileDescriptor objects contain a std::shared_ptr to a FDWrapper.
    class FDWrapper {
      public:
        int _fd;                    //!< The file descriptor number returned by the kernel
        bool _eof = false;          //!< Flag indicating whether FDWrapper::_fd is at EOF
        bool _closed = false;       //!< Flag indicating whether FDWrapper::_fd has been closed
        unsigned _read_count = 0;   //!< The number of times FDWrapper::_fd has been read
        unsigned _write_count = 0;  //!< The numberof times FDWrapper::_fd has been written

        //! Construct from a file descriptor number returned by the kernel
        explicit FDWrapper(const int fd);
        //! Closes the file descriptor upon destruction
        ~FDWrapper();
        //! Calls [close(2)](\ref man2::close) on FDWrapper::_fd
        void close();

        //! \name
        //! An FDWrapper cannot be copied or moved

        //!@{
        FDWrapper(const FDWrapper &other) = delete;
        FDWrapper &operator=(const FDWrapper &other) = delete;
        FDWrapper(FDWrapper &&other) = delete;
        FDWrapper &operator=(FDWrapper &&other) = delete;
        //!@}
    };

    //! A reference-counted handle to a shared FDWrapper
    std::shared_ptr<FDWrapper> _internal_fd;

    // private constructor used to duplicate the FileDescriptor (increase the reference count)
    explicit FileDescriptor(std::shared_ptr<FDWrapper> other_shared_ptr);

  protected:
    void register_read() { ++_internal_fd->_read_count; }    //!< increment read count
    void register_write() { ++_internal_fd->_write_count; }  //!< increment write count

  public:
    //! Construct from a file descriptor number returned by the kernel
    explicit FileDescriptor(const int fd);

    //! Free the std::shared_ptr; the FDWrapper destructor calls close() when the refcount goes to zero.
    ~FileDescriptor() = default;

    //! Read up to `limit` bytes
    std::string read(const size_t limit = std::numeric_limits<size_t>::max());

    //! Read up to `limit` bytes into `str` (caller can allocate storage)
    void read(std::string &str, const size_t limit = std::numeric_limits<size_t>::max());

    //! Write a string, possibly blocking until all is written
    size_t write(const char *str, const bool write_all = true) { return write(BufferViewList(str), write_all); }

    //! Write a string, possibly blocking until all is written
    size_t write(const std::string &str, const bool write_all = true) { return write(BufferViewList(str), write_all); }

    //! Write a buffer (or list of buffers), possibly blocking until all is written
    size_t write(BufferViewList buffer, const bool write_all = true);

    //! Close the underlying file descriptor
    void close() { _internal_fd->close(); }

    //! Copy a FileDescriptor explicitly, increasing the FDWrapper refcount
    FileDescriptor duplicate() const;

    //! Set blocking(true) or non-blocking(false)
    void set_blocking(const bool blocking_state);

    //! \name FDWrapper accessors
    //!@{

    //! underlying descriptor number
    int fd_num() const { return _internal_fd->_fd; }

    //! EOF flag state
    bool eof() const { return _internal_fd->_eof; }

    //! closed flag state
    bool closed() const { return _internal_fd->_closed; }

    //! number of reads
    unsigned int read_count() const { return _internal_fd->_read_count; }

    //! number of writes
    unsigned int write_count() const { return _internal_fd->_write_count; }
    //!@}

    //! \name Copy/move constructor/assignment operators
    //! FileDescriptor can be moved, but cannot be copied (but see duplicate())
    //!@{
    FileDescriptor(const FileDescriptor &other) = delete;             //!< \brief copy construction is forbidden
    FileDescriptor &operator=(const FileDescriptor &other) = delete;  //!< \brief copy assignment is forbidden
    FileDescriptor(FileDescriptor &&other) = default;                 //!< \brief move construction is allowed
    FileDescriptor &operator=(FileDescriptor &&other) = default;      //!< \brief move assignment is allowed
    //!@}
};

//! \class FileDescriptor
//! In addition, FileDescriptor tracks EOF state and calls to FileDescriptor::read and
//! FileDescriptor::write, which EventLoop uses to detect busy loop conditions.
//!
//! For an example of FileDescriptor use, see the EventLoop class documentation.

#endif  // SPONGE_LIBSPONGE_FILE_DESCRIPTOR_HH
