#include "bidirectional_stream_copy.hh"
#include "tcp_config.hh"
#include "tcp_sponge_socket.hh"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <tuple>

using namespace std;

constexpr uint16_t DPORT_DFLT = 1440;

static void show_usage(const char *argv0, const char *msg) {
    cout << "Usage: " << argv0 << " [options] <host> <port>\n\n"

         << "   Option                                                          Default\n"
         << "   --                                                              --\n\n"

         << "   -l              Server (listen) mode.                           (client mode)\n"
         << "                   In server mode, <host>:<port> is the address to bind.\n\n"

         << "   -w <winsz>      Use a window of <winsz> bytes                   " << TCPConfig::MAX_PAYLOAD_SIZE
         << "\n\n"

         << "   -t <tmout>      Set rt_timeout to tmout                         " << TCPConfig::TIMEOUT_DFLT << "\n\n"

         << "   -Lu <loss>      Set uplink loss to <rate> (float in 0..1)       (no loss)\n"
         << "   -Ld <loss>      Set downlink loss to <rate> (float in 0..1)     (no loss)\n\n"

         << "   -h              Show this message and quit.\n\n";

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

static tuple<TCPConfig, FdAdapterConfig, bool> get_config(int argc, char **argv) {
    TCPConfig c_fsm{};
    FdAdapterConfig c_filt{};

    int curr = 1;
    bool listen = false;

    while (argc - curr > 2) {
        if (strncmp("-l", argv[curr], 3) == 0) {
            listen = true;
            curr += 1;

        } else if (strncmp("-w", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -w requires one argument.");
            c_fsm.recv_capacity = strtol(argv[curr + 1], nullptr, 0);
            curr += 2;

        } else if (strncmp("-t", argv[curr], 3) == 0) {
            check_argc(argc, argv, curr, "ERROR: -t requires one argument.");
            c_fsm.rt_timeout = strtol(argv[curr + 1], nullptr, 0);
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
            show_usage(argv[0], std::string("ERROR: unrecognized option " + std::string(argv[curr])).c_str());
            exit(1);
        }
    }

    if (listen) {
        c_filt.source = {"0", argv[argc - 1]};
    } else {
        c_filt.destination = {argv[argc - 2], argv[argc - 1]};
    }

    return make_tuple(c_fsm, c_filt, listen);
}

int main(int argc, char **argv) {
    try {
        if (argc < 3) {
            show_usage(argv[0], "ERROR: required arguments are missing.");
            exit(1);
        }

        // handle configuration and UDP setup from cmdline arguments
        auto [c_fsm, c_filt, listen] = get_config(argc, argv);

        // build a TCP FSM on top of the UDP socket
        UDPSocket udp_sock;
        if (listen) {
            udp_sock.bind(c_filt.source);
        }
        LossyTCPOverUDPSpongeSocket tcp_socket(LossyTCPOverUDPSocketAdapter(TCPOverUDPSocketAdapter(move(udp_sock))));
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
