#include "byte_stream.hh"

#include <algorithm>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capa)
    : buffer(), capacity(capa), end_write(false), end_read(false), written_bytes(0), read_bytes(0) {}

size_t ByteStream::write(const string &data) {
    size_t canWrite = capacity - buffer.size();
    size_t realWrite = min(canWrite, data.length());
    for (size_t i = 0; i < realWrite; i++) {
        buffer.push_back(data[i]);
    }
    written_bytes += realWrite;
    return realWrite;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t canPeek = min(len, buffer.size());
    string out = "";
    for (size_t i = 0; i < canPeek; i++) {
        out += buffer[i];
    }
    return out;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len > buffer.size()) {
        set_error();
        return;
    }
    for (size_t i = 0; i < len; i++) {
        buffer.pop_front();
    }
    read_bytes += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string out = "";
    if (len > buffer.size()) {
        set_error();
        return out;
    }
    for (size_t i = 0; i < len; i++) {
        out += buffer.front();
        buffer.pop_front();
    }
    read_bytes += len;
    return out;
}

void ByteStream::end_input() { end_write = true; }

bool ByteStream::input_ended() const { return end_write; }

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer.empty(); }

bool ByteStream::eof() const { return buffer.empty() && end_write; }

size_t ByteStream::bytes_written() const { return written_bytes; }

size_t ByteStream::bytes_read() const { return read_bytes; }

size_t ByteStream::remaining_capacity() const { return capacity - buffer.size(); }
