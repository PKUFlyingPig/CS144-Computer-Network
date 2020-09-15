const uint16_t portnum = ((std::random_device()()) % 50000) + 1025;

// create a UDP socket and bind it to a local address
UDPSocket sock1;
sock1.bind(Address("127.0.0.1", portnum));

// create another UDP socket and send a datagram to the first socket without connecting
UDPSocket sock2;
sock2.sendto(Address("127.0.0.1", portnum), "hi there");

// receive sent datagram, connect the socket to the peer's address, and send a response
auto recvd = sock1.recv();
sock1.connect(recvd.source_address);
sock1.send("hi yourself");

auto recvd2 = sock2.recv();

if (recvd.payload != "hi there" || recvd2.payload != "hi yourself") {
    throw std::runtime_error("wrong data received");
}
