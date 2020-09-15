#include "byte_stream_test_harness.hh"

#include <memory>
#include <sstream>

using namespace std;

// ByteStreamTestStep

ByteStreamTestStep::operator std::string() const { return "ByteStreamTestStep"; }

void ByteStreamTestStep::execute(ByteStream &) const {}

ByteStreamTestStep::~ByteStreamTestStep() {}

// ByteStreamExpectationViolation

ByteStreamExpectationViolation::ByteStreamExpectationViolation(const std::string &msg) : std::runtime_error(msg) {}

template <typename T>
ByteStreamExpectationViolation ByteStreamExpectationViolation::property(const std::string &property_name,
                                                                        const T &expected,
                                                                        const T &actual) {
    return ByteStreamExpectationViolation("The ByteStream should have had " + property_name + " equal to " +
                                          to_string(expected) + " but instead it was " + to_string(actual));
}

// ByteStreamExpectation

ByteStreamExpectation::operator std::string() const { return "Expectation: " + description(); }

std::string ByteStreamExpectation::description() const { return "description missing"; }

void ByteStreamExpectation::execute(ByteStream &) const {}

ByteStreamExpectation::~ByteStreamExpectation() {}

// ByteStreamAction

ByteStreamAction::operator std::string() const { return "     Action: " + description(); }

std::string ByteStreamAction::description() const { return "description missing"; }

void ByteStreamAction::execute(ByteStream &) const {}

ByteStreamAction::~ByteStreamAction() {}

ByteStreamTestHarness::ByteStreamTestHarness(const std::string &test_name, const size_t capacity)
    : _test_name(test_name), _byte_stream(capacity) {
    std::ostringstream ss;
    ss << "Initialized with ("
       << "capacity=" << capacity << ")";
    _steps_executed.emplace_back(ss.str());
}

void ByteStreamTestHarness::execute(const ByteStreamTestStep &step) {
    try {
        step.execute(_byte_stream);
        _steps_executed.emplace_back(step);
    } catch (const ByteStreamExpectationViolation &e) {
        std::cerr << "Test Failure on expectation:\n\t" << std::string(step);
        std::cerr << "\n\nFailure message:\n\t" << e.what();
        std::cerr << "\n\nList of steps that executed successfully:";
        for (const std::string &s : _steps_executed) {
            std::cerr << "\n\t" << s;
        }
        std::cerr << std::endl << std::endl;
        throw ByteStreamExpectationViolation("The test \"" + _test_name + "\" failed");
    } catch (const exception &e) {
        std::cerr << "Test Failure on expectation:\n\t" << std::string(step);
        std::cerr << "\n\nException:\n\t" << e.what();
        std::cerr << "\n\nList of steps that executed successfully:";
        for (const std::string &s : _steps_executed) {
            std::cerr << "\n\t" << s;
        }
        std::cerr << std::endl << std::endl;
        throw ByteStreamExpectationViolation("The test \"" + _test_name +
                                             "\" caused your implementation to throw an exception!");
    }
}

// EndInput
std::string EndInput::description() const { return "end input"; }
void EndInput::execute(ByteStream &bs) const { bs.end_input(); }

// Write
Write::Write(const std::string &data) : _data(data) {}
Write &Write::with_bytes_written(const size_t bytes_written) {
    _bytes_written = bytes_written;
    return *this;
}
std::string Write::description() const { return "write \"" + _data + "\" to the stream"; }
void Write::execute(ByteStream &bs) const {
    auto bytes_written = bs.write(_data);
    if (_bytes_written and bytes_written != _bytes_written.value()) {
        throw ByteStreamExpectationViolation::property("bytes_written", _bytes_written.value(), bytes_written);
    }
}

// Pop
Pop::Pop(const size_t len) : _len(len) {}
std::string Pop::description() const { return "pop " + to_string(_len); }
void Pop::execute(ByteStream &bs) const { bs.pop_output(_len); }

// InputEnded
InputEnded::InputEnded(const bool input_ended) : _input_ended(input_ended) {}
std::string InputEnded::description() const { return "input_ended: " + to_string(_input_ended); }
void InputEnded::execute(ByteStream &bs) const {
    auto input_ended = bs.input_ended();
    if (input_ended != _input_ended) {
        throw ByteStreamExpectationViolation::property("input_ended", _input_ended, input_ended);
    }
}

// BufferEmpty
BufferEmpty::BufferEmpty(const bool buffer_empty) : _buffer_empty(buffer_empty) {}
std::string BufferEmpty::description() const { return "buffer_empty: " + to_string(_buffer_empty); }
void BufferEmpty::execute(ByteStream &bs) const {
    auto buffer_empty = bs.buffer_empty();
    if (buffer_empty != _buffer_empty) {
        throw ByteStreamExpectationViolation::property("buffer_empty", _buffer_empty, buffer_empty);
    }
}

// Eof
Eof::Eof(const bool eof) : _eof(eof) {}
std::string Eof::description() const { return "eof: " + to_string(_eof); }
void Eof::execute(ByteStream &bs) const {
    auto eof = bs.eof();
    if (eof != _eof) {
        throw ByteStreamExpectationViolation::property("eof", _eof, eof);
    }
}

// BufferSize
BufferSize::BufferSize(const size_t buffer_size) : _buffer_size(buffer_size) {}
std::string BufferSize::description() const { return "buffer_size: " + to_string(_buffer_size); }
void BufferSize::execute(ByteStream &bs) const {
    auto buffer_size = bs.buffer_size();
    if (buffer_size != _buffer_size) {
        throw ByteStreamExpectationViolation::property("buffer_size", _buffer_size, buffer_size);
    }
}

// RemainingCapacity
RemainingCapacity::RemainingCapacity(const size_t remaining_capacity) : _remaining_capacity(remaining_capacity) {}
std::string RemainingCapacity::description() const { return "remaining_capacity: " + to_string(_remaining_capacity); }
void RemainingCapacity::execute(ByteStream &bs) const {
    auto remaining_capacity = bs.remaining_capacity();
    if (remaining_capacity != _remaining_capacity) {
        throw ByteStreamExpectationViolation::property("remaining_capacity", _remaining_capacity, remaining_capacity);
    }
}

// BytesWritten
BytesWritten::BytesWritten(const size_t bytes_written) : _bytes_written(bytes_written) {}
std::string BytesWritten::description() const { return "bytes_written: " + to_string(_bytes_written); }
void BytesWritten::execute(ByteStream &bs) const {
    auto bytes_written = bs.bytes_written();
    if (bytes_written != _bytes_written) {
        throw ByteStreamExpectationViolation::property("bytes_written", _bytes_written, bytes_written);
    }
}

// BytesRead
BytesRead::BytesRead(const size_t bytes_read) : _bytes_read(bytes_read) {}
std::string BytesRead::description() const { return "bytes_read: " + to_string(_bytes_read); }
void BytesRead::execute(ByteStream &bs) const {
    auto bytes_read = bs.bytes_read();
    if (bytes_read != _bytes_read) {
        throw ByteStreamExpectationViolation::property("bytes_read", _bytes_read, bytes_read);
    }
}

// Peek
Peek::Peek(const std::string &output) : _output(output) {}
std::string Peek::description() const { return "\"" + _output + "\" at the front of the stream"; }
void Peek::execute(ByteStream &bs) const {
    auto output = bs.peek_output(_output.size());
    if (output != _output) {
        throw ByteStreamExpectationViolation("Expected \"" + _output + "\" at the front of the stream, but found \"" +
                                             output + "\"");
    }
}
