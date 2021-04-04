#include "send_equivalence_checker.hh"

#include "tcp_segment.hh"

#include <algorithm>
#include <exception>
#include <iostream>
#include <sstream>

using namespace std;

void check_equivalent_segments(const TCPSegment &a, const TCPSegment &b) {
    if (not(a.header() == b.header())) {
        cerr << a.header().to_string() << endl;
        cerr << b.header().to_string() << endl;
        stringstream s{};
        s << a.header().summary() << " with " << a.payload().size() << " bytes != " << b.header().summary() << " with "
          << b.payload().size() << " bytes";
        throw runtime_error(s.str());
    }
    if (a.payload().str() != b.payload().str()) {
        stringstream s{};
        s << a.header().summary() << " with " << a.payload().size() << " bytes != " << b.header().summary() << " with "
          << b.payload().size() << " bytes";
        throw runtime_error("unequal payloads: " + s.str());
    }
}

void SendEquivalenceChecker::submit_a(TCPSegment &seg) {
    if (not bs.empty()) {
        check_equivalent_segments(seg, bs.front());
        bs.pop_front();
    } else {
        TCPSegment cp;
        cp.parse(seg.serialize());
        as.emplace_back(move(cp));
    }
}

void SendEquivalenceChecker::submit_b(TCPSegment &seg) {
    if (not as.empty()) {
        check_equivalent_segments(as.front(), seg);
        as.pop_front();
    } else {
        TCPSegment cp;
        cp.parse(seg.serialize());
        bs.emplace_back(move(cp));
    }
}

void SendEquivalenceChecker::check_empty() const {
    if (not as.empty()) {
        stringstream s{};
        s << as.size() << " unmatched packets from standard" << endl;
        for (const TCPSegment &a : as) {
            s << "  - " << a.header().summary() << " with " << a.payload().size() << " bytes\n";
        }
        throw runtime_error(s.str());
    }
    if (not bs.empty()) {
        stringstream s{};
        s << bs.size() << " unmatched packets from factored" << endl;
        for (const TCPSegment &a : bs) {
            s << "  - " << a.header().summary() << " with " << a.payload().size() << " bytes\n";
        }
        throw runtime_error(s.str());
    }
}
