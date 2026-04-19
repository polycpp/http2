#pragma once

/**
 * @file constants.hpp
 * @brief HTTP/2 constants matching Node.js `http2.constants`.
 *
 * Defines all HTTP/2 protocol constants including error codes, frame types,
 * settings IDs, status codes, pseudo-header names, method names, and standard
 * header names. Values match both RFC 9113 and Node.js's `http2.constants`.
 *
 * @see https://nodejs.org/api/http2.html#http2constants
 * @see https://www.rfc-editor.org/rfc/rfc9113
 */

#include <cstdint>
#include <string_view>

namespace polycpp {
namespace http2 {
namespace constants {

/** @defgroup http2_constants HTTP/2 Constants
 *  @brief All HTTP/2 protocol constants.
 *  @{ */

// ── Session Types ──────────────────────────────────────────────────────
constexpr int NGHTTP2_SESSION_SERVER = 0;
constexpr int NGHTTP2_SESSION_CLIENT = 1;

// ── Stream States ──────────────────────────────────────────────────────
constexpr int NGHTTP2_STREAM_STATE_IDLE = 0;
constexpr int NGHTTP2_STREAM_STATE_OPEN = 1;
constexpr int NGHTTP2_STREAM_STATE_RESERVED_LOCAL = 2;
constexpr int NGHTTP2_STREAM_STATE_RESERVED_REMOTE = 3;
constexpr int NGHTTP2_STREAM_STATE_HALF_CLOSED_LOCAL = 4;
constexpr int NGHTTP2_STREAM_STATE_HALF_CLOSED_REMOTE = 5;
constexpr int NGHTTP2_STREAM_STATE_CLOSED = 6;

// ── Error Codes (RFC 9113 Section 7) ──────────────────────────────────
constexpr uint32_t NGHTTP2_NO_ERROR = 0x0;
constexpr uint32_t NGHTTP2_PROTOCOL_ERROR = 0x1;
constexpr uint32_t NGHTTP2_INTERNAL_ERROR = 0x2;
constexpr uint32_t NGHTTP2_FLOW_CONTROL_ERROR = 0x3;
constexpr uint32_t NGHTTP2_SETTINGS_TIMEOUT = 0x4;
constexpr uint32_t NGHTTP2_STREAM_CLOSED = 0x5;
constexpr uint32_t NGHTTP2_FRAME_SIZE_ERROR = 0x6;
constexpr uint32_t NGHTTP2_REFUSED_STREAM = 0x7;
constexpr uint32_t NGHTTP2_CANCEL = 0x8;
constexpr uint32_t NGHTTP2_COMPRESSION_ERROR = 0x9;
constexpr uint32_t NGHTTP2_CONNECT_ERROR = 0xa;
constexpr uint32_t NGHTTP2_ENHANCE_YOUR_CALM = 0xb;
constexpr uint32_t NGHTTP2_INADEQUATE_SECURITY = 0xc;
constexpr uint32_t NGHTTP2_HTTP_1_1_REQUIRED = 0xd;

// ── Frame Types ────────────────────────────────────────────────────────
constexpr uint8_t NGHTTP2_DATA = 0x0;
constexpr uint8_t NGHTTP2_HEADERS = 0x1;
constexpr uint8_t NGHTTP2_PRIORITY = 0x2;
constexpr uint8_t NGHTTP2_RST_STREAM = 0x3;
constexpr uint8_t NGHTTP2_SETTINGS = 0x4;
constexpr uint8_t NGHTTP2_PUSH_PROMISE = 0x5;
constexpr uint8_t NGHTTP2_PING = 0x6;
constexpr uint8_t NGHTTP2_GOAWAY = 0x7;
constexpr uint8_t NGHTTP2_WINDOW_UPDATE = 0x8;
constexpr uint8_t NGHTTP2_CONTINUATION = 0x9;
// HTTP/2 extension frame types
constexpr uint8_t NGHTTP2_ALTSVC = 0xa;
constexpr uint8_t NGHTTP2_ORIGIN = 0xc;

// ── Frame Flags ────────────────────────────────────────────────────────
constexpr uint8_t NGHTTP2_FLAG_NONE = 0x0;
constexpr uint8_t NGHTTP2_FLAG_END_STREAM = 0x1;
constexpr uint8_t NGHTTP2_FLAG_END_HEADERS = 0x4;
constexpr uint8_t NGHTTP2_FLAG_ACK = 0x1;
constexpr uint8_t NGHTTP2_FLAG_PADDED = 0x8;
constexpr uint8_t NGHTTP2_FLAG_PRIORITY = 0x20;

// ── Settings IDs ───────────────────────────────────────────────────────
constexpr int32_t NGHTTP2_SETTINGS_HEADER_TABLE_SIZE = 0x1;
constexpr int32_t NGHTTP2_SETTINGS_ENABLE_PUSH = 0x2;
constexpr int32_t NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS = 0x3;
constexpr int32_t NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE = 0x4;
constexpr int32_t NGHTTP2_SETTINGS_MAX_FRAME_SIZE = 0x5;
constexpr int32_t NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE = 0x6;
constexpr int32_t NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL = 0x8;

// ── Default Settings Values ────────────────────────────────────────────
constexpr uint32_t DEFAULT_SETTINGS_HEADER_TABLE_SIZE = 4096;
constexpr bool     DEFAULT_SETTINGS_ENABLE_PUSH = true;
constexpr uint32_t DEFAULT_SETTINGS_INITIAL_WINDOW_SIZE = 65535;
constexpr uint32_t DEFAULT_SETTINGS_MAX_FRAME_SIZE = 16384;
constexpr uint32_t MAX_MAX_FRAME_SIZE = 16777215;
constexpr uint32_t MIN_MAX_FRAME_SIZE = 16384;
constexpr int32_t  MAX_INITIAL_WINDOW_SIZE = 2147483647;

// ── Padding Strategies ─────────────────────────────────────────────────
constexpr int PADDING_STRATEGY_NONE = 0;
constexpr int PADDING_STRATEGY_MAX = 1;
constexpr int PADDING_STRATEGY_CALLBACK = 2;

// ── HTTP Status Codes ──────────────────────────────────────────────────
// Match Node.js http2.constants.HTTP_STATUS_*
constexpr int HTTP_STATUS_CONTINUE = 100;
constexpr int HTTP_STATUS_SWITCHING_PROTOCOLS = 101;
constexpr int HTTP_STATUS_PROCESSING = 102;
constexpr int HTTP_STATUS_EARLY_HINTS = 103;
constexpr int HTTP_STATUS_OK = 200;
constexpr int HTTP_STATUS_CREATED = 201;
constexpr int HTTP_STATUS_ACCEPTED = 202;
constexpr int HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION = 203;
constexpr int HTTP_STATUS_NO_CONTENT = 204;
constexpr int HTTP_STATUS_RESET_CONTENT = 205;
constexpr int HTTP_STATUS_PARTIAL_CONTENT = 206;
constexpr int HTTP_STATUS_MULTI_STATUS = 207;
constexpr int HTTP_STATUS_ALREADY_REPORTED = 208;
constexpr int HTTP_STATUS_IM_USED = 226;
constexpr int HTTP_STATUS_MULTIPLE_CHOICES = 300;
constexpr int HTTP_STATUS_MOVED_PERMANENTLY = 301;
constexpr int HTTP_STATUS_FOUND = 302;
constexpr int HTTP_STATUS_SEE_OTHER = 303;
constexpr int HTTP_STATUS_NOT_MODIFIED = 304;
constexpr int HTTP_STATUS_USE_PROXY = 305;
constexpr int HTTP_STATUS_TEMPORARY_REDIRECT = 307;
constexpr int HTTP_STATUS_PERMANENT_REDIRECT = 308;
constexpr int HTTP_STATUS_BAD_REQUEST = 400;
constexpr int HTTP_STATUS_UNAUTHORIZED = 401;
constexpr int HTTP_STATUS_PAYMENT_REQUIRED = 402;
constexpr int HTTP_STATUS_FORBIDDEN = 403;
constexpr int HTTP_STATUS_NOT_FOUND = 404;
constexpr int HTTP_STATUS_METHOD_NOT_ALLOWED = 405;
constexpr int HTTP_STATUS_NOT_ACCEPTABLE = 406;
constexpr int HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED = 407;
constexpr int HTTP_STATUS_REQUEST_TIMEOUT = 408;
constexpr int HTTP_STATUS_CONFLICT = 409;
constexpr int HTTP_STATUS_GONE = 410;
constexpr int HTTP_STATUS_LENGTH_REQUIRED = 411;
constexpr int HTTP_STATUS_PRECONDITION_FAILED = 412;
constexpr int HTTP_STATUS_PAYLOAD_TOO_LARGE = 413;
constexpr int HTTP_STATUS_URI_TOO_LONG = 414;
constexpr int HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE = 415;
constexpr int HTTP_STATUS_RANGE_NOT_SATISFIABLE = 416;
constexpr int HTTP_STATUS_EXPECTATION_FAILED = 417;
constexpr int HTTP_STATUS_TEAPOT = 418;
constexpr int HTTP_STATUS_MISDIRECTED_REQUEST = 421;
constexpr int HTTP_STATUS_UNPROCESSABLE_ENTITY = 422;
constexpr int HTTP_STATUS_LOCKED = 423;
constexpr int HTTP_STATUS_FAILED_DEPENDENCY = 424;
constexpr int HTTP_STATUS_TOO_EARLY = 425;
constexpr int HTTP_STATUS_UPGRADE_REQUIRED = 426;
constexpr int HTTP_STATUS_PRECONDITION_REQUIRED = 428;
constexpr int HTTP_STATUS_TOO_MANY_REQUESTS = 429;
constexpr int HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE = 431;
constexpr int HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS = 451;
constexpr int HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;
constexpr int HTTP_STATUS_NOT_IMPLEMENTED = 501;
constexpr int HTTP_STATUS_BAD_GATEWAY = 502;
constexpr int HTTP_STATUS_SERVICE_UNAVAILABLE = 503;
constexpr int HTTP_STATUS_GATEWAY_TIMEOUT = 504;
constexpr int HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED = 505;
constexpr int HTTP_STATUS_VARIANT_ALSO_NEGOTIATES = 506;
constexpr int HTTP_STATUS_INSUFFICIENT_STORAGE = 507;
constexpr int HTTP_STATUS_LOOP_DETECTED = 508;
constexpr int HTTP_STATUS_BANDWIDTH_LIMIT_EXCEEDED = 509;
constexpr int HTTP_STATUS_NOT_EXTENDED = 510;
constexpr int HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED = 511;

// ── HTTP/2 Pseudo-Header Names ─────────────────────────────────────────
constexpr std::string_view HTTP2_HEADER_STATUS = ":status";
constexpr std::string_view HTTP2_HEADER_METHOD = ":method";
constexpr std::string_view HTTP2_HEADER_AUTHORITY = ":authority";
constexpr std::string_view HTTP2_HEADER_SCHEME = ":scheme";
constexpr std::string_view HTTP2_HEADER_PATH = ":path";
constexpr std::string_view HTTP2_HEADER_PROTOCOL = ":protocol";

// ── HTTP/2 Method Name Constants ───────────────────────────────────────
// Match Node.js http2.constants.HTTP2_METHOD_*
constexpr std::string_view HTTP2_METHOD_ACL = "ACL";
constexpr std::string_view HTTP2_METHOD_BASELINE_CONTROL = "BASELINE-CONTROL";
constexpr std::string_view HTTP2_METHOD_BIND = "BIND";
constexpr std::string_view HTTP2_METHOD_CHECKIN = "CHECKIN";
constexpr std::string_view HTTP2_METHOD_CHECKOUT = "CHECKOUT";
constexpr std::string_view HTTP2_METHOD_CONNECT = "CONNECT";
constexpr std::string_view HTTP2_METHOD_COPY = "COPY";
constexpr std::string_view HTTP2_METHOD_DELETE = "DELETE";
constexpr std::string_view HTTP2_METHOD_GET = "GET";
constexpr std::string_view HTTP2_METHOD_HEAD = "HEAD";
constexpr std::string_view HTTP2_METHOD_LABEL = "LABEL";
constexpr std::string_view HTTP2_METHOD_LINK = "LINK";
constexpr std::string_view HTTP2_METHOD_LOCK = "LOCK";
constexpr std::string_view HTTP2_METHOD_MERGE = "MERGE";
constexpr std::string_view HTTP2_METHOD_MKACTIVITY = "MKACTIVITY";
constexpr std::string_view HTTP2_METHOD_MKCALENDAR = "MKCALENDAR";
constexpr std::string_view HTTP2_METHOD_MKCOL = "MKCOL";
constexpr std::string_view HTTP2_METHOD_MKREDIRECTREF = "MKREDIRECTREF";
constexpr std::string_view HTTP2_METHOD_MKWORKSPACE = "MKWORKSPACE";
constexpr std::string_view HTTP2_METHOD_MOVE = "MOVE";
constexpr std::string_view HTTP2_METHOD_OPTIONS = "OPTIONS";
constexpr std::string_view HTTP2_METHOD_ORDERPATCH = "ORDERPATCH";
constexpr std::string_view HTTP2_METHOD_PATCH = "PATCH";
constexpr std::string_view HTTP2_METHOD_POST = "POST";
constexpr std::string_view HTTP2_METHOD_PRI = "PRI";
constexpr std::string_view HTTP2_METHOD_PROPFIND = "PROPFIND";
constexpr std::string_view HTTP2_METHOD_PROPPATCH = "PROPPATCH";
constexpr std::string_view HTTP2_METHOD_PUT = "PUT";
constexpr std::string_view HTTP2_METHOD_REBIND = "REBIND";
constexpr std::string_view HTTP2_METHOD_REPORT = "REPORT";
constexpr std::string_view HTTP2_METHOD_SEARCH = "SEARCH";
constexpr std::string_view HTTP2_METHOD_UNBIND = "UNBIND";
constexpr std::string_view HTTP2_METHOD_UNCHECKOUT = "UNCHECKOUT";
constexpr std::string_view HTTP2_METHOD_UNLINK = "UNLINK";
constexpr std::string_view HTTP2_METHOD_UNLOCK = "UNLOCK";
constexpr std::string_view HTTP2_METHOD_UPDATE = "UPDATE";
constexpr std::string_view HTTP2_METHOD_UPDATEREDIRECTREF = "UPDATEREDIRECTREF";
constexpr std::string_view HTTP2_METHOD_VERSION_CONTROL = "VERSION-CONTROL";

// ── Standard HTTP Header Name Constants ────────────────────────────────
// Match Node.js http2.constants.HTTP2_HEADER_*
constexpr std::string_view HTTP2_HEADER_ACCEPT = "accept";
constexpr std::string_view HTTP2_HEADER_ACCEPT_CHARSET = "accept-charset";
constexpr std::string_view HTTP2_HEADER_ACCEPT_ENCODING = "accept-encoding";
constexpr std::string_view HTTP2_HEADER_ACCEPT_LANGUAGE = "accept-language";
constexpr std::string_view HTTP2_HEADER_ACCEPT_RANGES = "accept-ranges";
constexpr std::string_view HTTP2_HEADER_ACCESS_CONTROL_ALLOW_CREDENTIALS = "access-control-allow-credentials";
constexpr std::string_view HTTP2_HEADER_ACCESS_CONTROL_ALLOW_HEADERS = "access-control-allow-headers";
constexpr std::string_view HTTP2_HEADER_ACCESS_CONTROL_ALLOW_METHODS = "access-control-allow-methods";
constexpr std::string_view HTTP2_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN = "access-control-allow-origin";
constexpr std::string_view HTTP2_HEADER_ACCESS_CONTROL_EXPOSE_HEADERS = "access-control-expose-headers";
constexpr std::string_view HTTP2_HEADER_ACCESS_CONTROL_MAX_AGE = "access-control-max-age";
constexpr std::string_view HTTP2_HEADER_ACCESS_CONTROL_REQUEST_HEADERS = "access-control-request-headers";
constexpr std::string_view HTTP2_HEADER_ACCESS_CONTROL_REQUEST_METHOD = "access-control-request-method";
constexpr std::string_view HTTP2_HEADER_AGE = "age";
constexpr std::string_view HTTP2_HEADER_ALLOW = "allow";
constexpr std::string_view HTTP2_HEADER_AUTHORIZATION = "authorization";
constexpr std::string_view HTTP2_HEADER_CACHE_CONTROL = "cache-control";
constexpr std::string_view HTTP2_HEADER_CONNECTION = "connection";
constexpr std::string_view HTTP2_HEADER_CONTENT_DISPOSITION = "content-disposition";
constexpr std::string_view HTTP2_HEADER_CONTENT_ENCODING = "content-encoding";
constexpr std::string_view HTTP2_HEADER_CONTENT_LANGUAGE = "content-language";
constexpr std::string_view HTTP2_HEADER_CONTENT_LENGTH = "content-length";
constexpr std::string_view HTTP2_HEADER_CONTENT_LOCATION = "content-location";
constexpr std::string_view HTTP2_HEADER_CONTENT_MD5 = "content-md5";
constexpr std::string_view HTTP2_HEADER_CONTENT_RANGE = "content-range";
constexpr std::string_view HTTP2_HEADER_CONTENT_TYPE = "content-type";
constexpr std::string_view HTTP2_HEADER_COOKIE = "cookie";
constexpr std::string_view HTTP2_HEADER_DATE = "date";
constexpr std::string_view HTTP2_HEADER_DNT = "dnt";
constexpr std::string_view HTTP2_HEADER_ETAG = "etag";
constexpr std::string_view HTTP2_HEADER_EXPECT = "expect";
constexpr std::string_view HTTP2_HEADER_EXPIRES = "expires";
constexpr std::string_view HTTP2_HEADER_FORWARDED = "forwarded";
constexpr std::string_view HTTP2_HEADER_FROM = "from";
constexpr std::string_view HTTP2_HEADER_HOST = "host";
constexpr std::string_view HTTP2_HEADER_IF_MATCH = "if-match";
constexpr std::string_view HTTP2_HEADER_IF_MODIFIED_SINCE = "if-modified-since";
constexpr std::string_view HTTP2_HEADER_IF_NONE_MATCH = "if-none-match";
constexpr std::string_view HTTP2_HEADER_IF_RANGE = "if-range";
constexpr std::string_view HTTP2_HEADER_IF_UNMODIFIED_SINCE = "if-unmodified-since";
constexpr std::string_view HTTP2_HEADER_LAST_MODIFIED = "last-modified";
constexpr std::string_view HTTP2_HEADER_LINK = "link";
constexpr std::string_view HTTP2_HEADER_LOCATION = "location";
constexpr std::string_view HTTP2_HEADER_MAX_FORWARDS = "max-forwards";
constexpr std::string_view HTTP2_HEADER_PREFER = "prefer";
constexpr std::string_view HTTP2_HEADER_PROXY_AUTHENTICATE = "proxy-authenticate";
constexpr std::string_view HTTP2_HEADER_PROXY_AUTHORIZATION = "proxy-authorization";
constexpr std::string_view HTTP2_HEADER_RANGE = "range";
constexpr std::string_view HTTP2_HEADER_REFERER = "referer";
constexpr std::string_view HTTP2_HEADER_REFRESH = "refresh";
constexpr std::string_view HTTP2_HEADER_RETRY_AFTER = "retry-after";
constexpr std::string_view HTTP2_HEADER_SERVER = "server";
constexpr std::string_view HTTP2_HEADER_SET_COOKIE = "set-cookie";
constexpr std::string_view HTTP2_HEADER_STRICT_TRANSPORT_SECURITY = "strict-transport-security";
constexpr std::string_view HTTP2_HEADER_TE = "te";
constexpr std::string_view HTTP2_HEADER_TK = "tk";
constexpr std::string_view HTTP2_HEADER_TRAILER = "trailer";
constexpr std::string_view HTTP2_HEADER_TRANSFER_ENCODING = "transfer-encoding";
constexpr std::string_view HTTP2_HEADER_UPGRADE = "upgrade";
constexpr std::string_view HTTP2_HEADER_USER_AGENT = "user-agent";
constexpr std::string_view HTTP2_HEADER_VARY = "vary";
constexpr std::string_view HTTP2_HEADER_VIA = "via";
constexpr std::string_view HTTP2_HEADER_WARNING = "warning";
constexpr std::string_view HTTP2_HEADER_WWW_AUTHENTICATE = "www-authenticate";
constexpr std::string_view HTTP2_HEADER_X_CONTENT_TYPE_OPTIONS = "x-content-type-options";
constexpr std::string_view HTTP2_HEADER_X_FRAME_OPTIONS = "x-frame-options";
constexpr std::string_view HTTP2_HEADER_KEEP_ALIVE = "keep-alive";
constexpr std::string_view HTTP2_HEADER_PROXY_CONNECTION = "proxy-connection";
constexpr std::string_view HTTP2_HEADER_X_XSS_PROTECTION = "x-xss-protection";
constexpr std::string_view HTTP2_HEADER_ALT_SVC = "alt-svc";
constexpr std::string_view HTTP2_HEADER_CONTENT_SECURITY_POLICY = "content-security-policy";
constexpr std::string_view HTTP2_HEADER_EARLY_DATA = "early-data";
constexpr std::string_view HTTP2_HEADER_EXPECT_CT = "expect-ct";
constexpr std::string_view HTTP2_HEADER_ORIGIN = "origin";
constexpr std::string_view HTTP2_HEADER_PURPOSE = "purpose";
constexpr std::string_view HTTP2_HEADER_TIMING_ALLOW_ORIGIN = "timing-allow-origin";
constexpr std::string_view HTTP2_HEADER_X_FORWARDED_FOR = "x-forwarded-for";

/** @} */ // end of http2_constants group

} // namespace constants
} // namespace http2
} // namespace polycpp
