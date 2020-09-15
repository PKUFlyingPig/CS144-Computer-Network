const uint16_t portnum = ((std::random_device()()) % 50000) + 1025;

// create a TCP socket, bind it to a local address, and listen
TCPSocket sock1;
sock1.bind(Address("127.0.0.1", portnum));
sock1.listen(1);

// create another socket and connect to the first one
TCPSocket sock2;
sock2.connect(Address("127.0.0.1", portnum));

// accept the connection
auto sock3 = sock1.accept();
sock3.write("hi there");

auto recvd = sock2.read();
sock2.write("hi yourself");

auto recvd2 = sock3.read();

sock1.close();              // don't need to accept any more connections
sock2.close();              // you can call close(2) on a socket
sock3.shutdown(SHUT_RDWR);  // you can also shutdown(2) a socket
if (recvd != "hi there" || recvd2 != "hi yourself") {
    throw std::runtime_error("wrong data received");
}
