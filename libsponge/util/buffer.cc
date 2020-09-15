#include "buffer.hh"

using namespace std;

void Buffer::remove_prefix(const size_t n) {
    if (n > str().size()) {
        throw out_of_range("Buffer::remove_prefix");
    }
    _starting_offset += n;
    if (_storage and _starting_offset == _storage->size()) {
        _storage.reset();
    }
}

void BufferList::append(const BufferList &other) {
    for (const auto &buf : other._buffers) {
        _buffers.push_back(buf);
    }
}

BufferList::operator Buffer() const {
    switch (_buffers.size()) {
        case 0:
            return {};
        case 1:
            return _buffers[0];
        default: {
            throw runtime_error(
                "BufferList: please use concatenate() to combine a multi-Buffer BufferList into one Buffer");
        }
    }
}

string BufferList::concatenate() const {
    std::string ret;
    ret.reserve(size());
    for (const auto &buf : _buffers) {
        ret.append(buf);
    }
    return ret;
}

size_t BufferList::size() const {
    size_t ret = 0;
    for (const auto &buf : _buffers) {
        ret += buf.size();
    }
    return ret;
}

void BufferList::remove_prefix(size_t n) {
    while (n > 0) {
        if (_buffers.empty()) {
            throw std::out_of_range("BufferList::remove_prefix");
        }

        if (n < _buffers.front().str().size()) {
            _buffers.front().remove_prefix(n);
            n = 0;
        } else {
            n -= _buffers.front().str().size();
            _buffers.pop_front();
        }
    }
}

BufferViewList::BufferViewList(const BufferList &buffers) {
    for (const auto &x : buffers.buffers()) {
        _views.push_back(x);
    }
}

void BufferViewList::remove_prefix(size_t n) {
    while (n > 0) {
        if (_views.empty()) {
            throw std::out_of_range("BufferListView::remove_prefix");
        }

        if (n < _views.front().size()) {
            _views.front().remove_prefix(n);
            n = 0;
        } else {
            n -= _views.front().size();
            _views.pop_front();
        }
    }
}

size_t BufferViewList::size() const {
    size_t ret = 0;
    for (const auto &buf : _views) {
        ret += buf.size();
    }
    return ret;
}

vector<iovec> BufferViewList::as_iovecs() const {
    vector<iovec> ret;
    ret.reserve(_views.size());
    for (const auto &x : _views) {
        ret.push_back({const_cast<char *>(x.data()), x.size()});
    }
    return ret;
}
