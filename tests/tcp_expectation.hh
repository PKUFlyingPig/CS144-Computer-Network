#ifndef SPONGE_LIBSPONGE_TCP_EXPECTATION_HH
#define SPONGE_LIBSPONGE_TCP_EXPECTATION_HH

#include "tcp_config.hh"
#include "tcp_expectation_forward.hh"
#include "tcp_fsm_test_harness.hh"
#include "tcp_state.hh"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <optional>
#include <sstream>

struct TCPExpectation : public TCPTestStep {
    virtual ~TCPExpectation() {}
    std::string to_string() const { return "Expectation: " + description(); }
    virtual std::string description() const { return "description missing"; }
    virtual void execute(TCPTestHarness &) const {}
};

struct TCPAction : public TCPTestStep {
    virtual ~TCPAction() {}
    std::string to_string() const { return "Action:      " + description(); }
    virtual std::string description() const { return "description missing"; }
    virtual void execute(TCPTestHarness &) const {}
};

struct ExpectNoData : public TCPExpectation {
    std::string description() const { return "no (more) data available"; }
    void execute(TCPTestHarness &harness) const {
        if (harness.can_read()) {
            throw SegmentExpectationViolation("The TCP produced data when it should not have");
        }
    }
};

static void append_data(std::ostream &os, std::string_view data) {
    os << "\"";
    for (unsigned int i = 0; i < std::min(size_t(16), data.size()); i++) {
        const char x = data.at(i);
        if (isprint(x)) {
            os << data.at(i);
        } else {
            os << "<" << std::to_string(uint8_t(x)) << ">";
        }
    }
    if (data.size() > 16) {
        os << "...";
    }
    os << "\"";
}

struct ExpectData : public TCPExpectation {
    std::optional<std::string> data{};

    ExpectData() {}

    ExpectData &with_data(const std::string &data_) {
        data = data_;
        return *this;
    }

    std::string description() const {
        std::ostringstream msg;
        msg << "data available";
        if (data.has_value()) {
            msg << " (" << data.value().size() << " bytes starting with ";
            append_data(msg, data.value());
            msg << ")";
        }
        return msg.str();
    }

    void execute(TCPTestHarness &harness) const {
        if (harness._fsm.inbound_stream().buffer_empty()) {
            throw SegmentExpectationViolation("The TCP should have data for the user, but does not");
        }

        const size_t bytes_avail = harness._fsm.inbound_stream().buffer_size();
        std::string actual_data = harness._fsm.inbound_stream().read(bytes_avail);
        if (data.has_value()) {
            if (actual_data.size() != data->size()) {
                std::ostringstream msg{"The TCP produced "};
                msg << actual_data.size() << " bytes, but should have produced " << data->size() << " bytes";
                throw TCPExpectationViolation(msg.str());
            }
            if (not std::equal(actual_data.begin(), actual_data.end(), data->begin())) {
                std::ostringstream msg{"The TCP produced data "};
                msg << actual_data << ", but should have produced " << data.value() << " bytes";
                throw TCPExpectationViolation(msg.str());
            }
        }
    }
};

struct ExpectSegmentAvailable : public TCPExpectation {
    ExpectSegmentAvailable() {}
    std::string description() const { return "segment sent"; }
    void execute(TCPTestHarness &harness) const {
        if (not harness.can_read()) {
            throw SegmentExpectationViolation("The TCP should have produces a segment, but did not");
        }
    }
};

struct ExpectNoSegment : public TCPExpectation {
    std::string description() const { return "no (more) segments sent"; }
    void execute(TCPTestHarness &harness) const {
        if (harness.can_read()) {
            throw SegmentExpectationViolation("The TCP produced a segment when it should not have");
        }
    }
};

struct ExpectSegment : public TCPExpectation {
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
            o << "data=";
            append_data(o, data.value());
            o << ",";
        }
        o << ")";
        return o.str();
    }

    virtual std::string description() const { return "segment sent with " + segment_description(); }

    virtual TCPSegment expect_seg(TCPTestHarness &harness) const {
        if (not harness.can_read()) {
            throw SegmentExpectationViolation::violated_verb("existed");
        }
        TCPSegment seg;
        if (ParseResult::NoError != seg.parse(harness._flt.read())) {
            throw SegmentExpectationViolation::violated_verb("was parsable");
        }
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
        if (seg.length_in_sequence_space() > TCPConfig::MAX_PAYLOAD_SIZE) {
            throw SegmentExpectationViolation("packet has length_including_flags (" +
                                              std::to_string(seg.length_in_sequence_space()) +
                                              ") greater than the maximum");
        }
        if (data.has_value() and seg.payload().str() != *data) {
            throw SegmentExpectationViolation("payloads differ");
        }
        return seg;
    }

    virtual void execute(TCPTestHarness &harness) const { expect_seg(harness); }
};

