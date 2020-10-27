#ifndef SPONGE_NETWORK_INTERFACE_HARNESS_HH
#define SPONGE_NETWORK_INTERFACE_HARNESS_HH

#include "network_interface.hh"

#include <exception>

struct NetworkInterfaceTestStep {
    virtual operator std::string() const;
    virtual void execute(NetworkInterface &) const;
    virtual ~NetworkInterfaceTestStep();
};

class NetworkInterfaceExpectationViolation : public std::runtime_error {
  public:
    NetworkInterfaceExpectationViolation(const std::string &msg);

    template <typename T>
    static NetworkInterfaceExpectationViolation property(const std::string &property_name,
                                                         const T &expected,
                                                         const T &actual);
};

struct NetworkInterfaceExpectation : public NetworkInterfaceTestStep {
    operator std::string() const override;
    virtual std::string description() const;
    virtual void execute(NetworkInterface &) const override;
    virtual ~NetworkInterfaceExpectation() override;
};

struct NetworkInterfaceAction : public NetworkInterfaceTestStep {
    operator std::string() const override;
    virtual std::string description() const;
    virtual void execute(NetworkInterface &) const override;
    virtual ~NetworkInterfaceAction() override;
};

struct SendDatagram : public NetworkInterfaceAction {
    InternetDatagram dgram;
    Address next_hop;

    std::string description() const override;
    void execute(NetworkInterface &interface) const override;

    SendDatagram(InternetDatagram d, Address n) : dgram(d), next_hop(n) {}
};

struct ReceiveFrame : public NetworkInterfaceAction {
    EthernetFrame frame;
    std::optional<InternetDatagram> expected;

    std::string description() const override;
    void execute(NetworkInterface &interface) const override;

    ReceiveFrame(EthernetFrame f, std::optional<InternetDatagram> e) : frame(f), expected(e) {}
};

struct ExpectFrame : public NetworkInterfaceExpectation {
    EthernetFrame expected;

    std::string description() const override;
    void execute(NetworkInterface &interface) const override;

    ExpectFrame(EthernetFrame e) : expected(e) {}
};

struct ExpectNoFrame : public NetworkInterfaceExpectation {
    std::string description() const override;
    void execute(NetworkInterface &interface) const override;
};

struct Tick : public NetworkInterfaceAction {
    size_t _ms;

    std::string description() const override;
    void execute(NetworkInterface &interface) const override;

    Tick(const size_t ms) : _ms(ms) {}
};

class NetworkInterfaceTestHarness {
    std::string _test_name;
    NetworkInterface _interface;
    std::vector<std::string> _steps_executed{};

  public:
    NetworkInterfaceTestHarness(const std::string &test_name,
                                const EthernetAddress &ethernet_address,
                                const Address &ip_address);

    void execute(const NetworkInterfaceTestStep &step);
};

#endif  // SPONGE_NETWORK_INTERFACE_HARNESS_HH
