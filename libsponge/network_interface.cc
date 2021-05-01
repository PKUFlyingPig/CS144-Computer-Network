#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

optional<EthernetAddress> NetworkInterface::get_EthernetAdress(const uint32_t ip_addr) {
    optional<EthernetAddress> ret = nullopt;
    map<uint32_t, EthernetAddressEntry>::iterator iter;
    iter = _cache.find(ip_addr);
    if (iter != _cache.end()) {
        ret = iter->second.MAC_address;
    }
    return ret;
}

optional<NetworkInterface::WaitingList> NetworkInterface::get_WaitingList(const uint32_t ip_addr) {
    optional<WaitingList> ret = nullopt;
    map<uint32_t, NetworkInterface::WaitingList>::iterator iter;
    iter = _queue_map.find(ip_addr);
    if (iter != _queue_map.end()) {
        ret = iter->second;
    }
    return ret;
}

//! \param[in] MAC_addr the destination Ethernet Address
//! \param[in] dgram the IPv4 datagram to be sent
void NetworkInterface::send_helper(const EthernetAddress MAC_addr, const InternetDatagram &dgram) {
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src = _ethernet_address;
    frame.header().dst = MAC_addr;
    frame.payload() = dgram.serialize();
    _frames_out.push(frame);
}

//! \param[in] ipaddr the IPv4 address waits for resolving
//! \param[in] dgram the IPv4 datagram queued to be sent
//! push the datagram into the waiting queue
//! resend ARP request if a new ARP request need to be sent ,i.e., the last request was sent over 5 seconds ago or there is no request sent before
void NetworkInterface::queue_helper(const uint32_t ip_addr, const InternetDatagram &dgram) {
    optional<WaitingList> wait_list = get_WaitingList(ip_addr);
    bool send_ARP = false;
    if (wait_list.has_value()) {
        wait_list.value().waiting_datagram.push(dgram);        
        send_ARP = wait_list.value().time_since_last_ARP_request_send >= NetworkInterface::MAX_RETX_WAITING_TIME;
    } else {
        WaitingList new_wait_list;
        new_wait_list.waiting_datagram.push(dgram);
        _queue_map[ip_addr] = new_wait_list; 
        send_ARP = true;
    }
    if (send_ARP) send_ARP_request(ip_addr);
}

void NetworkInterface::send_ARP_request(const uint32_t ip_addr) {
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.header().src = _ethernet_address;
    frame.header().dst = ETHERNET_BROADCAST;
    ARPMessage arp;
    arp.opcode = ARPMessage::OPCODE_REQUEST;
    arp.sender_ethernet_address = _ethernet_address;
    arp.sender_ip_address = _ip_address.ipv4_numeric();
  //arp.target_ethernet_address = unknown address
    arp.target_ip_address = ip_addr;
    frame.payload() = BufferList(arp.serialize());
    _frames_out.push(frame);
}

void NetworkInterface::send_ARP_reply(const uint32_t ip_addr, const EthernetAddress& MAC_addr) {
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.header().src = _ethernet_address;
    frame.header().dst = MAC_addr;
    ARPMessage arp;
    arp.opcode = ARPMessage::OPCODE_REPLY;
    arp.sender_ethernet_address = _ethernet_address;
    arp.sender_ip_address = _ip_address.ipv4_numeric();
    arp.target_ethernet_address = MAC_addr;
    arp.target_ip_address = ip_addr;
    frame.payload() = BufferList(arp.serialize());
    _frames_out.push(frame);
}
bool NetworkInterface::valid_frame(const EthernetFrame &frame) {
    EthernetAddress dst = frame.header().dst;
    return dst == _ethernet_address || dst == ETHERNET_BROADCAST; 
}

void NetworkInterface::cache_mapping(uint32_t ip_addr, EthernetAddress MAC_addr) {
    map<uint32_t, EthernetAddressEntry>::iterator iter;
    iter = _cache.find(ip_addr);
    if (iter != _cache.end()) {
        // update the cache
        iter->second.caching_time = 0;
        iter->second.MAC_address = MAC_addr;
    } else {
        // add new entry
        EthernetAddressEntry entry;
        entry.caching_time = 0;
        entry.MAC_address = MAC_addr;
        _cache[ip_addr] = entry;
    }
}

void NetworkInterface::clear_waitinglist(uint32_t ip_addr, EthernetAddress MAC_addr) {
    map<uint32_t, WaitingList>::iterator iter;
    iter = _queue_map.find(ip_addr);
    if (iter != _queue_map.end()) {
        while (!iter->second.waiting_datagram.empty()) {
            InternetDatagram dgram = iter->second.waiting_datagram.front();
            iter->second.waiting_datagram.pop();
            send_helper(MAC_addr, dgram);
        }
    }
    _queue_map.erase(ip_addr);
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    optional<EthernetAddress> MAC_addr = get_EthernetAdress(next_hop_ip);
    if (MAC_addr.has_value()) {
        send_helper(MAC_addr.value(), dgram);
    } else {
        queue_helper(next_hop_ip, dgram);
    }    
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // filter frames whose dst is not this Network Interface
    optional<InternetDatagram> ret = nullopt;
    if (!valid_frame(frame)) return ret;
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(Buffer(frame.payload())) == ParseResult::NoError) {
            ret = dgram;
        }
    } else {
        ARPMessage arp;
        if (arp.parse(Buffer(frame.payload())) == ParseResult::NoError) {
            cache_mapping(arp.sender_ip_address, arp.sender_ethernet_address);
            clear_waitinglist(arp.sender_ip_address, arp.sender_ethernet_address);
            if (arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == _ip_address.ipv4_numeric()) 
                send_ARP_reply(arp.sender_ip_address, arp.sender_ethernet_address);
        }
    }
    return ret; 
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    map<uint32_t, NetworkInterface::WaitingList>::iterator iter1; 
    for (iter1 = _queue_map.begin(); iter1 != _queue_map.end(); iter1++) {
        iter1->second.time_since_last_ARP_request_send += ms_since_last_tick;
    }

    map<uint32_t, EthernetAddressEntry>::iterator iter2;
    queue<uint32_t> deleted_ips;
    for (iter2 = _cache.begin(); iter2 != _cache.end(); iter2++) {
        iter2->second.caching_time += ms_since_last_tick;
        if (iter2->second.caching_time >= NetworkInterface::MAX_CACHE_TIME) {
            deleted_ips.push(iter2->first);
        }
    }

    while (!deleted_ips.empty()) {
        uint32_t ip = deleted_ips.front();
        deleted_ips.pop();
        _cache.erase(ip);
    }
}
