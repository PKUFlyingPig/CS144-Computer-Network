#ifndef SPONGE_SENDER_HARNESS_HH
#define SPONGE_SENDER_HARNESS_HH

#include "byte_stream.hh"
#include "string_conversions.hh"
#include "tcp_sender.hh"
#include "tcp_state.hh"
#include "util.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <deque>
#include <exception>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

const unsigned int DEFAULT_TEST_WINDOW = 137;

struct SenderTestStep {
    virtual operator std::string() const { return "SenderTestStep"; }
    virtual void execute(TCPSender &, std::queue<TCPSegment> &) const {}
    virtual ~SenderTestStep() {}
};

class SenderExpectationViolation : public std::runtime_error {
  public:
    SenderExpectationViolation(const std::string msg) : std::runtime_error(msg) {}
};

class SegmentExpectationViolation : public SenderExpectationViolation {
  public:
    SegmentExpectationViolation(const std::string &msg) : SenderExpectationViolation(msg) {}
    static SegmentExpectationViolation violated_verb(const std::string &msg) {
        return SegmentExpectationViolation{"The Sender should have produced a segment that " + msg +
                                           ", but it did not"};
    }
    template <typename T>
    static SegmentExpectationViolation violated_field(const std::string &field_name,
                                                      const T &expected_value,
                                                      const T &actual_value) {
        std::stringstream ss{};
        ss << "The Sender produced a segment with `" << field_name << " = " << actual_value << "`, but " << field_name
           << " was expected to be `" << expected_value << "`";
        return SegmentExpectationViolation{ss.str()};
    }
};

struct SenderExpectation : public SenderTestStep {
    operator std::string() const { return "Expectation: " + description(); }
    virtual std::string description() const { return "description missing"; }
    virtual void execute(TCPSender &, std::queue<TCPSegment> &) const {}
    virtual ~SenderExpectation() {}
};

struct ExpectState : public SenderExpectation {
    std::string _state;

    ExpectState(const std::string &state) : _state(state) {}
    std::string description() const { return "in state `" + _state + "`"; }
    void execute(TCPSender &sender, std::queue<TCPSegment> &) const {
        if (TCPState::state_summary(sender) != _state) {
            throw SenderExpectationViolation("The TCPSender was in state `" + TCPState::state_summary(sender) +
                                             "`, but it was expected to be in state `" + _state + "`");
        }
    }
};

struct ExpectSeqno : public SenderExpectation {
    WrappingInt32 _seqno;

    ExpectSeqno(WrappingInt32 seqno) : _seqno(seqno) {}
    std::string description() const { return "next seqno " + std::to_string(_seqno.raw_value()); }

    void execute(TCPSender &sender, std::queue<TCPSegment> &) const {
        if (sender.next_seqno() != _seqno) {
            std::string reported = std::to_string(sender.next_seqno().raw_value());
            std::string expected = to_string(_seqno);
            throw SenderExpectationViolation("The TCPSender reported that the next seqno is `" + reported +
                                             "`, but it was expected to be `" + expected + "`");
        }
    }
};

struct ExpectBytesInFlight : public SenderExpectation {
    size_t _n_bytes;

    ExpectBytesInFlight(size_t n_bytes) : _n_bytes(n_bytes) {}
    std::string description() const { return std::to_string(_n_bytes) + " bytes in flight"; }

    void execute(TCPSender &sender, std::queue<TCPSegment> &) const {
        if (sender.bytes_in_flight() != _n_bytes) {
            std::ostringstream ss;
            ss << "The TCPSender reported " << sender.bytes_in_flight()
               << " bytes in flight, but there was expected to be " << _n_bytes << " bytes in flight";
            throw SenderExpectationViolation(ss.str());
        }
    }
};

struct ExpectNoSegment : public SenderExpectation {
    ExpectNoSegment() {}
    std::string description() const { return "no (more) segments"; }

    void execute(TCPSender &, std::queue<TCPSegment> &segments) const {
        if (not segments.empty()) {
            std::ostringstream ss;
            ss << "The TCPSender sent a segment, but should not have. Segment info:\n\t";
            TCPSegment &seg = segments.back();
            ss << seg.header().summary();
            ss << " with " << seg.payload().size() << " bytes";
            throw SenderExpectationViolation(ss.str());
        }
    }
};

struct SenderAction : public SenderTestStep {
    operator std::string() const { return "Action:      " + description(); }
    virtual std::string description() const { return "description missing"; }
    virtual void execute(TCPSender &, std::queue<TCPSegment> &) const {}
    virtual ~SenderAction() {}
};

struct WriteBytes : public SenderAction {
    std::string _bytes;
    bool _end_input;

    WriteBytes(std::string &&bytes) : _bytes(std::move(bytes)), _end_input(false) {}
    std::string description() const {
        std::ostringstream ss;
        ss << "write bytes: \"" << _bytes.substr(0, 16) << ((_bytes.size() > 16) ? "..." : "") << "\"";
        if (_end_input) {
            ss << " + EOF";
        }
        return ss.str();
    }