struct ExpectOneSegment : public ExpectSegment {
    std::string description() const { return "exactly one segment sent with " + segment_description(); }

    TCPSegment expect_seg(TCPTestHarness &harness) const {
        TCPSegment seg = ExpectSegment::expect_seg(harness);
        if (harness.can_read()) {
            throw SegmentExpectationViolation("The TCP an extra segment when it should not have");
        }
        return seg;
    }

    void execute(TCPTestHarness &harness) const {
        ExpectSegment::execute(harness);
        if (harness.can_read()) {
            throw SegmentExpectationViolation("The TCP an extra segment when it should not have");
        }
    }
};

struct ExpectState : public TCPExpectation {
    TCPState state;

    ExpectState(TCPState stat) : state(stat) {}

    std::string description() const {
        std::ostringstream o;
        o << "TCP in state ";
        o << state.name();
        return o.str();
    }

    void execute(TCPTestHarness &harness) const {
        TCPState actual_state = harness._fsm.state();
        if (actual_state != state) {
            throw StateExpectationViolation{state, actual_state};
        }
    }
};

struct ExpectNotInState : public TCPExpectation {
    TCPState state;

    ExpectNotInState(TCPState stat) : state(stat) {}

    std::string description() const {
        std::ostringstream o;
        o << "TCP **not** in state ";
        o << state.name();
        return o.str();
    }

    void execute(TCPTestHarness &harness) const {
        TCPState actual_state = harness._fsm.state();
        if (actual_state == state) {
            throw TCPPropertyViolation::make_not("state", state.name());
        }
    }
};

struct ExpectBytesInFlight : public TCPExpectation {
    uint64_t bytes;

    ExpectBytesInFlight(uint64_t bytes_) : bytes(bytes_) {}

    std::string description() const {
        std::ostringstream o;
        o << "TCP has " << bytes << " bytes in flight";
        return o.str();
    }

    void execute(TCPTestHarness &harness) const {
        uint64_t actual_bytes = harness._fsm.bytes_in_flight();
        if (actual_bytes != bytes) {
            throw TCPPropertyViolation::make("bytes_in_flight", bytes, actual_bytes);
        }
    }
};

struct ExpectUnassembledBytes : public TCPExpectation {
    uint64_t bytes;

    ExpectUnassembledBytes(uint64_t bytes_) : bytes(bytes_) {}

    std::string description() const {
        std::ostringstream o;
        o << "TCP has " << bytes << " unassembled bytes";
        return o.str();
    }

    void execute(TCPTestHarness &harness) const {
        uint64_t actual_unassembled_bytes = harness._fsm.unassembled_bytes();
        if (actual_unassembled_bytes != bytes) {
            throw TCPPropertyViolation::make("unassembled_bytes", bytes, actual_unassembled_bytes);
        }
    }
};

struct ExpectLingerTimer : public TCPExpectation {
    uint64_t ms;

    ExpectLingerTimer(uint64_t ms_) : ms(ms_) {}

    std::string description() const {
        std::ostringstream o;
        o << "Most recent incoming segment was " << ms << " ms ago";
        return o.str();
    }

    void execute(TCPTestHarness &harness) const {
        uint64_t actual_ms = harness._fsm.time_since_last_segment_received();
        if (actual_ms != ms) {
            throw TCPPropertyViolation::make("time_since_last_segment_received", ms, actual_ms);
        }
    }
};

struct SendSegment : public TCPAction {
    bool ack{false};
    bool rst{false};
    bool syn{false};
    bool fin{false};
    WrappingInt32 seqno{0};
    WrappingInt32 ackno{0};
    uint16_t win{0};
    size_t payload_size{0};
    std::string data{};

    SendSegment() {}

    SendSegment(TCPSegment seg) {
        ack = seg.header().ack;
        rst = seg.header().rst;
        syn = seg.header().syn;
        fin = seg.header().fin;
        seqno = seg.header().seqno;
        ackno = seg.header().ackno;
        win = seg.header().win;
        data = seg.payload();
    }

