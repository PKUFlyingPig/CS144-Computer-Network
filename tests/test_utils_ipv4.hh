#ifndef SPONGE_TESTS_TEST_UTILS_IPV4_HH
#define SPONGE_TESTS_TEST_UTILS_IPV4_HH

#include "ipv4_header.hh"

inline bool compare_ip_headers_nolen(const IPv4Header &h1, const IPv4Header &h2) {
    return h1.proto == h2.proto && h1.src == h2.src && h1.dst == h2.dst && h1.ver == h2.ver && h1.tos == h2.tos &&
           h1.id == h2.id && h1.df == h2.df && h1.mf == h2.mf && h1.offset == h2.offset && h1.ttl == h2.ttl;
}

inline bool compare_ip_headers(const IPv4Header &h1, const IPv4Header &h2) {
    return compare_ip_headers_nolen(h1, h2) && h1.hlen == h2.hlen && h1.len == h2.len && h1.cksum == h2.cksum;
}

#endif  // SPONGE_TESTS_TEST_UTILS_IPV4_HH
