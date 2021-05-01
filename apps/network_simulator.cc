#include "arp_message.hh"
#include "router.hh"
#include "util.hh"

#include <iostream>
#include <list>
#include <unordered_map>

using namespace std;

auto rd = get_random_generator();

EthernetAddress random_host_ethernet_address() {
    EthernetAddress addr;
    for (auto &byte : addr) {
        byte = rd();  // use a random local Ethernet address
    }
    addr.at(0) |= 0x02;  // "10" in last two binary digits marks a private Ethernet address
    addr.at(0) &= 0xfe;

    return addr;
}

EthernetAddress random_router_ethernet_address() {
    EthernetAddress addr;
    for (auto &byte : addr) {
        byte = rd();  // use a random local Ethernet address
    }
    addr.at(0) = 0x02;  // "10" in last two binary digits marks a private Ethernet address
    addr.at(1) = 0;
    addr.at(2) = 0;

    return addr;
}

uint32_t ip(const string &str) { return Address{str}.ipv4_numeric(); }

template <typename T>
void clear(T &queue1, T &queue2) {
    while (not queue1.empty()) {
        queue1.pop();
        queue2.pop();
    }
}

string summary(const EthernetFrame &frame) {
    string ret;
    ret += frame.header().to_string();
    switch (frame.header().type) {
        case EthernetHeader::TYPE_IPv4: {
            InternetDatagram dgram;
            if (dgram.parse(frame.payload()) == ParseResult::NoError) {
                ret += " " + dgram.header().summary();
                ret += " payload=\"" + string(dgram.payload().concatenate()) + "\"";
            } else {
                ret += " (bad IPv4)";
            }
        } break;
        case EthernetHeader::TYPE_ARP: {
            ARPMessage arp;
            if (arp.parse(frame.payload()) == ParseResult::NoError) {
                ret += " " + arp.to_string();
            } else {
                ret += " (bad ARP)";
            }
        }
        default:
            break;
    }
    return ret;
}

class Host {
    string _name;
    Address _my_address;
    AsyncNetworkInterface _interface;
    Address _next_hop;

    std::list<InternetDatagram> _expecting_to_receive{};

    bool expecting(const InternetDatagram &expected) const {
        for (const auto &x : _expecting_to_receive) {
            if (x.serialize().concatenate() == expected.serialize().concatenate()) {
                return true;
            }
        }
        return false;
    }

    void remove_expectation(const InternetDatagram &expected) {
        for (auto it = _expecting_to_receive.begin(); it != _expecting_to_receive.end(); ++it) {
            if (it->serialize().concatenate() == expected.serialize().concatenate()) {
                _expecting_to_receive.erase(it);
                return;
            }
        }
    }

  public:
    Host(const string &name, const Address &my_address, const Address &next_hop)
        : _name(name)
        , _my_address(my_address)
        , _interface(random_host_ethernet_address(), _my_address)
        , _next_hop(next_hop) {}

    InternetDatagram send_to(const Address &destination, const uint8_t ttl = 64) {
        InternetDatagram dgram;
        dgram.header().src = _my_address.ipv4_numeric();
        dgram.header().dst = destination.ipv4_numeric();
        dgram.payload() = "random payload: {" + to_string(rd()) + "}";
        dgram.header().len = dgram.header().hlen * 4 + dgram.payload().size();
        dgram.header().ttl = ttl;

        _interface.send_datagram(dgram, _next_hop);

        cerr << "Host " << _name << " trying to send datagram (with next hop = " << _next_hop.ip()
             << "): " << dgram.header().summary() << " payload=\"" << dgram.payload().concatenate() << "\"\n";

        return dgram;
    }

    const Address &address() { return _my_address; }

    AsyncNetworkInterface &interface() { return _interface; }

    void expect(const InternetDatagram &expected) { _expecting_to_receive.push_back(expected); }

    const string &name() { return _name; }

    void check() {
        while (not _interface.datagrams_out().empty()) {
            const auto &dgram_received = _interface.datagrams_out().front();
            if (not expecting(dgram_received)) {
                throw runtime_error("Host " + _name +
                                    " received unexpected Internet datagram: " + dgram_received.header().summary() +
                                    " payload=\"" + dgram_received.payload().concatenate() + "\"");
            }
            remove_expectation(dgram_received);
            _interface.datagrams_out().pop();
        }

        if (not _expecting_to_receive.empty()) {
            auto &expected = _expecting_to_receive.front();
            throw runtime_error("Host " + _name + " did NOT receive an expected Internet datagram: " +
                                expected.header().summary() + " payload=\"" + expected.payload().concatenate() + "\"");
        }
    }
};

