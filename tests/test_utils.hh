#ifndef SPONGE_TESTS_TEST_UTILS_HH
#define SPONGE_TESTS_TEST_UTILS_HH

#include "tcp_header.hh"
#include "util.hh"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <pcap/pcap.h>

inline void show_ethernet_frame(const uint8_t *pkt, const struct pcap_pkthdr &hdr) {
    const auto flags(std::cout.flags());

    std::cout << "source MAC: ";
    std::cout << std::hex << std::setfill('0');
    for (unsigned i = 0; i < 6; ++i) {
        std::cout << std::setw(2) << +pkt[i] << (i == 5 ? ' ' : ':');
    }

    std::cout << "    dest MAC: ";
    for (unsigned i = 0; i < 6; ++i) {
        std::cout << std::setw(2) << +pkt[i + 6] << (i == 5 ? ' ' : ':');
    }

    std::cout << "    ethertype: " << std::setw(2) << +pkt[12] << std::setw(2) << +pkt[13] << '\n';
    std::cout << std::dec << "length: " << hdr.len << "    captured: " << hdr.caplen << '\n';

    std::cout.flags(flags);
}

inline bool compare_tcp_headers_nolen(const TCPHeader &h1, const TCPHeader &h2) {
    return h1.sport == h2.sport && h1.dport == h2.dport && h1.seqno == h2.seqno && h1.ackno == h2.ackno &&
           h1.urg == h2.urg && h1.ack == h2.ack && h1.psh == h2.psh && h1.rst == h2.rst && h1.syn == h2.syn &&
           h1.fin == h2.fin && h1.win == h2.win && h1.uptr == h2.uptr;
}

inline bool compare_tcp_headers(const TCPHeader &h1, const TCPHeader &h2) {
    return compare_tcp_headers_nolen(h1, h2) && h1.doff == h2.doff;
}

#endif  // SPONGE_TESTS_TEST_UTILS_HH
