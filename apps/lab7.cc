#include "address.hh"
#include "arp_message.hh"
#include "bidirectional_stream_copy.hh"
#include "router.hh"
#include "tcp_over_ip.hh"
#include "tcp_sponge_socket.cc"
#include "util.hh"

#include <cstdlib>
#include <iostream>
#include <thread>

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

string summary(const EthernetFrame &frame) {
    string ret;
    ret += frame.header().to_string();
    switch (frame.header().type) {
        case EthernetHeader::TYPE_IPv4: {
            InternetDatagram dgram;
            if (dgram.parse(frame.payload().concatenate()) == ParseResult::NoError) {
                ret += " " + dgram.header().summary();
                if (dgram.header().proto == IPv4Header::PROTO_TCP) {
                    TCPSegment tcp_seg;
                    if (tcp_seg.parse(dgram.payload().concatenate(), dgram.header().pseudo_cksum()) ==
                        ParseResult::NoError) {
                        ret += " " + tcp_seg.header().summary();
                    }
                }
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

class NetworkInterfaceAdapter : public TCPOverIPv4Adapter {
  private:
    NetworkInterface _interface;
    Address _next_hop;
    pair<FileDescriptor, FileDescriptor> _data_socket_pair = socket_pair_helper(SOCK_DGRAM);

    void send_pending() {
        while (not _interface.frames_out().empty()) {
            _data_socket_pair.first.write(_interface.frames_out().front().serialize());
            _interface.frames_out().pop();
        }
    }

  public:
    NetworkInterfaceAdapter(const Address &ip_address, const Address &next_hop)
        : _interface(random_host_ethernet_address(), ip_address), _next_hop(next_hop) {}

    optional<TCPSegment> read() {
        EthernetFrame frame;
        if (frame.parse(_data_socket_pair.first.read()) != ParseResult::NoError) {
            return {};
        }

        // Give the frame to the NetworkInterface. Get back an Internet datagram if frame was carrying one.
        optional<InternetDatagram> ip_dgram = _interface.recv_frame(frame);

        // The incoming frame may have caused the NetworkInterface to send a frame
        send_pending();

        // Try to interpret IPv4 datagram as TCP
        if (ip_dgram) {
            return unwrap_tcp_in_ip(ip_dgram.value());
        }

        return {};
    }
    void write(TCPSegment &seg) {
        _interface.send_datagram(wrap_tcp_in_ip(seg), _next_hop);
        send_pending();
    }
    void tick(const size_t ms_since_last_tick) {
        _interface.tick(ms_since_last_tick);
        send_pending();
    }
    NetworkInterface &interface() { return _interface; }
    queue<EthernetFrame> frames_out() { return _interface.frames_out(); }

    operator FileDescriptor &() { return _data_socket_pair.first; }
    FileDescriptor &frame_fd() { return _data_socket_pair.second; }
};

class TCPSocketLab7 : public TCPSpongeSocket<NetworkInterfaceAdapter> {
    Address _local_address;

  public:
    TCPSocketLab7(const Address &ip_address, const Address &next_hop)
        : TCPSpongeSocket<NetworkInterfaceAdapter>(NetworkInterfaceAdapter(ip_address, next_hop))
        , _local_address(ip_address) {}

    void connect(const Address &address) {
        FdAdapterConfig multiplexer_config;

        _local_address = Address{_local_address.ip(), uint16_t(random_device()())};
        cerr << "DEBUG: Connecting from " << _local_address.to_string() << "...\n";
        multiplexer_config.source = _local_address;
        multiplexer_config.destination = address;

        TCPSpongeSocket<NetworkInterfaceAdapter>::connect({}, multiplexer_config);
    }

    void bind(const Address &address) {
        if (address.ip() != _local_address.ip()) {
            throw runtime_error("Cannot bind to " + address.to_string());
        }
        _local_address = Address{_local_address.ip(), address.port()};
    }

    void listen_and_accept() {
        FdAdapterConfig multiplexer_config;
        multiplexer_config.source = _local_address;
        TCPSpongeSocket<NetworkInterfaceAdapter>::listen_and_accept({}, multiplexer_config);
    }

    NetworkInterfaceAdapter &adapter() { return _datagram_adapter; }
};

void program_body(bool is_client, const string &bounce_host, const string &bounce_port, const bool debug) {
    UDPSocket internet_socket;
    Address bounce_address{bounce_host, bounce_port};

    /* let bouncer know where we are */
    internet_socket.sendto(bounce_address, "");
    internet_socket.sendto(bounce_address, "");
    internet_socket.sendto(bounce_address, "");

    /* set up the router */
    Router router;

    unsigned int host_side, internet_side;

    if (is_client) {
        host_side = router.add_interface({random_router_ethernet_address(), {"192.168.0.1"}});
        internet_side = router.add_interface({random_router_ethernet_address(), {"10.0.0.192"}});
        router.add_route(Address{"192.168.0.0"}.ipv4_numeric(), 16, {}, host_side);
        router.add_route(Address{"10.0.0.0"}.ipv4_numeric(), 8, {}, internet_side);
        router.add_route(Address{"172.16.0.0"}.ipv4_numeric(), 12, Address{"10.0.0.172"}, internet_side);
    } else {
        host_side = router.add_interface({random_router_ethernet_address(), {"172.16.0.1"}});
        internet_side = router.add_interface({random_router_ethernet_address(), {"10.0.0.172"}});
        router.add_route(Address{"172.16.0.0"}.ipv4_numeric(), 12, {}, host_side);
        router.add_route(Address{"10.0.0.0"}.ipv4_numeric(), 8, {}, internet_side);
        router.add_route(Address{"192.168.0.0"}.ipv4_numeric(), 16, Address{"10.0.0.192"}, internet_side);
    }

    /* set up the client */
    TCPSocketLab7 sock =
        is_client ? TCPSocketLab7{{"192.168.0.50"}, {"192.168.0.1"}} : TCPSocketLab7{{"172.16.0.100"}, {"172.16.0.1"}};

    atomic<bool> exit_flag {};

    /* set up the network */
    thread network_thread([&]() {
        try {
            EventLoop event_loop;
            // Frames from host to router
            event_loop.add_rule(sock.adapter().frame_fd(), Direction::In, [&] {
                EthernetFrame frame;
                if (frame.parse(sock.adapter().frame_fd().read()) != ParseResult::NoError) {
                    return;
                }
                if (debug) {
                    cerr << "     Host->router:     " << summary(frame) << "\n";
                }
                router.interface(host_side).recv_frame(frame);
                router.route();
            });

            // Frames from router to host
            event_loop.add_rule(sock.adapter().frame_fd(),
                                Direction::Out,
                                [&] {
                                    auto &f = router.interface(host_side).frames_out();
                                    if (debug) {
                                        cerr << "     Router->host:     " << summary(f.front()) << "\n";
                                    }
                                    sock.adapter().frame_fd().write(f.front().serialize());
                                    f.pop();
                                },
                                [&] { return not router.interface(host_side).frames_out().empty(); });

            // Frames from router to Internet
            event_loop.add_rule(internet_socket,
                                Direction::Out,
                                [&] {
                                    auto &f = router.interface(internet_side).frames_out();
                                    if (debug) {
                                        cerr << "     Router->Internet: " << summary(f.front()) << "\n";
                                    }
                                    internet_socket.sendto(bounce_address, f.front().serialize());
                                    f.pop();
                                },
                                [&] { return not router.interface(internet_side).frames_out().empty(); });

            // Frames from Internet to router
            event_loop.add_rule(internet_socket, Direction::In, [&] {
                EthernetFrame frame;
                if (frame.parse(internet_socket.read()) != ParseResult::NoError) {
                    return;
                }
                if (debug) {
                    cerr << "     Internet->router: " << summary(frame) << "\n";
                }
                router.interface(internet_side).recv_frame(frame);
                router.route();
            });

            while (true) {
                if (EventLoop::Result::Exit == event_loop.wait_next_event(50)) {
                    cerr << "Exiting...\n";
                    return;
                }
                router.interface(host_side).tick(50);
                router.interface(internet_side).tick(50);
                if (exit_flag) {
                    return;
                }
            }
        } catch (const exception &e) {
            cerr << "Thread ending from exception: " << e.what() << "\n";
        }
    });

    try {
        if (is_client) {
            sock.connect({"172.16.0.100", 1234});
        } else {
            sock.bind({"172.16.0.100", 1234});
            sock.listen_and_accept();
        }

        bidirectional_stream_copy(sock);
        sock.wait_until_closed();
    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << "\n";
    }

    cerr << "Exiting... ";
    exit_flag = true;
    network_thread.join();
    cerr << "done.\n";
}

void print_usage(const string &argv0) {
    cerr << "Usage: " << argv0 << " client HOST PORT [debug]\n";
    cerr << "or     " << argv0 << " server HOST PORT [debug]\n";
}

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        if (argc != 4 and argc != 5) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }

        if (argv[1] != "client"s and argv[1] != "server"s) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }

        program_body(argv[1] == "client"s, argv[2], argv[3], argc == 5);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
