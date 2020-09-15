// create a pair of stream sockets
std::array<int, 2> fds{};
SystemCall("socketpair", ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds.data()));
LocalStreamSocket pipe1{FileDescriptor(fds[0])}, pipe2{FileDescriptor(fds[1])};

pipe1.write("hi there");
auto recvd = pipe2.read();

pipe2.write("hi yourself");
auto recvd2 = pipe1.read();

if (recvd != "hi there" || recvd2 != "hi yourself") {
    throw std::runtime_error("wrong data received");
}
