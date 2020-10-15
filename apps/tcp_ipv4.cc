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

constexpr const char *TUN_DFLT = "tun144";
const string LOCAL_ADDRESS_DFLT = "169.254.144.9";

static void show_usage(const char *argv0, const char *msg) {
    cout << "Usage: " << argv0 << " [options] <host> <port>\n\n"
         << "   Option                                                          Default\n"
         << "   --                                                              --\n\n"

         << "   -l              Server (listen) mode.                           (client mode)\n"
         << "                   In server mode, <host>:<port> is the address to bind.\n\n"

         << "   -a <addr>       Set source address (client mode only)           " << LOCAL_ADDRESS_DFLT << "\n"
         << "   -s <port>       Set source port (client mode only)              (random)\n\n"

         << "   -w <winsz>      Use a window of <winsz> bytes                   " << TCPConfig::MAX_PAYLOAD_SIZE
         << "\n\n"

         << "   -t <tmout>      Set rt_timeout to tmout                         " << TCPConfig::TIMEOUT_DFLT << "\n\n"

         << "   -d <tundev>     Connect to tun <tundev>                         " << TUN_DFLT << "\n\n"

         << "   -Lu <loss>      Set uplink loss to <rate> (float in 0..1)       (no loss)\n"
         << "   -Ld <loss>      Set downlink loss to <rate> (float in 0..1)     (no loss)\n\n"

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

static tuple<TCPConfig, FdAdapterConfig, bool, char *> get_config(int argc, char **argv) {
    TCPConfig c_fsm{};
    FdAdapterConfig c_filt{};
    char *tundev = nullptr;

    int curr = 1;
    bool listen = false;

    string source_address = LOCAL_ADDRESS_DFLT;
    string source_port = to_string(uint16_t(random_device()()));

    while (argc - curr > 2) {
        if (strncmp("-l", argv[curr], 3) == 0) {
            listen = true;
            curr += 1;

        } else if (strncmp("-a", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -a requires one argument.");
            source_address = argv[curr + 1];
            curr += 2;

        } else if (strncmp("-s", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -s requires one argument.");
            source_port = argv[curr + 1];
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
            tundev = argv[curr + 1];
            curr += 2;

        } else if (strncmp("-Lu", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -Lu requires one argument.");
            float lossrate = strtof(argv[curr + 1], nullptr);
            using LossRateUpT = decltype(c_filt.loss_rate_up);
            c_filt.loss_rate_up =
                static_cast<LossRateUpT>(static_cast<float>(numeric_limits<LossRateUpT>::max()) * lossrate);
            curr += 2;

        } else if (strncmp("-Ld", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -Lu requires one argument.");
            float lossrate = strtof(argv[curr + 1], nullptr);
            using LossRateDnT = decltype(c_filt.loss_rate_dn);
            c_filt.loss_rate_dn =
                static_cast<LossRateDnT>(static_cast<float>(numeric_limits<LossRateDnT>::max()) * lossrate);
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
    if (listen) {
        c_filt.source = {"0", argv[curr + 1]};
        if (c_filt.source.port() == 0) {
            show_usage(argv[0], "ERROR: listen port cannot be zero in server mode.");
            exit(1);
        }
    } else {
        c_filt.destination = {argv[curr], argv[curr + 1]};
        c_filt.source = {source_address, source_port};
    }

    return make_tuple(c_fsm, c_filt, listen, tundev);
}

int main(int argc, char **argv) {
    try {
        if (argc < 3) {
            show_usage(argv[0], "ERROR: required arguments are missing.");
            return EXIT_FAILURE;
        }

        auto [c_fsm, c_filt, listen, tun_dev_name] = get_config(argc, argv);
        LossyTCPOverIPv4SpongeSocket tcp_socket(LossyTCPOverIPv4OverTunFdAdapter(
            TCPOverIPv4OverTunFdAdapter(TunFD(tun_dev_name == nullptr ? TUN_DFLT : tun_dev_name))));

        if (listen) {
            tcp_socket.listen_and_accept(c_fsm, c_filt);
        } else {
            tcp_socket.connect(c_fsm, c_filt);
        }

        bidirectional_stream_copy(tcp_socket);
        tcp_socket.wait_until_closed();
    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
