#include "arp_message.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "network_interface_test_harness.hh"

#include <cstdlib>
#include <iostream>

using namespace std;

EthernetAddress random_private_ethernet_address() {
    EthernetAddress addr;
    for (auto &byte : addr) {
        byte = random_device()();  // use a random local Ethernet address
    }
    addr.at(0) |= 0x02;  // "10" in last two binary digits marks a private Ethernet address
    addr.at(0) &= 0xfe;

    return addr;
}

InternetDatagram make_datagram(const string &src_ip, const string &dst_ip) {
    InternetDatagram dgram;
    dgram.header().src = Address(src_ip, 0).ipv4_numeric();
    dgram.header().dst = Address(dst_ip, 0).ipv4_numeric();
    dgram.payload() = string("hello");
    dgram.header().len = dgram.header().hlen * 4 + dgram.payload().size();
    return dgram;
}

ARPMessage make_arp(const uint16_t opcode,
                    const EthernetAddress sender_ethernet_address,
                    const string sender_ip_address,
                    const EthernetAddress target_ethernet_address,
                    const string target_ip_address) {
    ARPMessage arp;
    arp.opcode = opcode;
    arp.sender_ethernet_address = sender_ethernet_address;
    arp.sender_ip_address = Address(sender_ip_address, 0).ipv4_numeric();
    arp.target_ethernet_address = target_ethernet_address;
    arp.target_ip_address = Address(target_ip_address, 0).ipv4_numeric();
    return arp;
}

EthernetFrame make_frame(const EthernetAddress &src,
                         const EthernetAddress &dst,
                         const uint16_t type,
                         const BufferList payload) {
    EthernetFrame frame;
    frame.header().src = src;
    frame.header().dst = dst;
    frame.header().type = type;
    frame.payload() = payload.concatenate();
    return frame;
}