class Network {
  private:
    Router _router{};

    size_t default_id, eth0_id, eth1_id, eth2_id, uun3_id, hs4_id, mit5_id;

    std::unordered_map<string, Host> _hosts{};

    void exchange_frames(const string &x_name,
                         AsyncNetworkInterface &x,
                         const string &y_name,
                         AsyncNetworkInterface &y) {
        auto x_frames = x.frames_out(), y_frames = y.frames_out();

        deliver(x_name, x_frames, y_name, y);
        deliver(y_name, y_frames, x_name, x);

        clear(x_frames, x.frames_out());
        clear(y_frames, y.frames_out());
    }

    void exchange_frames(const string &x_name,
                         AsyncNetworkInterface &x,
                         const string &y_name,
                         AsyncNetworkInterface &y,
                         const string &z_name,
                         AsyncNetworkInterface &z) {
        auto x_frames = x.frames_out(), y_frames = y.frames_out(), z_frames = z.frames_out();

        deliver(x_name, x_frames, y_name, y);
        deliver(x_name, x_frames, z_name, z);

        deliver(y_name, y_frames, x_name, x);
        deliver(y_name, y_frames, z_name, z);

        deliver(z_name, z_frames, x_name, x);
        deliver(z_name, z_frames, y_name, y);

        clear(x_frames, x.frames_out());
        clear(y_frames, y.frames_out());
        clear(z_frames, z.frames_out());
    }

    void deliver(const string &src_name,
                 const queue<EthernetFrame> &src,
                 const string &dst_name,
                 AsyncNetworkInterface &dst) {
        queue<EthernetFrame> to_send = src;
        while (not to_send.empty()) {
            to_send.front().payload() = to_send.front().payload().concatenate();
            cerr << "Transferring frame from " << src_name << " to " << dst_name << ": " << summary(to_send.front())
                 << "\n";
            dst.recv_frame(move(to_send.front()));
            to_send.pop();
        }
    }

  public:
    Network()
        : default_id(_router.add_interface({random_router_ethernet_address(), {"171.67.76.46"}}))
        , eth0_id(_router.add_interface({random_router_ethernet_address(), {"10.0.0.1"}}))
        , eth1_id(_router.add_interface({random_router_ethernet_address(), {"172.16.0.1"}}))
        , eth2_id(_router.add_interface({random_router_ethernet_address(), {"192.168.0.1"}}))
        , uun3_id(_router.add_interface({random_router_ethernet_address(), {"198.178.229.1"}}))
        , hs4_id(_router.add_interface({random_router_ethernet_address(), {"143.195.0.2"}}))
        , mit5_id(_router.add_interface({random_router_ethernet_address(), {"128.30.76.255"}})) {
        _hosts.insert({"applesauce", {"applesauce", {"10.0.0.2"}, {"10.0.0.1"}}});
        _hosts.insert({"default_router", {"default_router", {"171.67.76.1"}, {"0"}}});
        ;
        _hosts.insert({"cherrypie", {"cherrypie", {"192.168.0.2"}, {"192.168.0.1"}}});
        _hosts.insert({"hs_router", {"hs_router", {"143.195.0.1"}, {"0"}}});
        _hosts.insert({"dm42", {"dm42", {"198.178.229.42"}, {"198.178.229.1"}}});
        _hosts.insert({"dm43", {"dm43", {"198.178.229.43"}, {"198.178.229.1"}}});

        _router.add_route(ip("0.0.0.0"), 0, host("default_router").address(), default_id);
        _router.add_route(ip("10.0.0.0"), 8, {}, eth0_id);
        _router.add_route(ip("172.16.0.0"), 16, {}, eth1_id);
        _router.add_route(ip("192.168.0.0"), 24, {}, eth2_id);
        _router.add_route(ip("198.178.229.0"), 24, {}, uun3_id);
        _router.add_route(ip("143.195.0.0"), 17, host("hs_router").address(), hs4_id);
        _router.add_route(ip("143.195.128.0"), 18, host("hs_router").address(), hs4_id);
        _router.add_route(ip("143.195.192.0"), 19, host("hs_router").address(), hs4_id);
        _router.add_route(ip("128.30.76.255"), 16, Address{"128.30.0.1"}, mit5_id);
    }

