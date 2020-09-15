#ifndef SPONGE_BYTE_STREAM_HARNESS_HH
#define SPONGE_BYTE_STREAM_HARNESS_HH

#include "byte_stream.hh"
#include "util.hh"

#include <exception>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

struct ByteStreamTestStep {
    virtual operator std::string() const;
    virtual void execute(ByteStream &) const;
    virtual ~ByteStreamTestStep();
};

class ByteStreamExpectationViolation : public std::runtime_error {
  public:
    ByteStreamExpectationViolation(const std::string &msg);

    template <typename T>
    static ByteStreamExpectationViolation property(const std::string &property_name,
                                                   const T &expected,
                                                   const T &actual);
};

struct ByteStreamExpectation : public ByteStreamTestStep {
    operator std::string() const override;
    virtual std::string description() const;
    virtual void execute(ByteStream &) const override;
    virtual ~ByteStreamExpectation() override;
};

struct ByteStreamAction : public ByteStreamTestStep {
    operator std::string() const override;
    virtual std::string description() const;
    virtual void execute(ByteStream &) const override;
    virtual ~ByteStreamAction() override;
};

struct EndInput : public ByteStreamAction {
    std::string description() const override;
    void execute(ByteStream &) const override;
};

struct Write : public ByteStreamAction {
    std::string _data;
    std::optional<size_t> _bytes_written{};

    Write(const std::string &data);
    Write &with_bytes_written(const size_t bytes_written);
    std::string description() const override;
    void execute(ByteStream &) const override;
};

struct Pop : public ByteStreamAction {
    size_t _len;

    Pop(const size_t len);
    std::string description() const override;
    void execute(ByteStream &) const override;
};

struct InputEnded : public ByteStreamExpectation {
    bool _input_ended;

    InputEnded(const bool input_ended);
    std::string description() const override;
    void execute(ByteStream &) const override;
};

struct BufferEmpty : public ByteStreamExpectation {
    bool _buffer_empty;

    BufferEmpty(const bool buffer_empty);
    std::string description() const override;
    void execute(ByteStream &) const override;
};

struct Eof : public ByteStreamExpectation {
    bool _eof;

    Eof(const bool eof);
    std::string description() const override;
    void execute(ByteStream &) const override;
};

struct BufferSize : public ByteStreamExpectation {
    size_t _buffer_size;

    BufferSize(const size_t buffer_size);
    std::string description() const override;
    void execute(ByteStream &) const override;
};

struct BytesWritten : public ByteStreamExpectation {
    size_t _bytes_written;

    BytesWritten(const size_t bytes_written);
    std::string description() const override;
    void execute(ByteStream &) const override;
};

struct BytesRead : public ByteStreamExpectation {
    size_t _bytes_read;

    BytesRead(const size_t bytes_read);
    std::string description() const override;
    void execute(ByteStream &) const override;
};

struct RemainingCapacity : public ByteStreamExpectation {
    size_t _remaining_capacity;

    RemainingCapacity(const size_t remaining_capacity);
    std::string description() const override;
    void execute(ByteStream &) const override;
};

struct Peek : public ByteStreamExpectation {
    std::string _output;

    Peek(const std::string &output);
    std::string description() const override;
    void execute(ByteStream &) const override;
};

class ByteStreamTestHarness {
    std::string _test_name;
    ByteStream _byte_stream;
    std::vector<std::string> _steps_executed{};

  public:
    ByteStreamTestHarness(const std::string &test_name, const size_t capacity);

    void execute(const ByteStreamTestStep &step);
};

#endif  // SPONGE_BYTE_STREAM_HARNESS_HH