int main() {
    try {
        {
            const EthernetAddress local_eth = random_private_ethernet_address();
            NetworkInterfaceTestHarness test{"typical ARP workflow", local_eth, Address("4.3.2.1", 0)};

            const auto datagram = make_datagram("5.6.7.8", "13.12.11.10");
            test.execute(SendDatagram{datagram, Address("192.168.0.1", 0)});

            // outgoing datagram should result in an ARP request
            test.execute(ExpectFrame{
                make_frame(local_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "4.3.2.1", {}, "192.168.0.1").serialize())});
            test.execute(ExpectNoFrame{});
            const EthernetAddress target_eth = random_private_ethernet_address();

            test.execute(Tick{800});
            test.execute(ExpectNoFrame{});

            // ARP reply should result in the queued datagram getting sent
            test.execute(ReceiveFrame{
                make_frame(
                    target_eth,
                    local_eth,
                    EthernetHeader::TYPE_ARP,
                    make_arp(ARPMessage::OPCODE_REPLY, target_eth, "192.168.0.1", local_eth, "4.3.2.1").serialize()),
                {}});

            test.execute(
                ExpectFrame{make_frame(local_eth, target_eth, EthernetHeader::TYPE_IPv4, datagram.serialize())});
            test.execute(ExpectNoFrame{});

            // any IP reply directed for our Ethernet address should be passed up the stack
            const auto reply_datagram = make_datagram("13.12.11.10", "5.6.7.8");
            test.execute(
                ReceiveFrame(make_frame(target_eth, local_eth, EthernetHeader::TYPE_IPv4, reply_datagram.serialize()),
                             reply_datagram));
            test.execute(ExpectNoFrame{});

            // incoming frames to another Ethernet address (not ours) should be ignored
            const EthernetAddress another_eth = {1, 1, 1, 1, 1, 1};
            test.execute(ReceiveFrame(
                make_frame(target_eth, another_eth, EthernetHeader::TYPE_IPv4, reply_datagram.serialize()), {}));
        }

        {
            const EthernetAddress local_eth = random_private_ethernet_address();
            const EthernetAddress remote_eth = random_private_ethernet_address();
            NetworkInterfaceTestHarness test{"reply to ARP request", local_eth, Address("5.5.5.5", 0)};
            test.execute(ReceiveFrame{
                make_frame(remote_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REQUEST, remote_eth, "10.0.1.1", {}, "7.7.7.7").serialize()),
                {}});
            test.execute(ExpectNoFrame{});
            test.execute(ReceiveFrame{
                make_frame(remote_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REQUEST, remote_eth, "10.0.1.1", {}, "5.5.5.5").serialize()),
                {}});
            test.execute(ExpectFrame{make_frame(
                local_eth,
                remote_eth,
                EthernetHeader::TYPE_ARP,
                make_arp(ARPMessage::OPCODE_REPLY, local_eth, "5.5.5.5", remote_eth, "10.0.1.1").serialize())});
            test.execute(ExpectNoFrame{});
        }

        {
            const EthernetAddress local_eth = random_private_ethernet_address();
            const EthernetAddress remote_eth = random_private_ethernet_address();
            NetworkInterfaceTestHarness test{"learn from ARP request", local_eth, Address("5.5.5.5", 0)};
            test.execute(ReceiveFrame{
                make_frame(remote_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REQUEST, remote_eth, "10.0.1.1", {}, "5.5.5.5").serialize()),
                {}});
            test.execute(ExpectFrame{make_frame(
                local_eth,
                remote_eth,
                EthernetHeader::TYPE_ARP,
                make_arp(ARPMessage::OPCODE_REPLY, local_eth, "5.5.5.5", remote_eth, "10.0.1.1").serialize())});
            test.execute(ExpectNoFrame{});

            const auto datagram = make_datagram("5.6.7.8", "13.12.11.10");
            test.execute(SendDatagram{datagram, Address("10.0.1.1", 0)});
            test.execute(
                ExpectFrame{make_frame(local_eth, remote_eth, EthernetHeader::TYPE_IPv4, datagram.serialize())});
            test.execute(ExpectNoFrame{});
        }

        {
            const EthernetAddress local_eth = random_private_ethernet_address();
            NetworkInterfaceTestHarness test{"pending mappings last five seconds", local_eth, Address("1.2.3.4", 0)};

            test.execute(SendDatagram{make_datagram("5.6.7.8", "13.12.11.10"), Address("10.0.0.1", 0)});
            test.execute(ExpectFrame{
                make_frame(local_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "1.2.3.4", {}, "10.0.0.1").serialize())});
            test.execute(ExpectNoFrame{});
            test.execute(Tick{4990});
            test.execute(SendDatagram{make_datagram("17.17.17.17", "18.18.18.18"), Address("10.0.0.1", 0)});
            test.execute(ExpectNoFrame{});
            test.execute(Tick{20});
            // pending mapping should now expire
            test.execute(SendDatagram{make_datagram("42.41.40.39", "13.12.11.10"), Address("10.0.0.1", 0)});
            test.execute(ExpectFrame{
                make_frame(local_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "1.2.3.4", {}, "10.0.0.1").serialize())});
            test.execute(ExpectNoFrame{});
        }

        {
            const EthernetAddress local_eth = random_private_ethernet_address();
            NetworkInterfaceTestHarness test{"active mappings last 30 seconds", local_eth, Address("4.3.2.1", 0)};

            const auto datagram = make_datagram("5.6.7.8", "13.12.11.10");
            const auto datagram2 = make_datagram("5.6.7.8", "13.12.11.11");
            const auto datagram3 = make_datagram("5.6.7.8", "13.12.11.12");
            const auto datagram4 = make_datagram("5.6.7.8", "13.12.11.13");

            test.execute(SendDatagram{datagram, Address("192.168.0.1", 0)});
            test.execute(ExpectFrame{
                make_frame(local_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "4.3.2.1", {}, "192.168.0.1").serialize())});
            const EthernetAddress target_eth = random_private_ethernet_address();
            test.execute(ReceiveFrame{
                make_frame(
                    target_eth,
                    local_eth,
                    EthernetHeader::TYPE_ARP,
                    make_arp(ARPMessage::OPCODE_REPLY, target_eth, "192.168.0.1", local_eth, "4.3.2.1").serialize()),
                {}});

            test.execute(
                ExpectFrame{make_frame(local_eth, target_eth, EthernetHeader::TYPE_IPv4, datagram.serialize())});
            test.execute(ExpectNoFrame{});

            // try after 10 seconds -- no ARP should be generated
            test.execute(Tick{10000});
            test.execute(SendDatagram{datagram2, Address("192.168.0.1", 0)});
            test.execute(
                ExpectFrame{make_frame(local_eth, target_eth, EthernetHeader::TYPE_IPv4, datagram2.serialize())});
            test.execute(ExpectNoFrame{});

            // another 10 seconds -- no ARP should be generated
            test.execute(Tick{10000});
            test.execute(SendDatagram{datagram3, Address("192.168.0.1", 0)});
            test.execute(
                ExpectFrame{make_frame(local_eth, target_eth, EthernetHeader::TYPE_IPv4, datagram3.serialize())});
            test.execute(ExpectNoFrame{});

            // after another 11 seconds, need to ARP again
            test.execute(Tick{11000});
            test.execute(SendDatagram{datagram4, Address("192.168.0.1", 0)});
            test.execute(ExpectFrame{
                make_frame(local_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "4.3.2.1", {}, "192.168.0.1").serialize())});
            test.execute(ExpectNoFrame{});

            // ARP reply again
            const EthernetAddress new_target_eth = random_private_ethernet_address();
            test.execute(ReceiveFrame{
                make_frame(new_target_eth,
                           local_eth,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REPLY, new_target_eth, "192.168.0.1", local_eth, "4.3.2.1")
                               .serialize()),
                {}});
            test.execute(
                ExpectFrame{make_frame(local_eth, new_target_eth, EthernetHeader::TYPE_IPv4, datagram4.serialize())});
            test.execute(ExpectNoFrame{});
        }

        {
            const EthernetAddress local_eth = random_private_ethernet_address();
            const EthernetAddress remote_eth1 = random_private_ethernet_address();
            const EthernetAddress remote_eth2 = random_private_ethernet_address();
            NetworkInterfaceTestHarness test{
                "different ARP mappings are independent", local_eth, Address("10.0.0.1", 0)};

            // first ARP mapping
            test.execute(ReceiveFrame{
                make_frame(remote_eth1,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REQUEST, remote_eth1, "10.0.0.5", {}, "10.0.0.1").serialize()),
                {}});
            test.execute(ExpectFrame{make_frame(
                local_eth,
                remote_eth1,
                EthernetHeader::TYPE_ARP,
                make_arp(ARPMessage::OPCODE_REPLY, local_eth, "10.0.0.1", remote_eth1, "10.0.0.5").serialize())});
            test.execute(ExpectNoFrame{});

            test.execute(Tick{15000});

            // second ARP mapping
            test.execute(ReceiveFrame{
                make_frame(remote_eth2,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REQUEST, remote_eth2, "10.0.0.19", {}, "10.0.0.1").serialize()),
                {}});
            test.execute(ExpectFrame{make_frame(
                local_eth,
                remote_eth2,
                EthernetHeader::TYPE_ARP,
                make_arp(ARPMessage::OPCODE_REPLY, local_eth, "10.0.0.1", remote_eth2, "10.0.0.19").serialize())});
            test.execute(ExpectNoFrame{});

            test.execute(Tick{10000});

            // outgoing datagram to first destination
            const auto datagram = make_datagram("5.6.7.8", "13.12.11.10");
            test.execute(SendDatagram{datagram, Address("10.0.0.5", 0)});

            // outgoing datagram to second destination
            const auto datagram2 = make_datagram("100.99.98.97", "4.10.4.10");
            test.execute(SendDatagram{datagram2, Address("10.0.0.19", 0)});

            test.execute(
                ExpectFrame{make_frame(local_eth, remote_eth1, EthernetHeader::TYPE_IPv4, datagram.serialize())});
            test.execute(
                ExpectFrame{make_frame(local_eth, remote_eth2, EthernetHeader::TYPE_IPv4, datagram2.serialize())});
            test.execute(ExpectNoFrame{});

            test.execute(Tick{5010});

            // outgoing datagram to second destination (mapping still alive)
            const auto datagram3 = make_datagram("150.140.130.120", "144.144.144.144");
            test.execute(SendDatagram{datagram3, Address("10.0.0.19", 0)});
            test.execute(
                ExpectFrame{make_frame(local_eth, remote_eth2, EthernetHeader::TYPE_IPv4, datagram3.serialize())});
            test.execute(ExpectNoFrame{});

            // outgoing datagram to second destination (mapping has expired)
            const auto datagram4 = make_datagram("244.244.244.244", "3.3.3.3");
            test.execute(SendDatagram{datagram4, Address("10.0.0.5", 0)});
            test.execute(ExpectFrame{
                make_frame(local_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "10.0.0.1", {}, "10.0.0.5").serialize())});
            test.execute(ExpectNoFrame{});
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
