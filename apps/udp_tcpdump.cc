#include "parser.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"
#include "util.hh"

#include <arpa/inet.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <pcap/pcap.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using namespace std;

static void show_usage(const char *arg0, const char *errmsg) {
    cout << "Usage: " << arg0 << " [-i <intf>] [-F <file>] [-h|--help] <expression>\n\n"
         << "  -i <intf>    only capture packets from <intf> (default: all)\n\n"

         << "  -F <file>    reads in a filter expression from <file>\n"
         << "               <expression> is ignored if -F is supplied.\n\n"

         << "  -h, --help   show this message\n\n"
         << "  <expression> a filter expression in pcap-filter(7) syntax\n";

    if (errmsg != nullptr) {
        cout << '\n' << errmsg;
    }
    cout << endl;
}

static void check_arg(char *arg0, int argc, int curr, const char *errmsg) {
    if (curr + 1 >= argc) {
        show_usage(arg0, errmsg);
        exit(1);
    }
}

static int parse_arguments(int argc, char **argv, char **dev_ptr) {
    int curr = 1;
    while (curr < argc) {
        if (strncmp("-i", argv[curr], 3) == 0) {
            check_arg(argv[0], argc, curr, "ERROR: -i requires an argument");
            *dev_ptr = argv[curr + 1];
            curr += 2;

        } else if ((strncmp("-h", argv[curr], 3) == 0) || (strncmp("--help", argv[curr], 7) == 0)) {
            show_usage(argv[0], nullptr);
            exit(0);

        } else {
            break;
        }
    }

    return curr;
}

static string inet4_addr(const uint8_t *data) {
    char addrbuf[128];
    auto *addr = reinterpret_cast<const in_addr *>(data);
    if (inet_ntop(AF_INET, addr, static_cast<char *>(addrbuf), 128) == nullptr) {
        return "unknown";
    }
    return string(static_cast<char *>(addrbuf));
}

static string inet6_addr(const uint8_t *data) {
    char addrbuf[128];
    auto *addr = reinterpret_cast<const in6_addr *>(data);
    if (inet_ntop(AF_INET6, addr, static_cast<char *>(addrbuf), 128) == nullptr) {
        return "unknown";
    }
    return string(static_cast<char *>(addrbuf));
}

static int process_ipv4_ipv6(int len, const uint8_t *data, string &src_addr, string &dst_addr) {
    // this is either an IPv4 or IPv6 packet, we hope
    if (len < 1) {
        return -1;
    }
    int data_offset = 0;
    const uint8_t pt = data[0] & 0xf0;
    if (pt == 0x40) {
        // check packet length and proto
        data_offset = (data[0] & 0x0f) * 4;
        if (len < data_offset) {
            return -1;
        }
        if (data[9] != 0x11) {
            cerr << "Not UDP; ";
            return -1;
        }
        src_addr = inet4_addr(data + 12);
        dst_addr = inet4_addr(data + 16);
    } else if (pt == 0x60) {
        // check packet length
        if (len < 42) {
            return -1;
        }
        data_offset = 40;
        uint8_t nxt = data[6];
        while (nxt != 0x11) {
            if (nxt != 0 && nxt != 43 && nxt != 60) {
                cerr << "Not UDP or fragmented; ";
                return -1;
            }
            nxt = data[data_offset];
            data_offset += 8 * (1 + data[data_offset + 1]);
            if (len < data_offset + 2) {
                return -1;
            }
        }
        src_addr = inet6_addr(data + 8);
        dst_addr = inet6_addr(data + 24);
    } else {
        return -1;
    }

    return data_offset + 8;  // skip UDP header
}

