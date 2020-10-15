#include "ipv4_datagram.hh"
#include "ipv4_header.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"
#include "test_utils.hh"
#include "test_utils_ipv4.hh"
#include "util.hh"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <pcap/pcap.h>
#include <string>
#include <vector>

using namespace std;

constexpr unsigned NREPS = 32;

uint16_t inet_cksum(const uint8_t *data, const size_t len) {
    InternetChecksum check;
    check.add({reinterpret_cast<const char *>(data), len});
    return check.value();
}

int main(int argc, char **argv) {
    try {
        // first, make sure the parser gets the correct values and catches errors
        auto rd = get_random_generator();
        for (unsigned i = 0; i < NREPS; ++i) {
            const uint16_t totlen = 30 + rd() % 1024;
            vector<uint8_t> test_header(totlen, 0);
            generate(test_header.begin(), test_header.end(), [&] { return rd(); });
            test_header[0] = 0x45;  // IPv4, length 20
            test_header[2] = totlen >> 8;
            test_header[3] = totlen & 0xff;
            test_header[14] = test_header[15] = 0;
            const uint16_t cksum = inet_cksum(test_header.data(), 20);
            test_header[14] = cksum >> 8;
            test_header[15] = cksum & 0xff;

            IPv4Header test_hdr{};
            {
                NetParser p{string(test_header.begin(), test_header.end())};
                if (auto ret = test_hdr.parse(p); ret != ParseResult::NoError) {
                    throw runtime_error("Parse error: " + as_string(ret));
                }
            }
            if (test_hdr.tos != test_header[1]) {
                throw runtime_error("Parse error: tos field is wrong");
            }
            if (test_hdr.len != totlen) {
                throw runtime_error("Parse error: len field is wrong");
            }
            if (uint16_t tval = (test_header[4] << 8) | test_header[5]; test_hdr.id != tval) {
                throw runtime_error("Parse error: id field is wrong");
            }
            if (uint16_t tval = ((test_header[6] & 0x7f) << 8) | test_header[7];
                tval != ((test_hdr.df ? 0x4000 : 0) | (test_hdr.mf ? 0x2000 : 0) | test_hdr.offset)) {
                throw runtime_error("Parse error: flags or fragment offset is wrong");
            }
            if (test_header[8] != test_hdr.ttl) {
                throw runtime_error("Parse error: ttl is wrong");
            }
            if (test_header[9] != test_hdr.proto) {
                throw runtime_error("Parse error: proto is wrong");
            }
            if (uint16_t tval = (test_header[10] << 8) | test_header[11]; test_hdr.cksum != tval) {
                throw runtime_error("Parse error: cksum is wrong");
            }
            if (uint32_t tval =
                    (test_header[12] << 24) | (test_header[13] << 16) | (test_header[14] << 8) | test_header[15];
                test_hdr.src != tval) {
                throw runtime_error("Parse error: src addr is wrong");
            }
            if (uint32_t tval =
                    (test_header[16] << 24) | (test_header[17] << 16) | (test_header[18] << 8) | test_header[19];
                test_hdr.dst != tval) {
                throw runtime_error("Parse error: dst addr is wrong");
            }

            test_header[0] = 0x55;
            {
                const uint16_t new_cksum = inet_cksum(test_header.data(), 20);
                test_header[14] = new_cksum >> 8;
                test_header[15] = new_cksum & 0xff;
                NetParser p{string(test_header.begin(), test_header.end())};
                if (auto ret = test_hdr.parse(p); ret != ParseResult::WrongIPVersion) {
                    throw runtime_error("Parse error: failed to detect wrong IP version.");
                }
            }

            test_header[0] = 0x44;
            {
                const uint16_t new_cksum = inet_cksum(test_header.data(), 20);
                test_header[14] = new_cksum >> 8;
                test_header[15] = new_cksum & 0xff;
                NetParser p{string(test_header.begin(), test_header.end())};
                if (auto ret = test_hdr.parse(p); ret != ParseResult::HeaderTooShort) {
                    throw runtime_error("Parse error: failed to detect header too short");
                }
            }

            test_header[0] = 0x45;
            test_header[14] = (cksum >> 8) + max(int(rd()), 1);
            test_header[15] = (cksum & 0xff) + max(int(rd()), 1);
            {
                NetParser p{string(test_header.begin(), test_header.end())};
                if (auto ret = test_hdr.parse(p); ret != ParseResult::BadChecksum) {
                    throw runtime_error("Parse error: failed to detect incorrect checksum");
                }
            }

            test_header.resize(totlen - 10);
            {
                NetParser p{string(test_header.begin(), test_header.end())};
                if (auto ret = test_hdr.parse(p); ret != ParseResult::TruncatedPacket) {
                    throw runtime_error("Parse error: failed to detect truncated packet");
                }
            }

            test_header[0] = 0x46;
            test_header.resize(20);
            {
                const uint16_t new_cksum = inet_cksum(test_header.data(), 20);
                test_header[14] = new_cksum >> 8;
                test_header[15] = new_cksum & 0xff;
                NetParser p{string(test_header.begin(), test_header.end())};
                if (auto ret = test_hdr.parse(p); ret != ParseResult::PacketTooShort) {
                    throw runtime_error("Parse error: failed to detect packet too short to parse");
                }
            }

            test_header[0] = 0x45;
            test_header[14] = (cksum >> 8) + max(int(rd()), 1);
            test_header[15] = (cksum & 0xff) + max(int(rd()), 1);
            test_header.resize(16);
            {
                NetParser p{string(test_header.begin(), test_header.end())};
                if (auto ret = test_hdr.parse(p); ret != ParseResult::PacketTooShort) {
                    throw runtime_error("Parse error: failed to detect packet too short to parse");
                }
            }
        }

        if (argc < 2) {
            cout << "USAGE: " << argv[0] << " <filename>" << endl;
            return EXIT_FAILURE;
        }

        char errbuf[PCAP_ERRBUF_SIZE];
        pcap_t *pcap = pcap_open_offline(argv[1], static_cast<char *>(errbuf));
        if (pcap == nullptr) {
            cout << "ERROR opening " << argv[1] << ": " << static_cast<char *>(errbuf) << endl;
            return EXIT_FAILURE;
        }

        if (pcap_datalink(pcap) != 1) {
            cout << "ERROR expected ethernet linktype in capture file" << endl;
            return EXIT_FAILURE;
        }

        bool ok = true;
        const uint8_t *pkt;
        struct pcap_pkthdr hdr;
        while ((pkt = pcap_next(pcap, &hdr)) != nullptr) {
            if (hdr.caplen < 14) {
                cout << "ERROR frame too short to contain Ethernet header\n";
                ok = false;
                continue;
            }

            const bool expect_fail = (pkt[12] != 0x08) || (pkt[13] != 0x00);
            IPv4Datagram ip_dgram;
            if (auto res = ip_dgram.parse(string(pkt + 14, pkt + hdr.caplen)); res != ParseResult::NoError) {
                // parse failed
                if (expect_fail) {
                    continue;
                }

                auto ip_parse_result = as_string(res);
                cout << "ERROR got unexpected IP parse failure " << ip_parse_result << " for this datagram:\n";
                show_ethernet_frame(pkt, hdr);
                hexdump(pkt + 14, hdr.caplen - 14);
                ok = false;
                continue;
            }

            TCPSegment tcp_seg;
            if (auto res = tcp_seg.parse(ip_dgram.payload(), ip_dgram.header().pseudo_cksum());
                res != ParseResult::NoError) {
                // parse failed
                if (expect_fail) {
                    continue;
                }

                auto tcp_parse_result = as_string(res);
                cout << "ERROR got unexpected TCP parse failure " << tcp_parse_result << " for this segment:\n";
                show_ethernet_frame(pkt, hdr);
                hexdump(pkt + 14, hdr.caplen - 14);
                ok = false;
                continue;
            }

            if (expect_fail) {
                cout << "ERROR: expected parse failure but got success. Something is wrong.\n";
                show_ethernet_frame(pkt, hdr);
                hexdump(pkt + 14, hdr.caplen - 14);
                ok = false;
                continue;
            }

            // parse succeeded. Create a new packet and rebuild the header by unparsing.
            cout << dec;

            IPv4Datagram ip_dgram_copy;
            TCPSegment tcp_seg_copy;
            tcp_seg_copy.payload() = tcp_seg.payload();

            // set headers in new packets, and fix up to remove extensions
            {
                const IPv4Header &ipv4_hdr_orig = ip_dgram.header();
                IPv4Header &ipv4_hdr_copy = ip_dgram_copy.header();
                ipv4_hdr_copy = ipv4_hdr_orig;

                const TCPHeader &tcp_hdr_orig = tcp_seg.header();
                TCPHeader &tcp_hdr_copy = tcp_seg_copy.header();
                tcp_hdr_copy = tcp_hdr_orig;

                // fix up packets to remove IPv4 and TCP header extensions
                ipv4_hdr_copy.len -= 4 * ipv4_hdr_orig.hlen - IPv4Header::LENGTH;
                ipv4_hdr_copy.hlen = 5;
                ipv4_hdr_copy.len -= 4 * tcp_hdr_orig.doff - TCPHeader::LENGTH;
                tcp_hdr_copy.doff = 5;
            }  // ipv4_hdr_{orig,copy}, tcp_hdr_{orig,copy} go out of scope

            if (!compare_ip_headers_nolen(ip_dgram.header(), ip_dgram_copy.header())) {
                cout << "ERROR: after unparsing, IP headers (other than length) don't match.\n";
            }
            if (!compare_tcp_headers_nolen(tcp_seg.header(), tcp_seg_copy.header())) {
                cout << "ERROR: after unparsing, TCP headers (other than length) don't match.\n";
            }

            // create a new datagram from the serialized IP and TCP headers + payload
            IPv4Datagram ip_dgram_copy2;

            string concat;
            concat.append(ip_dgram_copy.header().serialize());
            concat.append(tcp_seg_copy.serialize(ip_dgram_copy.header().pseudo_cksum()).concatenate());

            if (auto res = ip_dgram_copy2.parse(string(concat)); res != ParseResult::NoError) {
                auto ip_parse_result = as_string(res);
                cout << "ERROR got IP parse failure " << ip_parse_result << " for this datagram (copy2):\n";
                cout << ip_dgram_copy.header().to_string();
                hexdump(concat.data(), concat.size());
                cout << endl;
                hexdump(pkt + 14, hdr.caplen - 14);
                ok = false;
                continue;
            }

            TCPSegment tcp_seg_copy2;
            if (auto res = tcp_seg_copy2.parse(ip_dgram_copy2.payload(), ip_dgram_copy2.header().pseudo_cksum());
                res != ParseResult::NoError) {
                auto tcp_parse_result = as_string(res);
                cout << "ERROR got TCP parse failure " << tcp_parse_result << " for this segment (copy2):\n";
                cout << tcp_seg_copy.header().to_string();
                ok = false;
                continue;
            }

            if (!compare_ip_headers_nolen(ip_dgram.header(), ip_dgram_copy2.header())) {
                cout << "ERROR: after re-parsing, IP headers don't match (0<->2).\n";
                ok = false;
                continue;
            }
            if (!compare_ip_headers(ip_dgram_copy.header(), ip_dgram_copy2.header())) {
                cout << "ERROR: after re-parsing, IP headers don't match (1<->2).\n";
                ok = false;
                continue;
            }
            if (!compare_tcp_headers_nolen(tcp_seg.header(), tcp_seg_copy2.header())) {
                cout << "ERROR: after re-parsing, TCP headers don't match (0<->2).\n";
                ok = false;
                continue;
            }
            if (!compare_tcp_headers(tcp_seg_copy.header(), tcp_seg_copy2.header())) {
                cout << "ERROR: after re-parsing, TCP headers don't match (1<->2).\n";
                ok = false;
                continue;
            }
            if (tcp_seg_copy.payload().str() != tcp_seg_copy2.payload().str()) {
                cout << "ERROR: after re-parsing, TCP payloads don't match.\n";
                hexdump(tcp_seg_copy2.payload().str().data(), tcp_seg_copy2.payload().str().size());
                cout << endl;
                hexdump(tcp_seg_copy.payload().str().data(), tcp_seg_copy.payload().str().size());
                ok = false;
                continue;
            }
        }

        pcap_close(pcap);
        if (!ok) {
            return EXIT_FAILURE;
        }
    } catch (const exception &e) {
        cout << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