    SendSegment &with_ack(bool ack_) {
        ack = ack_;
        return *this;
    }

    SendSegment &with_rst(bool rst_) {
        rst = rst_;
        return *this;
    }

    SendSegment &with_syn(bool syn_) {
        syn = syn_;
        return *this;
    }

    SendSegment &with_fin(bool fin_) {
        fin = fin_;
        return *this;
    }

    SendSegment &with_seqno(WrappingInt32 seqno_) {
        seqno = seqno_;
        return *this;
    }

    SendSegment &with_seqno(uint32_t seqno_) { return with_seqno(WrappingInt32{seqno_}); }

    SendSegment &with_ackno(WrappingInt32 ackno_) {
        ackno = ackno_;
        return *this;
    }

    SendSegment &with_ackno(uint32_t ackno_) { return with_ackno(WrappingInt32{ackno_}); }

    SendSegment &with_win(uint16_t win_) {
        win = win_;
        return *this;
    }

    SendSegment &with_payload_size(size_t payload_size_) {
        payload_size = payload_size_;
        return *this;
    }

    SendSegment &with_data(std::string &&data_) {
        data = data_;
        return *this;
    }

    TCPSegment get_segment() const {
        TCPSegment data_seg;
        data_seg.payload() = std::string(data);
        auto &data_hdr = data_seg.header();
        data_hdr.ack = ack;
        data_hdr.rst = rst;
        data_hdr.syn = syn;
        data_hdr.fin = fin;
        data_hdr.ackno = ackno;
        data_hdr.seqno = seqno;
        data_hdr.win = win;
        return data_seg;
    }

    virtual std::string description() const {
        TCPSegment seg = get_segment();
        std::ostringstream o;
        o << "packet arrives: ";
        o << seg.header().summary();
        if (seg.payload().size() > 0) {
            o << " with " << seg.payload().size() << " data bytes ";
            append_data(o, seg.payload());
        } else {
            o << " with no payload";
        }
        return o.str();
    }

    virtual void execute(TCPTestHarness &harness) const {
        TCPSegment seg = get_segment();
        harness._fsm.segment_received(std::move(seg));
    }
};

struct Write : public TCPAction {
    std::string data;
    std::optional<size_t> _bytes_written{};

    Write(const std::string &data_) : data(data_) {}

    TCPAction &with_bytes_written(size_t bytes_written) {
        _bytes_written = bytes_written;
        return *this;
    }

    std::string description() const {
        std::ostringstream o;
        o << "write (" << data.size() << " bytes";
        if (_bytes_written.has_value()) {
            o << " with " << _bytes_written.value() << " accepted";
        }
        o << ") [";
        bool first = true;
        for (auto u : data.substr(0, 16)) {
            if (first) {
                first = false;
            } else {
                o << ", ";
            }
            o << std::hex << uint16_t(uint8_t(u));
        }

        if (data.size() > 16) {
            o << ", ...";
        }

        o << "]";
        return o.str();
    }

    void execute(TCPTestHarness &harness) const {
        size_t bytes_written = harness._fsm.write(data);
        if (_bytes_written.has_value() and bytes_written != _bytes_written.value()) {
            throw TCPExpectationViolation(std::to_string(_bytes_written.value()) +
                                          " bytes should have been written but " + std::to_string(bytes_written) +
                                          " were");
        }
    }
};

struct Tick : public TCPAction {
    size_t ms_since_last_tick;

    Tick(size_t ms_) : ms_since_last_tick(ms_) {}

    std::string description() const {
        std::ostringstream o;
        o << ms_since_last_tick << "ms pass";
        return o.str();
    }

    void execute(TCPTestHarness &harness) const { harness._fsm.tick(ms_since_last_tick); }
};

struct Connect : public TCPAction {
    std::string description() const { return "connect"; }
    void execute(TCPTestHarness &harness) const { harness._fsm.connect(); }
};

struct Listen : public TCPAction {
    std::string description() const { return "listen"; }
    void execute(TCPTestHarness &) const {}
};

struct Close : public TCPAction {
    std::string description() const { return "close"; }
    void execute(TCPTestHarness &harness) const { harness._fsm.end_input_stream(); }
};

#endif  // SPONGE_LIBSPONGE_TCP_EXPECTATION_HH
