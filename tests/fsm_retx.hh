#ifndef SPONGE_TESTS_FSM_RETX_HH
#define SPONGE_TESTS_FSM_RETX_HH

#include "tcp_expectation.hh"
#include "tcp_fsm_test_harness.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void check_segment(TCPTestHarness &test, const std::string &data, const bool multiple, const int lineno) {
    try {
        std::cerr << "  check_segment" << std::endl;
        test.execute(ExpectSegment{}.with_ack(true).with_payload_size(data.size()).with_data(data));
        if (!multiple) {
            test.execute(ExpectNoSegment{}, "test failed: multiple re-tx?");
        }
    } catch (const std::exception &e) {
        throw std::runtime_error(std::string(e.what()) + " (in check_segment called from line " +
                                 std::to_string(lineno) + ")");
    }
}

#endif  // SPONGE_TESTS_FSM_RETX_HH
