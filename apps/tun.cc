#include "tun.hh"

#include "ipv4_datagram.hh"
#include "parser.hh"
#include "tcp_segment.hh"
#include "util.hh"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <vector>

using namespace std;

int main() {
    try {
        TunFD tun("tun144");
        while (true) {
            auto buffer = tun.read();
            cout << "\n\n***\n*** Got packet:\n***\n";
            hexdump(buffer.data(), buffer.size());

            IPv4Datagram ip_dgram;

            cout << "attempting to parse as ipv4 datagram... ";
            if (ip_dgram.parse(move(buffer)) != ParseResult::NoError) {
                cout << "failed.\n";
                continue;
            }

            cout << "success! totlen=" << ip_dgram.header().len << ", IPv4 header contents:\n";
            cout << ip_dgram.header().to_string();

            if (ip_dgram.header().proto != IPv4Header::PROTO_TCP) {
                cout << "\nNot TCP, skipping.\n";
                continue;
            }

            cout << "\nAttempting to parse as a TCP segment... ";

            TCPSegment tcp_seg;

            if (tcp_seg.parse(ip_dgram.payload(), ip_dgram.header().pseudo_cksum()) != ParseResult::NoError) {
                cout << "failed.\n";
                continue;
            }

            cout << "success! payload len=" << tcp_seg.payload().size() << ", TCP header contents:\n";
            cout << tcp_seg.header().to_string() << endl;
        }
    } catch (const exception &e) {
        cout << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
