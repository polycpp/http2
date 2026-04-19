#pragma once

/**
 * @file http2_detail_decl.hpp
 * @brief Internal declarations for HTTP/2 detail functions.
 */

#include <cstdint>
#include <string>

namespace polycpp {
namespace http2 {
namespace detail {

/// @internal Parses an HTTP/2 authority string into host, port, and scheme.
void parseAuthority(const std::string& authority,
                    std::string& host, uint16_t& port, std::string& scheme);

} // namespace detail
} // namespace http2
} // namespace polycpp
