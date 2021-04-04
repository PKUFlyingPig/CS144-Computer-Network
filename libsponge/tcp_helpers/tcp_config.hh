#ifndef SPONGE_LIBSPONGE_TCP_CONFIG_HH
#define SPONGE_LIBSPONGE_TCP_CONFIG_HH

#include "address.hh"
#include "wrapping_integers.hh"

#include <cstddef>
#include <cstdint>
#include <optional>

//! Config for TCP sender and receiver
class TCPConfig {
  public:
    static constexpr size_t DEFAULT_CAPACITY = 64000;  //!< Default capacity
    static constexpr size_t MAX_PAYLOAD_SIZE = 1452;   //!< Max TCP payload that fits in either IPv4 or UDP datagram
    static constexpr uint16_t TIMEOUT_DFLT = 1000;     //!< Default re-transmit timeout is 1 second
    static constexpr unsigned MAX_RETX_ATTEMPTS = 8;   //!< Maximum re-transmit attempts before giving up

    uint16_t rt_timeout = TIMEOUT_DFLT;       //!< Initial value of the retransmission timeout, in milliseconds
    size_t recv_capacity = DEFAULT_CAPACITY;  //!< Receive capacity, in bytes
    size_t send_capacity = DEFAULT_CAPACITY;  //!< Sender capacity, in bytes
    std::optional<WrappingInt32> fixed_isn{};
};

//! Config for classes derived from FdAdapter
class FdAdapterConfig {
  public:
    Address source{"0", 0};       //!< Source address and port
    Address destination{"0", 0};  //!< Destination address and port

    uint16_t loss_rate_dn = 0;  //!< Downlink loss rate (for LossyFdAdapter)
    uint16_t loss_rate_up = 0;  //!< Uplink loss rate (for LossyFdAdapter)
};

#endif  // SPONGE_LIBSPONGE_TCP_CONFIG_HH
