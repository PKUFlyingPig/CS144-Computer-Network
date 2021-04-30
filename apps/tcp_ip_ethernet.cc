#include "bidirectional_stream_copy.hh"
#include "tcp_config.hh"
#include "tcp_sponge_socket.hh"
#include "tun.hh"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <tuple>

using namespace std;

constexpr const char *TAP_DFLT = "tap10";
const string LOCAL_ADDRESS_DFLT = "169.254.10.9";
const string GATEWAY_DFLT = "169.254.10.1";

static void show_usage(const char *argv0, const char *msg) {
    cout << "Usage: " << argv0 << " [options] <host> <port>\n\n"
         << "   Option                                                          Default\n"
         << "   --                                                              --\n\n"

         << "   -a <addr>       Set IP source address (client mode only)        " << LOCAL_ADDRESS_DFLT << "\n"
         << "   -s <port>       Set TCP source port (client mode only)          (random)\n\n"
         << "   -n <addr>       Set IP next-hop address                         " << GATEWAY_DFLT << "\n"

         << "   -w <winsz>      Use a window of <winsz> bytes                   " << TCPConfig::MAX_PAYLOAD_SIZE
         << "\n\n"

         << "   -t <tmout>      Set rt_timeout to tmout                         " << TCPConfig::TIMEOUT_DFLT << "\n\n"

         << "   -d <tapdev>     Connect to tap <tapdev>                         " << TAP_DFLT << "\n\n"

         << "   -h              Show this message.\n\n";

    if (msg != nullptr) {
        cout << msg;
    }
    cout << endl;
}

static void check_argc(int argc, char **argv, int curr, const char *err) {
    if (curr + 3 >= argc) {
        show_usage(argv[0], err);
        exit(1);
    }
}

static tuple<TCPConfig, FdAdapterConfig, Address, string> get_config(int argc, char **argv) {
    TCPConfig c_fsm{};
    FdAdapterConfig c_filt{};
    string tapdev = TAP_DFLT;

    int curr = 1;

    string source_address = LOCAL_ADDRESS_DFLT;
    string source_port = to_string(uint16_t(random_device()()));
    string next_hop_address = GATEWAY_DFLT;

    while (argc - curr > 2) {
        if (strncmp("-a", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -a requires one argument.");
            source_address = argv[curr + 1];
            curr += 2;

        } else if (strncmp("-s", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -s requires one argument.");
            source_port = argv[curr + 1];
            curr += 2;

        } else if (strncmp("-n", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -n requires one argument.");
            next_hop_address = argv[curr + 1];
            curr += 2;

        } else if (strncmp("-w", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -w requires one argument.");
            c_fsm.recv_capacity = strtol(argv[curr + 1], nullptr, 0);
            curr += 2;

        } else if (strncmp("-t", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -t requires one argument.");
            c_fsm.rt_timeout = strtol(argv[curr + 1], nullptr, 0);
            curr += 2;

        } else if (strncmp("-d", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -t requires one argument.");
            tapdev = argv[curr + 1];
            curr += 2;

        } else if (strncmp("-h", argv[curr], 3) == 0) {
            show_usage(argv[0], nullptr);
            exit(0);

        } else {
            show_usage(argv[0], string("ERROR: unrecognized option " + string(argv[curr])).c_str());
            exit(1);
        }
    }

    // parse positional command-line arguments
    c_filt.destination = {argv[curr], argv[curr + 1]};
    c_filt.source = {source_address, source_port};

    Address next_hop{next_hop_address, "0"};

    return make_tuple(c_fsm, c_filt, next_hop, tapdev);
}

int main(int argc, char **argv) {
    try {
        if (argc < 3) {
            show_usage(argv[0], "ERROR: required arguments are missing.");
            return EXIT_FAILURE;
        }

        // choose a random local Ethernet address (and make sure it's private, i.e. not owned by a manufacturer)
        EthernetAddress local_ethernet_address;
        for (auto &byte : local_ethernet_address) {
            byte = random_device()();  // use a random local Ethernet address
        }
        local_ethernet_address.at(0) |= 0x02;  // "10" in last two binary digits marks a private Ethernet address
        local_ethernet_address.at(0) &= 0xfe;

        auto [c_fsm, c_filt, next_hop, tap_dev_name] = get_config(argc, argv);

        TCPOverIPv4OverEthernetSpongeSocket tcp_socket(TCPOverIPv4OverEthernetAdapter(
            TCPOverIPv4OverEthernetAdapter(TapFD(tap_dev_name), local_ethernet_address, c_filt.source, next_hop)));

        tcp_socket.connect(c_fsm, c_filt);

        bidirectional_stream_copy(tcp_socket);
        tcp_socket.wait_until_closed();
    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
