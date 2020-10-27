#include "network_interface_test_harness.hh"

#include "arp_message.hh"

#include <iostream>
#include <memory>
#include <sstream>

using namespace std;

// NetworkInterfaceTestStep

NetworkInterfaceTestStep::operator std::string() const { return "NetworkInterfaceTestStep"; }

void NetworkInterfaceTestStep::execute(NetworkInterface &) const {}

NetworkInterfaceTestStep::~NetworkInterfaceTestStep() {}

// NetworkInterfaceExpectationViolation

NetworkInterfaceExpectationViolation::NetworkInterfaceExpectationViolation(const std::string &msg)
    : std::runtime_error(msg) {}

template <typename T>
NetworkInterfaceExpectationViolation NetworkInterfaceExpectationViolation::property(const std::string &property_name,
                                                                                    const T &expected,
                                                                                    const T &actual) {
    return NetworkInterfaceExpectationViolation("The NetworkInterface should have had " + property_name + " equal to " +
                                                to_string(expected) + " but instead it was " + to_string(actual));
}

// NetworkInterfaceExpectation

NetworkInterfaceExpectation::operator std::string() const { return "Expectation: " + description(); }

std::string NetworkInterfaceExpectation::description() const { return "description missing"; }

void NetworkInterfaceExpectation::execute(NetworkInterface &) const {}

NetworkInterfaceExpectation::~NetworkInterfaceExpectation() {}

// NetworkInterfaceAction

NetworkInterfaceAction::operator std::string() const { return "     Action: " + description(); }

std::string NetworkInterfaceAction::description() const { return "description missing"; }

void NetworkInterfaceAction::execute(NetworkInterface &) const {}

NetworkInterfaceAction::~NetworkInterfaceAction() {}

NetworkInterfaceTestHarness::NetworkInterfaceTestHarness(const std::string &test_name,
                                                         const EthernetAddress &ethernet_address,
                                                         const Address &ip_address)
    : _test_name(test_name), _interface(ethernet_address, ip_address) {
    std::ostringstream ss;
    ss << "Initialized with ("
       << "ethernet_address=" << to_string(ethernet_address) << ", "
       << "ip_address=" << ip_address.ip() << ")";
    _steps_executed.emplace_back(ss.str());
}

string summary(const EthernetFrame &frame) {
    Buffer payload_single = frame.payload().concatenate();
    string out = frame.header().to_string();
    out.append(" + payload (" + to_string(frame.payload().size()) + " bytes) ");
    switch (frame.header().type) {
        case EthernetHeader::TYPE_IPv4: {
            InternetDatagram dgram;
            if (dgram.parse(payload_single) == ParseResult::NoError) {
                out.append("IPv4: " + dgram.header().summary());
            } else {
                out.append("bad IPv4 datagram");
            }
        } break;
        case EthernetHeader::TYPE_ARP: {
            ARPMessage arp;
            if (arp.parse(payload_single) == ParseResult::NoError) {
                out.append("ARP: " + arp.to_string());
            } else {
                out.append("bad ARP message");
            }
        } break;
        default:
            out.append("unknown frame type");
            break;
    }
    return out;
}

void NetworkInterfaceTestHarness::execute(const NetworkInterfaceTestStep &step) {
    try {
        step.execute(_interface);
        _steps_executed.emplace_back(step);
    } catch (const NetworkInterfaceExpectationViolation &e) {
        std::cerr << "Test Failure on expectation:\n\t" << std::string(step);
        std::cerr << "\n\nFailure message:\n\t" << e.what();
        std::cerr << "\n\nList of steps that executed successfully:";
        for (const std::string &s : _steps_executed) {
            std::cerr << "\n\t" << s;
        }
        std::cerr << std::endl << std::endl;
        throw NetworkInterfaceExpectationViolation("The test \"" + _test_name + "\" failed");
    } catch (const exception &e) {
        std::cerr << "Test Failure on expectation:\n\t" << std::string(step);
        std::cerr << "\n\nException:\n\t" << e.what();
        std::cerr << "\n\nList of steps that executed successfully:";
        for (const std::string &s : _steps_executed) {
            std::cerr << "\n\t" << s;
        }
        std::cerr << std::endl << std::endl;
        throw NetworkInterfaceExpectationViolation("The test \"" + _test_name +
                                                   "\" caused your implementation to throw an exception!");
    }
}

string SendDatagram::description() const {
    return "request to send datagram (to next hop " + next_hop.ip() + "): " + dgram.header().summary();
}

void SendDatagram::execute(NetworkInterface &interface) const { interface.send_datagram(dgram, next_hop); }

string ReceiveFrame::description() const { return "frame arrives (" + summary(frame) + ")"; }

void ReceiveFrame::execute(NetworkInterface &interface) const {
    const optional<InternetDatagram> result = interface.recv_frame(frame);

    if ((not result.has_value()) and (not expected.has_value())) {
        return;
    } else if (result.has_value() and not expected.has_value()) {
        throw NetworkInterfaceExpectationViolation(
            "an arriving Ethernet frame was passed up the stack as an Internet datagram, but was not expected to be "
            "(did destination address match our interface?)");
    } else if (expected.has_value() and not result.has_value()) {
        throw NetworkInterfaceExpectationViolation(
            "an arriving Ethernet frame was expected to be passed up the stack as an Internet datagram, but wasn't");
    } else if (result.value().serialize().concatenate() != expected.value().serialize().concatenate()) {
        throw NetworkInterfaceExpectationViolation(
            string("NetworkInterface::recv_frame() produced a different Internet datagram than was expected: ") +
            "actual={" + result.value().header().summary() + "}");
    }
}

string ExpectFrame::description() const { return "frame transmitted (" + summary(expected) + ")"; }

void ExpectFrame::execute(NetworkInterface &interface) const {
    if (interface.frames_out().empty()) {
        throw NetworkInterfaceExpectationViolation(
            "NetworkInterface was expected to send an Ethernet frame, but did not");
    }

    if (interface.frames_out().front().serialize().concatenate() != expected.serialize().concatenate()) {
        throw NetworkInterfaceExpectationViolation(
            "NetworkInterface sent a different Ethernet frame than was expected: actual={" +
            summary(interface.frames_out().front()) + "}");
    }

    interface.frames_out().pop();
}

string ExpectNoFrame::description() const { return "no frame transmitted"; }

void ExpectNoFrame::execute(NetworkInterface &interface) const {
    if (not interface.frames_out().empty()) {
        throw NetworkInterfaceExpectationViolation(
            "NetworkInterface sent an Ethernet frame although none was expected");
    }
}

string Tick::description() const { return to_string(_ms) + " ms pass"; }

void Tick::execute(NetworkInterface &interface) const { interface.tick(_ms); }