int main(int argc, char **argv) {
    char *dev = nullptr;
    const int exp_start = parse_arguments(argc, argv, &dev);

    // create pcap handle
    if (dev != nullptr) {
        cout << "Capturing on interface " << dev;
    } else {
        cout << "Capturing on all interfaces";
    }
    pcap_t *p_hdl = nullptr;
    const int dl_type = [&] {
        char errbuf[PCAP_ERRBUF_SIZE] = {
            0,
        };
        p_hdl = pcap_open_live(dev, 65535, 0, 100, static_cast<char *>(errbuf));
        if (p_hdl == nullptr) {
            cout << "\nError initiating capture: " << static_cast<char *>(errbuf) << endl;
            exit(1);
        }
        int dlt = pcap_datalink(p_hdl);
        // need to handle: DLT_RAW, DLT_NULL, DLT_EN10MB, DLT_LINUX_SLL
        if (dlt != DLT_RAW && dlt != DLT_NULL && dlt != DLT_EN10MB && dlt != DLT_LINUX_SLL
#ifdef DLT_LINUX_SLL2
            && dlt != DLT_LINUX_SLL2
#endif
        ) {
            cout << "\nError: unsupported datalink type " << pcap_datalink_val_to_description(dlt) << endl;
            exit(1);
        }
        cout << " (type: " << pcap_datalink_val_to_description(dlt) << ")\n";
        return dlt;
    }();

    // compile and set filter
    {
        struct bpf_program p_flt {};
        stringstream f_stream;
        for (int i = exp_start; i < argc; ++i) {
            f_stream << argv[i] << ' ';
        }
        string filter_expression = f_stream.str();
        cout << "Using filter expression: " << filter_expression << "\n";
        if (pcap_compile(p_hdl, &p_flt, filter_expression.c_str(), 1, PCAP_NETMASK_UNKNOWN) != 0) {
            cout << "Error compiling filter expression: " << pcap_geterr(p_hdl) << endl;
            return EXIT_FAILURE;
        }
        if (pcap_setfilter(p_hdl, &p_flt) != 0) {
            cout << "Error configuring packet filter: " << pcap_geterr(p_hdl) << endl;
            return EXIT_FAILURE;
        }
        pcap_freecode(&p_flt);
    }

    int next_ret = 0;
    struct pcap_pkthdr *pkt_hdr = nullptr;
    const uint8_t *pkt_data = nullptr;
    cout << setfill('0');
    while ((next_ret = pcap_next_ex(p_hdl, &pkt_hdr, &pkt_data)) >= 0) {
        if (next_ret == 0) {
            // timeout; just listen again
            continue;
        }

        size_t hdr_off = 0;
        int start_off = 0;
        // figure out where in the datagram to look based on link type
        if (dl_type == DLT_NULL) {
            hdr_off = 4;
            if (pkt_hdr->caplen < hdr_off) {
                cerr << "[INFO] Skipping malformed packet.\n";
                continue;
            }
            const uint8_t pt = pkt_data[3];
            if (pt != 2 && pt != 24 && pt != 28 && pt != 30) {
                cerr << "[INFO] Skipping non-IP packet.\n";
                continue;
            }
        } else if (dl_type == DLT_EN10MB) {
            hdr_off = 14;
            if (pkt_hdr->caplen < hdr_off) {
                cerr << "[INFO] Skipping malformed packet.\n";
                continue;
            }
            const uint16_t pt = (pkt_data[12] << 8) | pkt_data[13];
            if (pt != 0x0800 && pt != 0x86dd) {
                cerr << "[INFO] Skipping non-IP packet.\n";
                continue;
            }
        } else if (dl_type == DLT_LINUX_SLL) {
            hdr_off = 16;
            if (pkt_hdr->caplen < hdr_off) {
                cerr << "[INFO] Skipping malformed packet.\n";
                continue;
            }
            const uint16_t pt = (pkt_data[14] << 8) | pkt_data[15];
            if (pt != 0x0800 && pt != 0x86dd) {
                cerr << "[INFO] Skipping non-IP packet.\n";
                continue;
            }
#ifdef DLT_LINUX_SLL2
        } else if (dl_type == DLT_LINUX_SLL2) {
            if (pkt_hdr->caplen < 20) {
                cerr << "[INFO] Skipping malformed packet.\n";
                continue;
            }
            const uint16_t pt = (pkt_data[0] << 8) | pkt_data[1];
            hdr_off = 20;
            if (pt != 0x0800 && pt != 0x86dd) {
                cerr << "[INFO] Skipping non-IP packet.\n";
                continue;
            }
#endif
        } else if (dl_type != DLT_RAW) {
            cerr << "Mysterious datalink type. Giving up.";
            return EXIT_FAILURE;
        }

        // now actually parse the packet
        string src{}, dst{};
        if ((start_off = process_ipv4_ipv6(pkt_hdr->caplen - hdr_off, pkt_data + hdr_off, src, dst)) < 0) {
            cerr << "Error parsing IPv4/IPv6 packet. Skipping.\n";
            continue;
        }

        // hdr_off + start_off is now the start of the UDP payload
        const size_t payload_off = hdr_off + start_off;
        const size_t payload_len = pkt_hdr->caplen - payload_off;

        string_view payload{reinterpret_cast<const char *>(pkt_data) + payload_off, payload_len};

        // try to parse UDP payload as TCP packet
        auto seg = TCPSegment{};
        if (const auto res = seg.parse(string(payload), 0); res > ParseResult::BadChecksum) {
            cout << "(did not recognize TCP header) src: " << src << " dst: " << dst << '\n';
        } else {
            const TCPHeader &tcp_hdr = seg.header();
            uint32_t seqlen = seg.length_in_sequence_space();

            cout << src << ':' << tcp_hdr.sport << " > " << dst << ':' << tcp_hdr.dport << "\n    Flags ["

                 << (tcp_hdr.urg ? "U" : "") << (tcp_hdr.psh ? "P" : "") << (tcp_hdr.rst ? "R" : "")
                 << (tcp_hdr.syn ? "S" : "") << (tcp_hdr.fin ? "F" : "") << (tcp_hdr.ack ? "." : "")

                 << "] cksum 0x" << hex << setw(4) << tcp_hdr.cksum << dec
                 << (res == ParseResult::NoError ? " (correct)" : " (incorrect!)")

                 << " seq " << tcp_hdr.seqno;

            if (seqlen > 0) {
                cout << ':' << (tcp_hdr.seqno + seqlen);
            }

            cout << " ack " << tcp_hdr.ackno << " win " << tcp_hdr.win << " length " << payload_len << endl;
        }
        hexdump(payload.data(), payload.size(), 8);
    }

    pcap_close(p_hdl);
    if (next_ret == -1) {
        cout << "Error listening for packet: " << pcap_geterr(p_hdl) << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
