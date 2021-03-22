#ifndef SPONGE_STRING_CONVERSIONS_HH
#define SPONGE_STRING_CONVERSIONS_HH

#include "wrapping_integers.hh"

#include <optional>
#include <string>
#include <utility>

// https://stackoverflow.com/questions/33399594/making-a-user-defined-class-stdto-stringable

namespace sponge_conversions {
using std::to_string;

std::string to_string(WrappingInt32 i) { return std::to_string(i.raw_value()); }

template <typename T>
std::string to_string(const std::optional<T> &v) {
    if (v.has_value()) {
        return "Some(" + to_string(v.value()) + ")";
    } else {
        return "None";
    }
}

template <typename T>
std::string as_string(T &&t) {
    return to_string(std::forward<T>(t));
}
}  // namespace sponge_conversions

template <typename T>
std::string to_string(T &&t) {
    return sponge_conversions::as_string(std::forward<T>(t));
}

#endif  // SPONGE_STRING_CONVERSIONS_HH
