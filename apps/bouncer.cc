#include "eventloop.hh"
#include "socket.hh"

#include <cstdlib>
#include <iostream>

using namespace std;

void program_body() {
    EventLoop loop;
    vector<UDPSocket> sockets;
    sockets.reserve(66000);

    for (uint16_t lower_port = 1024; lower_port <= 64000; lower_port += 2) {
        sockets.emplace_back();
        sockets.emplace_back();
        UDPSocket &x = sockets.at(sockets.size() - 2);
        UDPSocket &y = sockets.at(sockets.size() - 1);

        x.bind(Address{"0", lower_port});
        y.bind(Address{"0", uint16_t(lower_port + 1)});

        optional<Address> x_peer, y_peer;

        loop.add_rule(x, Direction::In, [&] {
            auto rec = x.recv();
            if (not x_peer.has_value() or x_peer.value() != rec.source_address) {
                x_peer = rec.source_address;
                cerr << "Learned new address for X at " << x_peer.value().to_string() << "\n";
            }
            if (y_peer.has_value() and not rec.payload.empty()) {
                y.sendto(y_peer.value(), rec.payload);
            }
        });

        loop.add_rule(y, Direction::In, [&] {
            auto rec = y.recv();
            if (not y_peer.has_value() or y_peer.value() != rec.source_address) {
                y_peer = rec.source_address;
                cerr << "Learned new address for Y at " << y_peer.value().to_string() << "\n";
            }
            if (x_peer.has_value() and not rec.payload.empty()) {
                x.sendto(x_peer.value(), rec.payload);
            }
        });
    }

    cerr << "Starting event loop...\n";

    while (EventLoop::Result::Exit != loop.wait_next_event(-1)) {
    }
}

int main() {
    try {
        program_body();
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