    void execute(TCPSender &sender, std::queue<TCPSegment> &) const {
        sender.stream_in().write(std::move(_bytes));
        if (_end_input) {
            sender.stream_in().end_input();
        }
        sender.fill_window();
    }

    WriteBytes &with_end_input(const bool end_input) {
        _end_input = end_input;
        return *this;
    }
};

struct Tick : public SenderAction {
    size_t _ms;
    std::optional<bool> max_retx_exceeded{};

    Tick(size_t ms) : _ms(ms) {}

    Tick &with_max_retx_exceeded(bool max_retx_exceeded_) {
        max_retx_exceeded = max_retx_exceeded_;
        return *this;
    }

    std::string description() const {
        std::ostringstream ss;
        ss << _ms << " ms pass";
        if (max_retx_exceeded.has_value()) {
            ss << " with max_retx_exceeded = " << max_retx_exceeded.value();
        }
        return ss.str();
    }

    void execute(TCPSender &sender, std::queue<TCPSegment> &) const {
        sender.tick(_ms);
        if (max_retx_exceeded.has_value() and
            max_retx_exceeded != (sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)) {
            std::ostringstream ss;
            ss << "after " << _ms << "ms passed the TCP Sender reported\n\tconsecutive_retransmissions = "
               << sender.consecutive_retransmissions() << "\nbut it should have been\n\t";
            if (max_retx_exceeded.value()) {
                ss << "greater than ";
            } else {
                ss << "less than or equal to ";
            }
            ss << TCPConfig::MAX_RETX_ATTEMPTS << std::endl;
            throw SenderExpectationViolation(ss.str());
        }
    }
};

struct AckReceived : public SenderAction {
    WrappingInt32 _ackno;
    std::optional<uint16_t> _window_advertisement{};

    AckReceived(WrappingInt32 ackno) : _ackno(ackno) {}
    std::string description() const {
        std::ostringstream ss;
        ss << "ack " << _ackno.raw_value() << " winsize " << _window_advertisement.value_or(DEFAULT_TEST_WINDOW);
        return ss.str();
    }

    AckReceived &with_win(uint16_t win) {
        _window_advertisement.emplace(win);
        return *this;
    }

    void execute(TCPSender &sender, std::queue<TCPSegment> &) const {
        sender.ack_received(_ackno, _window_advertisement.value_or(DEFAULT_TEST_WINDOW));
        sender.fill_window();
    }
};

struct Close : public SenderAction {
    Close() {}
    std::string description() const { return "close"; }

    void execute(TCPSender &sender, std::queue<TCPSegment> &) const {
        sender.stream_in().end_input();
        sender.fill_window();
    }
};

struct ExpectSegment : public SenderExpectation {
    std::optional<bool> ack{};
    std::optional<bool> rst{};
    std::optional<bool> syn{};
    std::optional<bool> fin{};
    std::optional<WrappingInt32> seqno{};
    std::optional<WrappingInt32> ackno{};
    std::optional<uint16_t> win{};
    std::optional<size_t> payload_size{};
    std::optional<std::string> data{};

    ExpectSegment &with_ack(bool ack_) {
        ack = ack_;
        return *this;
    }

    ExpectSegment &with_rst(bool rst_) {
        rst = rst_;
        return *this;
    }

    ExpectSegment &with_syn(bool syn_) {
        syn = syn_;
        return *this;
    }

    ExpectSegment &with_fin(bool fin_) {
        fin = fin_;
        return *this;
    }

    ExpectSegment &with_no_flags() {
        ack = false;
        rst = false;
        syn = false;
        fin = false;
        return *this;
    }

    ExpectSegment &with_seqno(WrappingInt32 seqno_) {
        seqno = seqno_;
        return *this;
    }

    ExpectSegment &with_seqno(uint32_t seqno_) { return with_seqno(WrappingInt32{seqno_}); }

    ExpectSegment &with_ackno(WrappingInt32 ackno_) {
        ackno = ackno_;
        return *this;
    }

    ExpectSegment &with_ackno(uint32_t ackno_) { return with_ackno(WrappingInt32{ackno_}); }

    ExpectSegment &with_win(uint16_t win_) {
        win = win_;
        return *this;
    }

    ExpectSegment &with_payload_size(size_t payload_size_) {
        payload_size = payload_size_;
        return *this;
    }

    ExpectSegment &with_data(std::string data_) {
        data = data_;
        return *this;
    }

