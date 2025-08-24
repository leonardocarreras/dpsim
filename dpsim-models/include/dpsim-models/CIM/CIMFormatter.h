#pragma once
#include <string>
#include <fmt/format.h>
#include <String.hpp>   // defines CIMPP::String

// Helper(s) in your own namespace
namespace dpsim::cimpp {
inline std::string to_std(const CIMPP::String& s) {
    return static_cast<std::string>(s);  // uses CIMPP::String -> std::string
}
}

// Provide == / != for CIMPP::String (free functions in CIMPP namespace)
namespace CIMPP {
inline bool operator==(const String& a, const String& b) {
    return static_cast<std::string>(a) == static_cast<std::string>(b);
}
inline bool operator!=(const String& a, const String& b) {
    return !(a == b);
}
} // namespace CIMPP

// fmt / spdlog formatter for CIMPP::String
namespace fmt {
template <>
struct formatter<CIMPP::String> : formatter<std::string> {
  template <typename FormatContext>
  auto format(const CIMPP::String& s, FormatContext& ctx) const {
    return formatter<std::string>::format(static_cast<std::string>(s), ctx);
  }
};
} // namespace fmt