    void simulate_physical_connections() {
        exchange_frames(
            "router.default", _router.interface(default_id), "default_router", host("default_router").interface());
        exchange_frames("router.eth0", _router.interface(eth0_id), "applesauce", host("applesauce").interface());
        exchange_frames("router.eth2", _router.interface(eth2_id), "cherrypie", host("cherrypie").interface());
        exchange_frames("router.hs4", _router.interface(hs4_id), "hs_router", host("hs_router").interface());
        exchange_frames("router.uun3",
                        _router.interface(uun3_id),
                        "dm42",
                        host("dm42").interface(),
                        "dm43",
                        host("dm43").interface());
    }

    void simulate() {
        for (unsigned int i = 0; i < 256; i++) {
            _router.route();
            simulate_physical_connections();
        }

        for (auto &host : _hosts) {
            host.second.check();
        }
    }

    Host &host(const string &name) {
        auto it = _hosts.find(name);
        if (it == _hosts.end()) {
            throw runtime_error("unknown host: " + name);
        }
        if (it->second.name() != name) {
            throw runtime_error("invalid host: " + name);
        }
        return it->second;
    }
};

void network_simulator() {
    const string green = "\033[32;1m", normal = "\033[m";

    cerr << green << "Constructing network." << normal << "\n";

    Network network;

    cout << green << "\n\nTesting traffic between two ordinary hosts (applesauce to cherrypie)..." << normal << "\n\n";
    {
        auto dgram_sent = network.host("applesauce").send_to(network.host("cherrypie").address());
        dgram_sent.header().ttl--;
        network.host("cherrypie").expect(dgram_sent);
        network.simulate();
    }

    cout << green << "\n\nTesting traffic between two ordinary hosts (cherrypie to applesauce)..." << normal << "\n\n";
    {
        auto dgram_sent = network.host("cherrypie").send_to(network.host("applesauce").address());
        dgram_sent.header().ttl--;
        network.host("applesauce").expect(dgram_sent);
        network.simulate();
    }

    cout << green << "\n\nSuccess! Testing applesauce sending to the Internet." << normal << "\n\n";
    {
        auto dgram_sent = network.host("applesauce").send_to({"1.2.3.4"});
        dgram_sent.header().ttl--;
        network.host("default_router").expect(dgram_sent);
        network.simulate();
    }

    cout << green << "\n\nSuccess! Testing sending to the HS network and Internet." << normal << "\n\n";
    {
        auto dgram_sent = network.host("applesauce").send_to({"143.195.131.17"});
        dgram_sent.header().ttl--;
        network.host("hs_router").expect(dgram_sent);
        network.simulate();

        dgram_sent = network.host("cherrypie").send_to({"143.195.193.52"});
        dgram_sent.header().ttl--;
        network.host("hs_router").expect(dgram_sent);
        network.simulate();

        dgram_sent = network.host("cherrypie").send_to({"143.195.223.255"});
        dgram_sent.header().ttl--;
        network.host("hs_router").expect(dgram_sent);
        network.simulate();

        dgram_sent = network.host("cherrypie").send_to({"143.195.224.0"});
        dgram_sent.header().ttl--;
        network.host("default_router").expect(dgram_sent);
        network.simulate();
    }

    cout << green << "\n\nSuccess! Testing two hosts on the same network (dm42 to dm43)..." << normal << "\n\n";
    {
        auto dgram_sent = network.host("dm42").send_to(network.host("dm43").address());
        dgram_sent.header().ttl--;
        network.host("dm43").expect(dgram_sent);
        network.simulate();
    }

    cout << green << "\n\nSuccess! Testing TTL expiration..." << normal << "\n\n";
    {
        auto dgram_sent = network.host("applesauce").send_to({"1.2.3.4"}, 1);
        network.simulate();

        dgram_sent = network.host("applesauce").send_to({"1.2.3.4"}, 0);
        network.simulate();
    }

    cout << "\n\n\033[32;1mCongratulations! All datagrams were routed successfully.\033[m\n";
}

int main() {
    try {
        network_simulator();
    } catch (const exception &e) {
        cerr << "\n\n\n";
        cerr << "\033[31;1mError: " << e.what() << "\033[m\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
