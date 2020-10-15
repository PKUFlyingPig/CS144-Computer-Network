#ifndef SPONGE_LIBSPONGE_TCP_EXPECTATION_FORWARD_HH
#define SPONGE_LIBSPONGE_TCP_EXPECTATION_FORWARD_HH

#include "tcp_segment.hh"
#include "tcp_state.hh"

#include <cstdint>
#include <exception>
#include <optional>
#include <sstream>

class TCPTestHarness;

struct TCPTestStep {
  public:
    virtual ~TCPTestStep() {}
    virtual std::string to_string() const { return "TestStep"; }
    virtual void execute(TCPTestHarness &) const {}
    virtual TCPSegment expect_seg(TCPTestHarness &) const { throw std::runtime_error("unimplemented"); }
};

struct TCPExpectation;
struct ExpectNoSegment;
struct ExpectState;
struct ExpectNotInState;
struct ExpectOneSegment;
struct ExpectSegment;
struct ExpectData;
struct ExpectNoData;
struct ExpectSegmentAvailable;
struct ExpectBytesInFlight;
struct ExpectUnassembledBytes;
struct ExpectWaitTimer;
struct SendSegment;
struct Write;
struct Tick;
struct Connect;
struct Listen;
struct Close;

class TCPExpectationViolation : public std::runtime_error {
  public:
    TCPExpectationViolation(const std::string msg) : std::runtime_error(msg) {}
};

class TCPPropertyViolation : public TCPExpectationViolation {
  public:
    TCPPropertyViolation(const std::string &msg) : TCPExpectationViolation(msg) {}
    template <typename T>
    static TCPPropertyViolation make(const std::string &property_name, const T &expected_value, const T &actual_value) {
        std::stringstream ss{};
        ss << "The TCP has `" << property_name << " = " << actual_value << "`, but " << property_name
           << " was expected to be `" << expected_value << "`";
        return TCPPropertyViolation{ss.str()};
    }

    template <typename T>
    static TCPPropertyViolation make_not(const std::string &property_name, const T &expected_non_value) {
        std::stringstream ss{};
        ss << "The TCP has `" << property_name << " = " << expected_non_value << "`, but " << property_name
           << " was expected to **not** be `" << expected_non_value << "`";
        return TCPPropertyViolation{ss.str()};
    }
};

class SegmentExpectationViolation : public TCPExpectationViolation {
  public:
    SegmentExpectationViolation(const std::string &msg) : TCPExpectationViolation(msg) {}
    static SegmentExpectationViolation violated_verb(const std::string &msg) {
        return SegmentExpectationViolation{"The TCP should have produced a segment that " + msg + ", but it did not"};
    }
    template <typename T>
    static SegmentExpectationViolation violated_field(const std::string &field_name,
                                                      const T &expected_value,
                                                      const T &actual_value) {
        std::stringstream ss{};
        ss << "The TCP produced a segment with `" << field_name << " = " << actual_value << "`, but " << field_name
           << " was expected to be `" << expected_value << "`";
        return SegmentExpectationViolation{ss.str()};
    }
};

class StateExpectationViolation : public TCPExpectationViolation {
  public:
    StateExpectationViolation(const std::string &msg) : TCPExpectationViolation(msg) {}
    StateExpectationViolation(const TCPState &expected_state, const TCPState &actual_state)
        : TCPExpectationViolation("The TCP was in state `" + actual_state.name() +
                                  "`, but it was expected to be in state `" + expected_state.name() + "`") {}
};

#endif  // SPONGE_LIBSPONGE_TCP_EXPECTATION_FORWARD_HH
