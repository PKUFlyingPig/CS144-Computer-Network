#ifndef SPONGE_RECEIVER_HARNESS_HH
#define SPONGE_RECEIVER_HARNESS_HH

#include "byte_stream.hh"
#include "tcp_receiver.hh"
#include "tcp_state.hh"
#include "util.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <exception>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

struct ReceiverTestStep {
    virtual std::string to_string() const { return "ReceiverTestStep"; }
    virtual void execute(TCPReceiver &) const {}
    virtual ~ReceiverTestStep() {}
};

class ReceiverExpectationViolation : public std::runtime_error {
  public:
    ReceiverExpectationViolation(const std::string msg) : std::runtime_error(msg) {}
};

struct ReceiverExpectation : public ReceiverTestStep {
    std::string to_string() const { return "Expectation: " + description(); }
    virtual std::string description() const { return "description missing"; }
    virtual void execute(TCPReceiver &) const {}
    virtual ~ReceiverExpectation() {}
};

struct ExpectState : public ReceiverExpectation {
    std::string _state;

    ExpectState(const std::string &state) : _state(state) {}
    std::string description() const { return "in state `" + _state + "`"; }
    void execute(TCPReceiver &receiver) const {
        if (TCPState::state_summary(receiver) != _state) {
            throw ReceiverExpectationViolation("The TCPReceiver was in state `" + TCPState::state_summary(receiver) +
                                               "`, but it was expected to be in state `" + _state + "`");
        }
    }
};

struct ExpectAckno : public ReceiverExpectation {
    std::optional<WrappingInt32> _ackno;

    ExpectAckno(std::optional<WrappingInt32> ackno) : _ackno(ackno) {}
    std::string description() const {
        if (_ackno.has_value()) {
            return "ackno " + std::to_string(_ackno.value().raw_value());
        } else {
            return "no ackno available";
        }
    }

    void execute(TCPReceiver &receiver) const {
        if (receiver.ackno() != _ackno) {
            std::string reported =
                receiver.ackno().has_value() ? std::to_string(receiver.ackno().value().raw_value()) : "none";
            std::string expected = _ackno.has_value() ? std::to_string(_ackno.value().raw_value()) : "none";
            throw ReceiverExpectationViolation("The TCPReceiver reported ackno `" + reported +
                                               "`, but it was expected to be `" + expected + "`");
        }
    }
};

struct ExpectWindow : public ReceiverExpectation {
    size_t _window;

    ExpectWindow(const size_t window) : _window(window) {}
    std::string description() const { return "window " + std::to_string(_window); }

    void execute(TCPReceiver &receiver) const {
        if (receiver.window_size() != _window) {
            std::string reported = std::to_string(receiver.window_size());
            std::string expected = std::to_string(_window);
            throw ReceiverExpectationViolation("The TCPReceiver reported window `" + reported +
                                               "`, but it was expected to be `" + expected + "`");
        }
    }
};

struct ExpectUnassembledBytes : public ReceiverExpectation {
    size_t _n_bytes;

    ExpectUnassembledBytes(size_t n_bytes) : _n_bytes(n_bytes) {}
    std::string description() const { return std::to_string(_n_bytes) + " unassembled bytes"; }

    void execute(TCPReceiver &receiver) const {
        if (receiver.unassembled_bytes() != _n_bytes) {
            std::ostringstream ss;
            ss << "The TCPReceiver reported `" << receiver.unassembled_bytes()
               << "` unassembled bytes, but there was expected to be `" << _n_bytes << "` unassembled bytes";
            throw ReceiverExpectationViolation(ss.str());
        }
    }
};

struct ExpectTotalAssembledBytes : public ReceiverExpectation {
    size_t _n_bytes;

    ExpectTotalAssembledBytes(size_t n_bytes) : _n_bytes(n_bytes) {}
    std::string description() const { return std::to_string(_n_bytes) + " assembled bytes, in total"; }

    void execute(TCPReceiver &receiver) const {
        if (receiver.stream_out().bytes_written() != _n_bytes) {
            std::ostringstream ss;
            ss << "The TCPReceiver stream reported `" << receiver.stream_out().bytes_written()
               << "` bytes written, but there was expected to be `" << _n_bytes << "` bytes written (in total)";
            throw ReceiverExpectationViolation(ss.str());
        }
    }
};

struct ExpectEof : public ReceiverExpectation {
    ExpectEof() {}
    std::string description() const { return "receiver.stream_out().eof() == true"; }

    void execute(TCPReceiver &receiver) const {
        if (not receiver.stream_out().eof()) {
            throw ReceiverExpectationViolation(
                "The TCPReceiver stream reported eof() == false, but was expected to be true");
        }
    }
};

struct ExpectInputNotEnded : public ReceiverExpectation {
    ExpectInputNotEnded() {}
    std::string description() const { return "receiver.stream_out().input_ended() == false"; }

    void execute(TCPReceiver &receiver) const {
        if (receiver.stream_out().input_ended()) {
            throw ReceiverExpectationViolation(
                "The TCPReceiver stream reported input_ended() == true, but was expected to be false");
        }
    }
};

struct ExpectBytes : public ReceiverExpectation {
    std::string _bytes;

