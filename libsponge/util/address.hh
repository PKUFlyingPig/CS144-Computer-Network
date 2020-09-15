#ifndef SPONGE_LIBSPONGE_ADDRESS_HH
#define SPONGE_LIBSPONGE_ADDRESS_HH

#include <cstddef>
#include <cstdint>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <utility>

//! Wrapper around [IPv4 addresses](@ref man7::ip) and DNS operations.
class Address {
  public:
    //! \brief Wrapper around [sockaddr_storage](@ref man7::socket).
    //! \details A `sockaddr_storage` is enough space to store any socket address (IPv4 or IPv6).
    class Raw {
      public:
        sockaddr_storage storage{};  //!< The wrapped struct itself.
        operator sockaddr *();
        operator const sockaddr *() const;
    };

  private:
    socklen_t _size;  //!< Size of the wrapped address.
    Raw _address{};   //!< A wrapped [sockaddr_storage](@ref man7::socket) containing the address.

    //! Constructor from ip/host, service/port, and hints to the resolver.
    Address(const std::string &node, const std::string &service, const addrinfo &hints);

  public:
    //! Construct by resolving a hostname and servicename.
    Address(const std::string &hostname, const std::string &service);

    //! Construct from dotted-quad string ("18.243.0.1") and numeric port.
    Address(const std::string &ip, const std::uint16_t port = 0);

    //! Construct from a [sockaddr *](@ref man7::socket).
    Address(const sockaddr *addr, const std::size_t size);

    //! Equality comparison.
    bool operator==(const Address &other) const;
    bool operator!=(const Address &other) const { return not operator==(other); }

    //! \name Conversions
    //!@{

    //! Dotted-quad IP address string ("18.243.0.1") and numeric port.
    std::pair<std::string, uint16_t> ip_port() const;
    //! Dotted-quad IP address string ("18.243.0.1").
    std::string ip() const { return ip_port().first; }
    //! Numeric port (host byte order).
    uint16_t port() const { return ip_port().second; }
    //! Numeric IP address as an integer (i.e., in [host byte order](\ref man3::byteorder)).
    uint32_t ipv4_numeric() const;
    //! Create an Address from a 32-bit raw numeric IP address
    static Address from_ipv4_numeric(const uint32_t ip_address);
    //! Human-readable string, e.g., "8.8.8.8:53".
    std::string to_string() const;
    //!@}

    //! \name Low-level operations
    //!@{

    //! Size of the underlying address storage.
    socklen_t size() const { return _size; }
    //! Const pointer to the underlying socket address storage.
    operator const sockaddr *() const { return _address; }
    //!@}
};

//! \class Address
//! For example, you can do DNS lookups:
//!
//! \include address_example_1.cc
//!
//! or you can specify an IP address and port number:
//!
//! \include address_example_2.cc
//!
//! Once you have an address, you can convert it to other useful representations, e.g.,
//!
//! \include address_example_3.cc

#endif  // SPONGE_LIBSPONGE_ADDRESS_HH
