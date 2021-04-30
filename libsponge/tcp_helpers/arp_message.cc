#include "arp_message.hh"

#include <arpa/inet.h>
#include <iomanip>
#include <sstream>

using namespace std;

ParseResult ARPMessage::parse(const Buffer buffer) {
    NetParser p{buffer};

    if (p.buffer().size() < ARPMessage::LENGTH) {
        return ParseResult::PacketTooShort;
    }

    hardware_type = p.u16();
    protocol_type = p.u16();
    hardware_address_size = p.u8();
    protocol_address_size = p.u8();
    opcode = p.u16();

    if (not supported()) {
        return ParseResult::Unsupported;
    }

    // read sender addresses (Ethernet and IP)
    for (auto &byte : sender_ethernet_address) {
        byte = p.u8();
    }
    sender_ip_address = p.u32();

    // read target addresses (Ethernet and IP)
    for (auto &byte : target_ethernet_address) {
        byte = p.u8();
    }
    target_ip_address = p.u32();

    return p.get_error();
}

bool ARPMessage::supported() const {
    return hardware_type == TYPE_ETHERNET and protocol_type == EthernetHeader::TYPE_IPv4 and
           hardware_address_size == sizeof(EthernetHeader::src) and protocol_address_size == sizeof(IPv4Header::src) and
           ((opcode == OPCODE_REQUEST) or (opcode == OPCODE_REPLY));
}

string ARPMessage::serialize() const {
    if (not supported()) {
        throw runtime_error(
            "ARPMessage::serialize(): unsupported field combination (must be Ethernet/IP, and request or reply)");
    }

    string ret;
    NetUnparser::u16(ret, hardware_type);
    NetUnparser::u16(ret, protocol_type);
    NetUnparser::u8(ret, hardware_address_size);
    NetUnparser::u8(ret, protocol_address_size);
    NetUnparser::u16(ret, opcode);

    /* write sender addresses */
    for (auto &byte : sender_ethernet_address) {
        NetUnparser::u8(ret, byte);
    }
    NetUnparser::u32(ret, sender_ip_address);

    /* write target addresses */
    for (auto &byte : target_ethernet_address) {
        NetUnparser::u8(ret, byte);
    }
    NetUnparser::u32(ret, target_ip_address);

    return ret;
}

string ARPMessage::to_string() const {
    stringstream ss{};
    string opcode_str = "(unknown type)";
    if (opcode == OPCODE_REQUEST) {
        opcode_str = "REQUEST";
    }
    if (opcode == OPCODE_REPLY) {
        opcode_str = "REPLY";
    }
    ss << "opcode=" << opcode_str << ", sender=" << ::to_string(sender_ethernet_address) << "/"
       << inet_ntoa({htobe32(sender_ip_address)}) << ", target=" << ::to_string(target_ethernet_address) << "/"
       << inet_ntoa({htobe32(target_ip_address)});
    return ss.str();
}