    ExpectBytes(std::string &&bytes) : _bytes(std::move(bytes)) {}
    std::string description() const {
        std::ostringstream ss;
        ss << "bytes available: \"" << _bytes << "\"";
        return ss.str();
    }

    void execute(TCPReceiver &receiver) const {
        ByteStream &stream = receiver.stream_out();
        if (stream.buffer_size() != _bytes.size()) {
            std::ostringstream ss;
            ss << "The TCPReceiver reported `" << stream.buffer_size()
               << "` bytes available, but there were expected to be `" << _bytes.size() << "` bytes available";
            throw ReceiverExpectationViolation(ss.str());
        }
        std::string bytes = stream.read(_bytes.size());
        if (not std::equal(bytes.begin(), bytes.end(), _bytes.begin(), _bytes.end())) {
            std::ostringstream ss;
            ss << "the TCPReceiver assembled \"" << bytes << "\", but was expected to assemble \"" << _bytes << "\".";
            throw ReceiverExpectationViolation(ss.str());
        }
    }
};

struct ReceiverAction : public ReceiverTestStep {
    std::string to_string() const { return "Action:      " + description(); }
    virtual std::string description() const { return "description missing"; }
    virtual void execute(TCPReceiver &) const {}
    virtual ~ReceiverAction() {}
};

struct SegmentArrives : public ReceiverAction {
    enum class Result { NOT_SYN, OK };

    static std::string result_name(Result res) {
        switch (res) {
            case Result::NOT_SYN:
                return "(no SYN received, so no ackno available)";
            case Result::OK:
                return "(SYN received, so ackno available)";
            default:
                return "unknown";
        }
    }

    bool ack{};
    bool rst{};
    bool syn{};
    bool fin{};
    WrappingInt32 seqno{0};
    WrappingInt32 ackno{0};
    uint16_t win{};
    std::string data{};
    std::optional<Result> result{};

    SegmentArrives &with_ack(WrappingInt32 ackno_) {
        ack = true;
        ackno = ackno_;
        return *this;
    }

    SegmentArrives &with_ack(uint32_t ackno_) { return with_ack(WrappingInt32{ackno_}); }

    SegmentArrives &with_rst() {
        rst = true;
        return *this;
    }

    SegmentArrives &with_syn() {
        syn = true;
        return *this;
    }

    SegmentArrives &with_fin() {
        fin = true;
        return *this;
    }

    SegmentArrives &with_seqno(WrappingInt32 seqno_) {
        seqno = seqno_;
        return *this;
    }

    SegmentArrives &with_seqno(uint32_t seqno_) { return with_seqno(WrappingInt32{seqno_}); }

    SegmentArrives &with_win(uint16_t win_) {
        win = win_;
        return *this;
    }

    SegmentArrives &with_data(std::string data_) {
        data = data_;
        return *this;
    }

    SegmentArrives &with_result(Result result_) {
        result = result_;
        return *this;
    }

    TCPSegment build_segment() const {
        TCPSegment seg;
        seg.payload() = std::string(data);
        seg.header().ack = ack;
        seg.header().fin = fin;
        seg.header().syn = syn;
        seg.header().rst = rst;
        seg.header().ackno = ackno;
        seg.header().seqno = seqno;
        seg.header().win = win;
        return seg;
    }

    std::string description() const override {
        TCPSegment seg = build_segment();
        std::ostringstream o;
        o << "segment arrives ";
        o << seg.header().summary();
        if (data.size() > 0) {
            o << " with data \"" << data << "\"";
        }
        return o.str();
    }

    void execute(TCPReceiver &receiver) const override {
        TCPSegment seg = build_segment();
        std::ostringstream o;
        o << seg.header().summary();
        if (data.size() > 0) {
            o << " with data \"" << data << "\"";
        }

        receiver.segment_received(std::move(seg));

        Result res;

        if (not receiver.ackno().has_value()) {
            res = Result::NOT_SYN;
        } else {
            res = Result::OK;
        }

        if (result.has_value() and result.value() != res) {
            throw ReceiverExpectationViolation("TCPReceiver::segment_received() reported `" + result_name(res) +
                                               "` in response to `" + o.str() + "`, but it was expected to report `" +
                                               result_name(result.value()) + "`");
        }
    }
};

class TCPReceiverTestHarness {
    TCPReceiver receiver;
    std::vector<std::string> steps_executed;

  public:
    TCPReceiverTestHarness(size_t capacity) : receiver(capacity), steps_executed() {
        std::ostringstream ss;
        ss << "Initialized with ("
           << "capacity=" << capacity << ")";
        steps_executed.emplace_back(ss.str());
    }
    void execute(const ReceiverTestStep &step) {
        try {
            step.execute(receiver);
            steps_executed.emplace_back(step.to_string());
        } catch (const ReceiverExpectationViolation &e) {
            std::cerr << "Test Failure on expectation:\n\t" << step.to_string();
            std::cerr << "\n\nFailure message:\n\t" << e.what();
            std::cerr << "\n\nList of steps that executed successfully:";
            for (const std::string &s : steps_executed) {
                std::cerr << "\n\t" << s;
            }
            std::cerr << std::endl << std::endl;
            throw e;
        }
    }
};

#endif  // SPONGE_RECEIVER_HARNESS_HH