    std::string segment_description() const {
        std::ostringstream o;
        o << "(";
        if (ack.has_value()) {
            o << (ack.value() ? "A=1," : "A=0,");
        }
        if (rst.has_value()) {
            o << (rst.value() ? "R=1," : "R=0,");
        }
        if (syn.has_value()) {
            o << (syn.value() ? "S=1," : "S=0,");
        }
        if (fin.has_value()) {
            o << (fin.value() ? "F=1," : "F=0,");
        }
        if (ackno.has_value()) {
            o << "ackno=" << ackno.value() << ",";
        }
        if (win.has_value()) {
            o << "win=" << win.value() << ",";
        }
        if (seqno.has_value()) {
            o << "seqno=" << seqno.value() << ",";
        }
        if (payload_size.has_value()) {
            o << "payload_size=" << payload_size.value() << ",";
        }
        if (data.has_value()) {
            o << "\"";
            for (unsigned int i = 0; i < std::min(size_t(16), data.value().size()); i++) {
                const char x = data.value().at(i);
                if (isprint(x)) {
                    o << data.value().at(i);
                } else {
                    o << "<" << std::to_string(uint8_t(x)) << ">";
                }
            }
            if (data.value().size() > 16) {
                o << "...";
            }
            o << "\",";
        }
        o << "...)";
        return o.str();
    }

    virtual std::string description() const { return "segment sent with " + segment_description(); }

    void execute(TCPSender &, std::queue<TCPSegment> &segments) const {
        if (segments.empty()) {
            throw SegmentExpectationViolation::violated_verb("existed");
        }
        TCPSegment seg = std::move(segments.front());
        segments.pop();
        if (ack.has_value() and seg.header().ack != ack.value()) {
            throw SegmentExpectationViolation::violated_field("ack", ack.value(), seg.header().ack);
        }
        if (rst.has_value() and seg.header().rst != rst.value()) {
            throw SegmentExpectationViolation::violated_field("rst", rst.value(), seg.header().rst);
        }
        if (syn.has_value() and seg.header().syn != syn.value()) {
            throw SegmentExpectationViolation::violated_field("syn", syn.value(), seg.header().syn);
        }
        if (fin.has_value() and seg.header().fin != fin.value()) {
            throw SegmentExpectationViolation::violated_field("fin", fin.value(), seg.header().fin);
        }
        if (seqno.has_value() and seg.header().seqno != seqno.value()) {
            throw SegmentExpectationViolation::violated_field("seqno", seqno.value(), seg.header().seqno);
        }
        if (ackno.has_value() and seg.header().ackno != ackno.value()) {
            throw SegmentExpectationViolation::violated_field("ackno", ackno.value(), seg.header().ackno);
        }
        if (win.has_value() and seg.header().win != win.value()) {
            throw SegmentExpectationViolation::violated_field("win", win.value(), seg.header().win);
        }
        if (payload_size.has_value() and seg.payload().size() != payload_size.value()) {
            throw SegmentExpectationViolation::violated_field(
                "payload_size", payload_size.value(), seg.payload().size());
        }
        if (seg.payload().size() > TCPConfig::MAX_PAYLOAD_SIZE) {
            throw SegmentExpectationViolation("packet has length (" + std::to_string(seg.payload().size()) +
                                              ") greater than the maximum");
        }
        if (data.has_value() and seg.payload().str() != data.value()) {
            throw SegmentExpectationViolation("payloads differ. expected \"" + data.value() + "\" but found \"" +
                                              std::string(seg.payload().str()) + "\"");
        }
    }
};

class TCPSenderTestHarness {
    std::queue<TCPSegment> outbound_segments;
    TCPSender sender;
    std::vector<std::string> steps_executed;
    std::string name;

    void collect_output() {
        while (not sender.segments_out().empty()) {
            outbound_segments.push(std::move(sender.segments_out().front()));
            sender.segments_out().pop();
        }
    }

  public:
    TCPSenderTestHarness(const std::string &name_, TCPConfig config)
        : outbound_segments()
        , sender(config.send_capacity, config.rt_timeout, config.fixed_isn)
        , steps_executed()
        , name(name_) {
        sender.fill_window();
        collect_output();
        std::ostringstream ss;
        ss << "Initialized with ("
           << "retx-timeout=" << config.rt_timeout << ") and called fill_window()";
        steps_executed.emplace_back(ss.str());
    }

    void execute(const SenderTestStep &step) {
        try {
            step.execute(sender, outbound_segments);
            collect_output();
            steps_executed.emplace_back(step);
        } catch (const SenderExpectationViolation &e) {
            std::cerr << "Test Failure on expectation:\n\t" << std::string(step);
            std::cerr << "\n\nFailure message:\n\t" << e.what();
            std::cerr << "\n\nList of steps that executed successfully:";
            for (const std::string &s : steps_executed) {
                std::cerr << "\n\t" << s;
            }
            std::cerr << std::endl << std::endl;
            throw SenderExpectationViolation("The test \"" + name + "\" failed");
        } catch (const std::exception &e) {
            std::cerr << "Test Failure on expectation:\n\t" << std::string(step);
            std::cerr << "\n\nFailure message:\n\t" << e.what();
            std::cerr << "\n\nList of steps that executed successfully:";
            for (const std::string &s : steps_executed) {
                std::cerr << "\n\t" << s;
            }
            std::cerr << std::endl << std::endl;
            throw SenderExpectationViolation("The test \"" + name +
                                             "\" caused your implementation to throw an exception!");
        }
    }
};

#endif  // SPONGE_SENDER_HARNESS_HH
