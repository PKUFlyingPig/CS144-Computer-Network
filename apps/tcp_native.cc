#include "bidirectional_stream_copy.hh"

#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace std;

void show_usage(const char *argv0) {
    cerr << "Usage: " << argv0 << " [-l] <host> <port>\n\n"
         << "  -l specifies listen mode; <host>:<port> is the listening address." << endl;
}

int main(int argc, char **argv) {
    try {
        bool server_mode = false;
        if (argc < 3 || ((server_mode = (strncmp("-l", argv[1], 3) == 0)) && argc < 4)) {
            show_usage(argv[0]);
            return EXIT_FAILURE;
        }

        // in client mode, connect; in server mode, accept exactly one connection
        auto socket = [&] {
            if (server_mode) {
                TCPSocket listening_socket;                 // create a TCP socket
                listening_socket.set_reuseaddr();           // reuse the server's address as soon as the program quits
                listening_socket.bind({argv[2], argv[3]});  // bind to specified address
                listening_socket.listen();                  // mark the socket as listening for incoming connections
                return listening_socket.accept();           // accept exactly one connection
            }
            TCPSocket connecting_socket;
            connecting_socket.connect({argv[1], argv[2]});
            return connecting_socket;
        }();

        bidirectional_stream_copy(socket);
    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
