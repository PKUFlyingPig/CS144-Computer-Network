#include "parser.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"
#include "test_utils.hh"
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
            vector<uint8_t> test_header(20, 0);
            generate(test_header.begin(), test_header.end(), [&] { return rd(); });
            test_header[12] = 0x50;                 // make sure doff is correct
            test_header[16] = test_header[17] = 0;  // zero out cksum
            const auto checksum = inet_cksum(test_header.data(), test_header.size());
            test_header[16] = checksum >> 8;
            test_header[17] = checksum & 0xff;

            TCPHeader test_1{};
            {
                NetParser p{string(test_header.begin(), test_header.end())};
                if (const auto res = test_1.parse(p); res != ParseResult::NoError) {
                    throw runtime_error("header parse failed: " + as_string(res));
                }
            }
            if (const uint16_t tval = (test_header[0] << 8) | test_header[1]; test_1.sport != tval) {
                throw runtime_error("bad parse: wrong source port");
            }
            if (const uint16_t tval = (test_header[2] << 8) | test_header[3]; test_1.dport != tval) {
                throw runtime_error("bad parse: wrong destination port");
            }
            if (const uint32_t tval =
                    (test_header[4] << 24) | (test_header[5] << 16) | (test_header[6] << 8) | test_header[7];
                test_1.seqno.raw_value() != tval) {
                throw runtime_error("bad parse: wrong seqno");
            }
            if (const uint32_t tval =
                    (test_header[8] << 24) | (test_header[9] << 16) | (test_header[10] << 8) | test_header[11];
                test_1.ackno.raw_value() != tval) {
                throw runtime_error("bad parse: wrong ackno");
            }
            if (const uint8_t tval = (test_1.urg ? 0x20 : 0) | (test_1.ack ? 0x10 : 0) | (test_1.psh ? 0x08 : 0) |
                                     (test_1.rst ? 0x04 : 0) | (test_1.syn ? 0x02 : 0) | (test_1.fin ? 0x01 : 0);
                tval != (test_header[13] & 0x3f)) {
                throw runtime_error("bad parse: bad flags");
            }
            if (const uint16_t tval = (test_header[14] << 8) | test_header[15]; test_1.win != tval) {
                throw runtime_error("bad parse: wrong window value");
            }
            if (test_1.cksum != checksum) {
                throw runtime_error("bad parse: wrong checksum");
            }
            if (const uint16_t tval = (test_header[18] << 8) | test_header[19]; test_1.uptr != tval) {
                throw runtime_error("bad parse: wrong urgent pointer");
            }

            test_header[12] = 0x40;
            {
                const auto new_cksum = inet_cksum(test_header.data(), test_header.size());
                test_header[16] = new_cksum >> 8;
                test_header[17] = new_cksum & 0xff;
                NetParser p{string(test_header.begin(), test_header.end())};
                if (const auto res = test_1.parse(p); res != ParseResult::HeaderTooShort) {
                    throw runtime_error("bad parse: got wrong error for header with bad doff value");
                }
            }

            test_header[12] = 0x50;
            test_header[16] = (checksum >> 8) + max(int(rd()), 1);
            test_header[17] = checksum + max(int(rd()), 1);
            /* // checksum is taken over whole segment, so only TCPSegment parser checks the checksum
                {
                    NetParser p{string(test_header.begin(), test_header.end())};
                    if (const auto res = test_1.parse(p); res != ParseResult::BadChecksum) {
                        throw runtime_error("bad parse: got wrong error for incorrect checksum: " + as_string(res));
                    }
                }
            */

            test_header[12] = 0x60;
            test_header[16] = checksum >> 8;
            test_header[17] = checksum & 0xff;
            {
                NetParser p{string(test_header.begin(), test_header.end())};
                if (const auto res = test_1.parse(p); res != ParseResult::PacketTooShort) {
                    throw runtime_error("bad parse: got wrong error for segment shorter than 4 * doff: " +
                                        as_string(res));
                }
            }

            test_header[12] = 0x50;
            test_header.resize(16);
            {
                NetParser p{string(test_header.begin(), test_header.end())};
                if (const auto res = test_1.parse(p); res != ParseResult::PacketTooShort) {
                    throw runtime_error("bad parse: got wrong error for segment shorter than 20 bytes");
                }
            }
        }

        // now process some segments off the wire for correctness of parser and unparser
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

            if (pkt[12] != 0x08 || pkt[13] != 0x00) {
                continue;
            }
            uint8_t hdrlen = (pkt[14] & 0x0f) << 2;
            uint16_t tlen = (pkt[16] << 8) | pkt[17];
            if (hdr.caplen - 14 != tlen) {
                continue;  // weird! truncated segment
            }
            const uint8_t *const tcp_seg_data = pkt + 14 + hdrlen;
            const auto tcp_seg_len = hdr.caplen - 14 - hdrlen;
            auto [tcp_seg, result] = [&] {
                vector<uint8_t> tcp_data(tcp_seg_data, tcp_seg_data + tcp_seg_len);

                // fix up checksum to remove contribution from IPv4 pseudo-header
                uint32_t cksum_fixup = ((pkt[26] << 8) | pkt[27]) + ((pkt[28] << 8) | pkt[29]);  // src addr
                cksum_fixup += ((pkt[30] << 8) | pkt[31]) + ((pkt[32] << 8) | pkt[33]);          // dst addr
                cksum_fixup += pkt[23];                                                          // proto
                cksum_fixup += tcp_seg_len;                                                      // len
                cksum_fixup += (tcp_data[16] << 8) | tcp_data[17];                               // original cksum
                while (cksum_fixup > 0xffff) {                                                   // carry bits
                    cksum_fixup = (cksum_fixup >> 16) + (cksum_fixup & 0xffff);
                }

                tcp_data[16] = cksum_fixup >> 8;
                tcp_data[17] = cksum_fixup & 0xff;

                TCPSegment tcp_seg_ret;
                const auto parse_result = tcp_seg_ret.parse(string(tcp_data.begin(), tcp_data.end()), 0);
                return make_tuple(tcp_seg_ret, parse_result);
            }();

            if (result != ParseResult::NoError) {
                auto tcp_parse_result = as_string(result);
                cout << "ERROR got unexpected parse failure " << tcp_parse_result << " for this segment:\n";
                hexdump(tcp_seg_data, tcp_seg_len);
                ok = false;
                continue;
            }

            // parse succeeded. Create a new segment and rebuild the header by unparsing.
            cout << dec;

            TCPSegment tcp_seg_copy;
            tcp_seg_copy.payload() = tcp_seg.payload();

            // set headers in new segment, and fix up to remove extensions
            {
                auto &tcp_hdr_orig = tcp_seg.header();
                TCPHeader &tcp_hdr_copy = tcp_seg_copy.header();
                tcp_hdr_copy = tcp_hdr_orig;
                // fix up segment to remove IPv4 and TCP header extensions
                tcp_hdr_copy.doff = 5;
            }  // tcp_hdr_{orig,copy} go out of scope

            if (!compare_tcp_headers_nolen(tcp_seg.header(), tcp_seg_copy.header())) {
                cout << "ERROR: after unparsing, TCP headers (other than length) don't match.\n";
            }

            TCPSegment tcp_seg_copy2;
            if (const auto res = tcp_seg_copy2.parse(tcp_seg_copy.serialize().concatenate());
                res != ParseResult::NoError) {
                auto tcp_parse_result = as_string(res);
                cout << "ERROR got parse failure " << tcp_parse_result << " for this segment:\n";
                cout << endl << "original:\n";
                hexdump(tcp_seg_data, tcp_seg_len);
                ok = false;
                continue;
            }

            if (!compare_tcp_headers_nolen(tcp_seg.header(), tcp_seg_copy2.header())) {
                cout << "ERROR: after re-parsing, TCP headers don't match.\n";
                ok = false;
                continue;
            }
            if (!compare_tcp_headers(tcp_seg_copy.header(), tcp_seg_copy2.header())) {
                cout << "ERROR: after re-parsing, TCP headers don't match.\n";
                ok = false;
                continue;
            }
            if (tcp_seg_copy.payload().str() != tcp_seg_copy2.payload().str()) {
                cout << "ERROR: after re-parsing, TCP payloads don't match.\n";
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
