#pragma once
#include <polycpp/config.hpp>

/**
 * @file util.hpp
 * @brief HTTP/2 header validation and utility functions.
 *
 * Provides utilities for pseudo-header validation, connection-specific header
 * detection, single-value header checks, and header name/value validation
 * per RFC 9113 and RFC 9110.
 *
 * @see https://www.rfc-editor.org/rfc/rfc9113#section-8.2
 */

#include <string>
#include <string_view>
#include <unordered_set>
#include <algorithm>

namespace polycpp {
namespace http2 {
namespace util {

/** @defgroup http2_util HTTP/2 Utilities
 *  @brief Header validation and utility functions.
 *  @{ */

// ── Pseudo-header validation ───────────────────────────────────────────

/**
 * @brief Check if a header name is a pseudo-header (starts with ':').
 * @param name Header name to check.
 * @return true if name starts with ':'.
 */
inline bool isPseudoHeader(std::string_view name) noexcept {
    return !name.empty() && name[0] == ':';
}

/**
 * @brief Check if a header name is a valid request pseudo-header.
 * @param name Header name to check.
 * @return true if name is one of :method, :path, :scheme, :authority, :protocol.
 */
inline bool isRequestPseudoHeader(std::string_view name) noexcept {
    return name == ":method" || name == ":path" ||
           name == ":scheme" || name == ":authority" ||
           name == ":protocol";
}

/**
 * @brief Check if a header name is a valid response pseudo-header.
 * @param name Header name to check.
 * @return true if name is ":status".
 */
inline bool isResponsePseudoHeader(std::string_view name) noexcept {
    return name == ":status";
}

// ── Connection-specific headers forbidden in HTTP/2 ────────────────────

/**
 * @brief Check if a header is a connection-specific header forbidden in HTTP/2.
 *
 * RFC 9113 Section 8.2.2 forbids: connection, keep-alive, proxy-connection,
 * transfer-encoding, upgrade.
 *
 * @param name Header name to check (case-insensitive).
 * @return true if the header is forbidden in HTTP/2.
 */
inline bool isConnectionHeader(std::string_view name) {
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower == "connection" || lower == "keep-alive" ||
           lower == "proxy-connection" || lower == "transfer-encoding" ||
           lower == "upgrade";
}

// ── Single-value headers (cannot be repeated) ──────────────────────────

/**
 * @brief Check if a header is a single-value header that cannot be repeated.
 *
 * Reference: Node.js internal/http2/util.js kSingleValueHeaders.
 *
 * @param name Header name to check.
 * @return true if the header must have a single value.
 */
inline bool isSingleValueHeader(std::string_view name) {
    // Using a static set for O(1) lookup
    static const std::unordered_set<std::string_view> single = {
        ":status", ":method", ":authority", ":scheme", ":path", ":protocol",
        "access-control-allow-credentials",
        "access-control-max-age",
        "access-control-request-method",
        "age", "authorization",
        "content-encoding", "content-language", "content-length",
        "content-location", "content-md5", "content-range",
        "content-type",
        "date", "dnt", "etag", "expires",
        "from", "host",
        "if-match", "if-modified-since", "if-none-match",
        "if-range", "if-unmodified-since",
        "last-modified", "location",
        "max-forwards",
        "proxy-authorization",
        "range", "referer", "retry-after",
        "tk",
        "upgrade-insecure-requests",
        "user-agent",
        "x-content-type-options"
    };
    return single.count(name) > 0;
}

// ── TE header validation ───────────────────────────────────────────────

/**
 * @brief Check if a TE header value is valid for HTTP/2.
 *
 * In HTTP/2, the TE header is only valid with value "trailers".
 *
 * @param value TE header value.
 * @return true if value is "trailers".
 */
inline bool isValidTEHeader(std::string_view value) noexcept {
    return value == "trailers";
}

// ── Header name conversion ─────────────────────────────────────────────

/**
 * @brief Convert header name to lowercase (HTTP/2 requires lowercase).
 * @param name Header name.
 * @return Lowercase copy of the name.
 */
inline std::string toLowerHeaderName(std::string_view name) {
    std::string result(name);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// ── Validate header name characters (RFC 9110 field-name) ──────────────

/**
 * @brief Validate header name characters per RFC 9110 token rules.
 *
 * @param name Header name to validate.
 * @return true if the name contains only valid token characters.
 */
inline bool isValidHeaderName(std::string_view name) {
    if (name.empty()) return false;
    for (size_t i = 0; i < name.size(); ++i) {
        char c = name[i];
        // Valid token chars per RFC 9110
        if (c <= 0x20 || c >= 0x7f) return false;
        switch (c) {
            case '(': case ')': case '<': case '>': case '@':
            case ',': case ';': case '\\': case '"':
            case '/': case '[': case ']': case '?': case '=':
            case '{': case '}':
                return false;
            case ':':
                // Pseudo-headers start with ':' at position 0 — that's valid
                if (i == 0) continue;
                return false;
            default:
                break;
        }
    }
    return true;
}

// ── Validate header value (no \r\n\0) ──────────────────────────────────

/**
 * @brief Validate header value characters.
 *
 * Header values must not contain CR, LF, or NUL characters.
 *
 * @param value Header value to validate.
 * @return true if value contains no forbidden characters.
 */
inline bool isValidHeaderValue(std::string_view value) noexcept {
    for (char c : value) {
        if (c == '\r' || c == '\n' || c == '\0') return false;
    }
    return true;
}

/** @} */ // end of http2_util group

} // namespace util
} // namespace http2
} // namespace polycpp
