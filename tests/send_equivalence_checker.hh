#ifndef SPONGE_LIBSPONGE_SEND_EQUIVALENCE_CHECKER_HH
#define SPONGE_LIBSPONGE_SEND_EQUIVALENCE_CHECKER_HH

#include "tcp_segment.hh"

#include <deque>
#include <optional>

class SendEquivalenceChecker {
    std::deque<TCPSegment> as{};
    std::deque<TCPSegment> bs{};

  public:
    void submit_a(TCPSegment &seg);
    void submit_b(TCPSegment &seg);
    void check_empty() const;
};

#endif  // SPONGE_LIBSPONGE_SEND_EQUIVALENCE_CHECKER_HH
