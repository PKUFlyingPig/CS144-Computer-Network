#include "bidirectional_stream_copy.hh"

#include "byte_stream.hh"
#include "eventloop.hh"

#include <algorithm>
#include <iostream>
#include <unistd.h>

using namespace std;

void bidirectional_stream_copy(Socket &socket) {
    constexpr size_t max_copy_length = 65536;
    constexpr size_t buffer_size = 1048576;

    EventLoop _eventloop{};
    FileDescriptor _input{STDIN_FILENO};
    FileDescriptor _output{STDOUT_FILENO};
    ByteStream _outbound{buffer_size};
    ByteStream _inbound{buffer_size};
    bool _outbound_shutdown{false};
    bool _inbound_shutdown{false};

    socket.set_blocking(false);
    _input.set_blocking(false);
    _output.set_blocking(false);

    // rule 1: read from stdin into outbound byte stream
    _eventloop.add_rule(
        _input,
        Direction::In,
        [&] {
            _outbound.write(_input.read(_outbound.remaining_capacity()));
            if (_input.eof()) {
                _outbound.end_input();
            }
        },
        [&] { return (not _outbound.error()) and (_outbound.remaining_capacity() > 0) and (not _inbound.error()); },
        [&] { _outbound.end_input(); });

    // rule 2: read from outbound byte stream into socket
    _eventloop.add_rule(socket,
                        Direction::Out,
                        [&] {
                            const size_t bytes_to_write = min(max_copy_length, _outbound.buffer_size());
                            const size_t bytes_written = socket.write(_outbound.peek_output(bytes_to_write), false);
                            _outbound.pop_output(bytes_written);
                            if (_outbound.eof()) {
                                socket.shutdown(SHUT_WR);
                                _outbound_shutdown = true;
                            }
                        },
                        [&] { return (not _outbound.buffer_empty()) or (_outbound.eof() and not _outbound_shutdown); },
                        [&] { _outbound.end_input(); });

    // rule 3: read from socket into inbound byte stream
    _eventloop.add_rule(
        socket,
        Direction::In,
        [&] {
            _inbound.write(socket.read(_inbound.remaining_capacity()));
            if (socket.eof()) {
                _inbound.end_input();
            }
        },
        [&] { return (not _inbound.error()) and (_inbound.remaining_capacity() > 0) and (not _outbound.error()); },
        [&] { _inbound.end_input(); });

    // rule 4: read from inbound byte stream into stdout
    _eventloop.add_rule(_output,
                        Direction::Out,
                        [&] {
                            const size_t bytes_to_write = min(max_copy_length, _inbound.buffer_size());
                            const size_t bytes_written = _output.write(_inbound.peek_output(bytes_to_write), false);
                            _inbound.pop_output(bytes_written);

                            if (_inbound.eof()) {
                                _output.close();
                                _inbound_shutdown = true;
                            }
                        },
                        [&] { return (not _inbound.buffer_empty()) or (_inbound.eof() and not _inbound_shutdown); },
                        [&] { _inbound.end_input(); });

    // loop until completion
    while (true) {
        if (EventLoop::Result::Exit == _eventloop.wait_next_event(-1)) {
            return;
        }
    }
}
