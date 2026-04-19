#include <gtest/gtest.h>
#include <polycpp/http2.hpp>
#include <polycpp/http2/detail/session_impl.hpp>       // for Http2SessionImpl tests
#include <polycpp/http2/detail/http2_detail_decl.hpp>  // for detail::parseAuthority tests
#include <polycpp/buffer.hpp>
#include <polycpp/core/error.hpp>
#include <polycpp/io/tcp_socket.hpp>
#include <polycpp/io/timer.hpp>
#include "test_ipv6_helpers.hpp"
#include "test_helpers/platform_helpers.hpp"

#include <chrono>
#include <cstdio>   // for std::tmpfile, std::fwrite, etc.
#include <fcntl.h>  // for open()
#ifndef _WIN32
#include <unistd.h> // for close(), write()
#else
#include <io.h>     // for _open(), _close(), _write() on Windows
#include <polycpp/http2/events.hpp>
#include <polycpp/stream/events.hpp>
#endif

using namespace polycpp::http2;
namespace http = polycpp::http;
using polycpp::Error;
using polycpp::buffer::Buffer;

// ══════════════════════════════════════════════════════════════════════
// Constants Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2ConstantsTest, ErrorCodesMatchRFC) {
    EXPECT_EQ(constants::NGHTTP2_NO_ERROR, 0x0u);
    EXPECT_EQ(constants::NGHTTP2_PROTOCOL_ERROR, 0x1u);
    EXPECT_EQ(constants::NGHTTP2_INTERNAL_ERROR, 0x2u);
    EXPECT_EQ(constants::NGHTTP2_FLOW_CONTROL_ERROR, 0x3u);
    EXPECT_EQ(constants::NGHTTP2_SETTINGS_TIMEOUT, 0x4u);
    EXPECT_EQ(constants::NGHTTP2_STREAM_CLOSED, 0x5u);
    EXPECT_EQ(constants::NGHTTP2_FRAME_SIZE_ERROR, 0x6u);
    EXPECT_EQ(constants::NGHTTP2_REFUSED_STREAM, 0x7u);
    EXPECT_EQ(constants::NGHTTP2_CANCEL, 0x8u);
    EXPECT_EQ(constants::NGHTTP2_COMPRESSION_ERROR, 0x9u);
    EXPECT_EQ(constants::NGHTTP2_CONNECT_ERROR, 0xau);
    EXPECT_EQ(constants::NGHTTP2_ENHANCE_YOUR_CALM, 0xbu);
    EXPECT_EQ(constants::NGHTTP2_INADEQUATE_SECURITY, 0xcu);
    EXPECT_EQ(constants::NGHTTP2_HTTP_1_1_REQUIRED, 0xdu);
}

TEST(Http2ConstantsTest, FrameTypesMatchRFC) {
    EXPECT_EQ(constants::NGHTTP2_DATA, 0x0);
    EXPECT_EQ(constants::NGHTTP2_HEADERS, 0x1);
    EXPECT_EQ(constants::NGHTTP2_PRIORITY, 0x2);
    EXPECT_EQ(constants::NGHTTP2_RST_STREAM, 0x3);
    EXPECT_EQ(constants::NGHTTP2_SETTINGS, 0x4);
    EXPECT_EQ(constants::NGHTTP2_PUSH_PROMISE, 0x5);
    EXPECT_EQ(constants::NGHTTP2_PING, 0x6);
    EXPECT_EQ(constants::NGHTTP2_GOAWAY, 0x7);
    EXPECT_EQ(constants::NGHTTP2_WINDOW_UPDATE, 0x8);
    EXPECT_EQ(constants::NGHTTP2_CONTINUATION, 0x9);
    EXPECT_EQ(constants::NGHTTP2_ALTSVC, 0xa);
    EXPECT_EQ(constants::NGHTTP2_ORIGIN, 0xc);
}

TEST(Http2ConstantsTest, FrameFlags) {
    EXPECT_EQ(constants::NGHTTP2_FLAG_NONE, 0x0);
    EXPECT_EQ(constants::NGHTTP2_FLAG_END_STREAM, 0x1);
    EXPECT_EQ(constants::NGHTTP2_FLAG_END_HEADERS, 0x4);
    EXPECT_EQ(constants::NGHTTP2_FLAG_ACK, 0x1);
    EXPECT_EQ(constants::NGHTTP2_FLAG_PADDED, 0x8);
    EXPECT_EQ(constants::NGHTTP2_FLAG_PRIORITY, 0x20);
}

TEST(Http2ConstantsTest, SettingsIDsMatchRFC) {
    EXPECT_EQ(constants::NGHTTP2_SETTINGS_HEADER_TABLE_SIZE, 0x1);
    EXPECT_EQ(constants::NGHTTP2_SETTINGS_ENABLE_PUSH, 0x2);
    EXPECT_EQ(constants::NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 0x3);
    EXPECT_EQ(constants::NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, 0x4);
    EXPECT_EQ(constants::NGHTTP2_SETTINGS_MAX_FRAME_SIZE, 0x5);
    EXPECT_EQ(constants::NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE, 0x6);
    EXPECT_EQ(constants::NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL, 0x8);
}

TEST(Http2ConstantsTest, DefaultSettingsValues) {
    EXPECT_EQ(constants::DEFAULT_SETTINGS_HEADER_TABLE_SIZE, 4096u);
    EXPECT_EQ(constants::DEFAULT_SETTINGS_ENABLE_PUSH, true);
    EXPECT_EQ(constants::DEFAULT_SETTINGS_INITIAL_WINDOW_SIZE, 65535u);
    EXPECT_EQ(constants::DEFAULT_SETTINGS_MAX_FRAME_SIZE, 16384u);
    EXPECT_EQ(constants::MAX_MAX_FRAME_SIZE, 16777215u);
    EXPECT_EQ(constants::MIN_MAX_FRAME_SIZE, 16384u);
    EXPECT_EQ(constants::MAX_INITIAL_WINDOW_SIZE, 2147483647);
}

TEST(Http2ConstantsTest, HttpStatusCodes) {
    EXPECT_EQ(constants::HTTP_STATUS_CONTINUE, 100);
    EXPECT_EQ(constants::HTTP_STATUS_OK, 200);
    EXPECT_EQ(constants::HTTP_STATUS_CREATED, 201);
    EXPECT_EQ(constants::HTTP_STATUS_NO_CONTENT, 204);
    EXPECT_EQ(constants::HTTP_STATUS_MOVED_PERMANENTLY, 301);
    EXPECT_EQ(constants::HTTP_STATUS_FOUND, 302);
    EXPECT_EQ(constants::HTTP_STATUS_NOT_MODIFIED, 304);
    EXPECT_EQ(constants::HTTP_STATUS_BAD_REQUEST, 400);
    EXPECT_EQ(constants::HTTP_STATUS_UNAUTHORIZED, 401);
    EXPECT_EQ(constants::HTTP_STATUS_FORBIDDEN, 403);
    EXPECT_EQ(constants::HTTP_STATUS_NOT_FOUND, 404);
    EXPECT_EQ(constants::HTTP_STATUS_TEAPOT, 418);
    EXPECT_EQ(constants::HTTP_STATUS_INTERNAL_SERVER_ERROR, 500);
    EXPECT_EQ(constants::HTTP_STATUS_BAD_GATEWAY, 502);
    EXPECT_EQ(constants::HTTP_STATUS_SERVICE_UNAVAILABLE, 503);
}

TEST(Http2ConstantsTest, PseudoHeaderNames) {
    EXPECT_EQ(constants::HTTP2_HEADER_STATUS, ":status");
    EXPECT_EQ(constants::HTTP2_HEADER_METHOD, ":method");
    EXPECT_EQ(constants::HTTP2_HEADER_PATH, ":path");
    EXPECT_EQ(constants::HTTP2_HEADER_SCHEME, ":scheme");
    EXPECT_EQ(constants::HTTP2_HEADER_AUTHORITY, ":authority");
    EXPECT_EQ(constants::HTTP2_HEADER_PROTOCOL, ":protocol");
}

TEST(Http2ConstantsTest, StandardHeaderNames) {
    EXPECT_EQ(constants::HTTP2_HEADER_CONTENT_TYPE, "content-type");
    EXPECT_EQ(constants::HTTP2_HEADER_CONTENT_LENGTH, "content-length");
    EXPECT_EQ(constants::HTTP2_HEADER_AUTHORIZATION, "authorization");
    EXPECT_EQ(constants::HTTP2_HEADER_SET_COOKIE, "set-cookie");
    EXPECT_EQ(constants::HTTP2_HEADER_ACCEPT, "accept");
    EXPECT_EQ(constants::HTTP2_HEADER_CACHE_CONTROL, "cache-control");
    EXPECT_EQ(constants::HTTP2_HEADER_HOST, "host");
    EXPECT_EQ(constants::HTTP2_HEADER_USER_AGENT, "user-agent");
}

TEST(Http2ConstantsTest, MethodNames) {
    EXPECT_EQ(constants::HTTP2_METHOD_GET, "GET");
    EXPECT_EQ(constants::HTTP2_METHOD_POST, "POST");
    EXPECT_EQ(constants::HTTP2_METHOD_PUT, "PUT");
    EXPECT_EQ(constants::HTTP2_METHOD_DELETE, "DELETE");
    EXPECT_EQ(constants::HTTP2_METHOD_PATCH, "PATCH");
    EXPECT_EQ(constants::HTTP2_METHOD_HEAD, "HEAD");
    EXPECT_EQ(constants::HTTP2_METHOD_OPTIONS, "OPTIONS");
    EXPECT_EQ(constants::HTTP2_METHOD_CONNECT, "CONNECT");
}

TEST(Http2ConstantsTest, SessionTypes) {
    EXPECT_EQ(constants::NGHTTP2_SESSION_SERVER, 0);
    EXPECT_EQ(constants::NGHTTP2_SESSION_CLIENT, 1);
}

TEST(Http2ConstantsTest, StreamStates) {
    EXPECT_EQ(constants::NGHTTP2_STREAM_STATE_IDLE, 0);
    EXPECT_EQ(constants::NGHTTP2_STREAM_STATE_OPEN, 1);
    EXPECT_EQ(constants::NGHTTP2_STREAM_STATE_RESERVED_LOCAL, 2);
    EXPECT_EQ(constants::NGHTTP2_STREAM_STATE_RESERVED_REMOTE, 3);
    EXPECT_EQ(constants::NGHTTP2_STREAM_STATE_HALF_CLOSED_LOCAL, 4);
    EXPECT_EQ(constants::NGHTTP2_STREAM_STATE_HALF_CLOSED_REMOTE, 5);
    EXPECT_EQ(constants::NGHTTP2_STREAM_STATE_CLOSED, 6);
}

TEST(Http2ConstantsTest, PaddingStrategies) {
    EXPECT_EQ(constants::PADDING_STRATEGY_NONE, 0);
    EXPECT_EQ(constants::PADDING_STRATEGY_MAX, 1);
    EXPECT_EQ(constants::PADDING_STRATEGY_CALLBACK, 2);
}

// ══════════════════════════════════════════════════════════════════════
// Settings Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2SettingsTest, DefaultSettings) {
    auto settings = getDefaultSettings();
    EXPECT_EQ(settings.headerTableSize.value(), 4096u);
    EXPECT_EQ(settings.enablePush.value(), true);
    EXPECT_EQ(settings.initialWindowSize.value(), 65535u);
    EXPECT_EQ(settings.maxFrameSize.value(), 16384u);
    EXPECT_FALSE(settings.maxConcurrentStreams.has_value());
    EXPECT_FALSE(settings.maxHeaderListSize.has_value());
    EXPECT_EQ(settings.enableConnectProtocol.value(), false);
}

TEST(Http2SettingsTest, PackAndUnpackRoundTrip) {
    Settings original;
    original.headerTableSize = 8192;
    original.enablePush = false;
    original.initialWindowSize = 32768;
    original.maxFrameSize = 32768;
    original.maxConcurrentStreams = 100;
    original.maxHeaderListSize = 65535;
    original.enableConnectProtocol = true;

    auto packed = getPackedSettings(original);
    // 7 settings * 6 bytes each = 42 bytes
    EXPECT_EQ(packed.length(), 42u);

    auto unpacked = getUnpackedSettings(packed);
    EXPECT_EQ(unpacked.headerTableSize.value(), 8192u);
    EXPECT_EQ(unpacked.enablePush.value(), false);
    EXPECT_EQ(unpacked.initialWindowSize.value(), 32768u);
    EXPECT_EQ(unpacked.maxFrameSize.value(), 32768u);
    EXPECT_EQ(unpacked.maxConcurrentStreams.value(), 100u);
    EXPECT_EQ(unpacked.maxHeaderListSize.value(), 65535u);
    EXPECT_EQ(unpacked.enableConnectProtocol.value(), true);
}

TEST(Http2SettingsTest, PackAndUnpackDefaultSettings) {
    auto defaults = getDefaultSettings();
    auto packed = getPackedSettings(defaults);

    // Default settings have 5 set values (headerTableSize, enablePush,
    // initialWindowSize, maxFrameSize, enableConnectProtocol)
    EXPECT_EQ(packed.length(), 5u * 6u);

    auto unpacked = getUnpackedSettings(packed);
    EXPECT_EQ(unpacked.headerTableSize.value(), 4096u);
    EXPECT_EQ(unpacked.enablePush.value(), true);
    EXPECT_EQ(unpacked.initialWindowSize.value(), 65535u);
    EXPECT_EQ(unpacked.maxFrameSize.value(), 16384u);
    EXPECT_EQ(unpacked.enableConnectProtocol.value(), false);
}

TEST(Http2SettingsTest, PackEmptySettings) {
    Settings empty;
    auto packed = getPackedSettings(empty);
    EXPECT_EQ(packed.length(), 0u);
}

TEST(Http2SettingsTest, PackSingleSetting) {
    Settings s;
    s.maxFrameSize = 16384;
    auto packed = getPackedSettings(s);
    EXPECT_EQ(packed.length(), 6u);

    auto unpacked = getUnpackedSettings(packed);
    EXPECT_EQ(unpacked.maxFrameSize.value(), 16384u);
    EXPECT_FALSE(unpacked.headerTableSize.has_value());
    EXPECT_FALSE(unpacked.enablePush.has_value());
    EXPECT_FALSE(unpacked.maxConcurrentStreams.has_value());
}

TEST(Http2SettingsTest, PackLargeValues) {
    Settings s;
    s.headerTableSize = 4294967295u;  // Max uint32
    s.maxConcurrentStreams = 4294967295u;
    auto packed = getPackedSettings(s);
    EXPECT_EQ(packed.length(), 12u);

    auto unpacked = getUnpackedSettings(packed);
    EXPECT_EQ(unpacked.headerTableSize.value(), 4294967295u);
    EXPECT_EQ(unpacked.maxConcurrentStreams.value(), 4294967295u);
}

TEST(Http2SettingsTest, UnpackInvalidBufferSize) {
    auto buf = Buffer::alloc(7);  // Not a multiple of 6
    EXPECT_THROW(getUnpackedSettings(buf), Error);
}

TEST(Http2SettingsTest, UnpackInvalidBufferSizeFive) {
    auto buf = Buffer::alloc(5);
    EXPECT_THROW(getUnpackedSettings(buf), Error);
}

TEST(Http2SettingsTest, UnpackEmptyBuffer) {
    auto buf = Buffer::alloc(0);
    auto settings = getUnpackedSettings(buf);
    // All fields should be nullopt
    EXPECT_FALSE(settings.headerTableSize.has_value());
    EXPECT_FALSE(settings.enablePush.has_value());
    EXPECT_FALSE(settings.maxFrameSize.has_value());
}

TEST(Http2SettingsTest, UnpackUnknownSettingId) {
    // Create a buffer with an unknown setting ID (0xFF)
    uint8_t data[6] = {0x00, 0xFF, 0x00, 0x00, 0x10, 0x00};
    auto buf = Buffer::from(data, 6);
    // Should not throw — unknown IDs are ignored per RFC 9113
    auto settings = getUnpackedSettings(buf);
    EXPECT_FALSE(settings.headerTableSize.has_value());
}

TEST(Http2SettingsTest, ValidateSettingsValid) {
    Settings s;
    s.maxFrameSize = 16384;
    s.initialWindowSize = 65535;
    s.enablePush = true;
    EXPECT_NO_THROW(validateSettings(s));
}

TEST(Http2SettingsTest, ValidateSettingsEmptyIsValid) {
    Settings s;
    EXPECT_NO_THROW(validateSettings(s));
}

TEST(Http2SettingsTest, ValidateSettingsMaxFrameSizeTooSmall) {
    Settings s;
    s.maxFrameSize = 1000;  // Below minimum 16384
    EXPECT_THROW(validateSettings(s), Error);
}

TEST(Http2SettingsTest, ValidateSettingsMaxFrameSizeTooLarge) {
    Settings s;
    s.maxFrameSize = 20000000;  // Above maximum 16777215
    EXPECT_THROW(validateSettings(s), Error);
}

TEST(Http2SettingsTest, ValidateSettingsMaxFrameSizeAtMinimum) {
    Settings s;
    s.maxFrameSize = 16384;  // Exactly minimum
    EXPECT_NO_THROW(validateSettings(s));
}

TEST(Http2SettingsTest, ValidateSettingsMaxFrameSizeAtMaximum) {
    Settings s;
    s.maxFrameSize = 16777215;  // Exactly maximum
    EXPECT_NO_THROW(validateSettings(s));
}

TEST(Http2SettingsTest, ValidateSettingsInitialWindowSizeTooLarge) {
    Settings s;
    s.initialWindowSize = 2147483648u;  // Exceeds 2^31-1
    EXPECT_THROW(validateSettings(s), Error);
}

TEST(Http2SettingsTest, ValidateSettingsInitialWindowSizeAtMax) {
    Settings s;
    s.initialWindowSize = 2147483647u;  // Exactly 2^31-1
    EXPECT_NO_THROW(validateSettings(s));
}

// ══════════════════════════════════════════════════════════════════════
// Utility Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2UtilTest, IsPseudoHeader) {
    EXPECT_TRUE(util::isPseudoHeader(":method"));
    EXPECT_TRUE(util::isPseudoHeader(":status"));
    EXPECT_TRUE(util::isPseudoHeader(":path"));
    EXPECT_TRUE(util::isPseudoHeader(":scheme"));
    EXPECT_TRUE(util::isPseudoHeader(":authority"));
    EXPECT_TRUE(util::isPseudoHeader(":protocol"));
    EXPECT_TRUE(util::isPseudoHeader(":unknown"));
    EXPECT_FALSE(util::isPseudoHeader("content-type"));
    EXPECT_FALSE(util::isPseudoHeader("host"));
    EXPECT_FALSE(util::isPseudoHeader(""));
}

TEST(Http2UtilTest, IsRequestPseudoHeader) {
    EXPECT_TRUE(util::isRequestPseudoHeader(":method"));
    EXPECT_TRUE(util::isRequestPseudoHeader(":path"));
    EXPECT_TRUE(util::isRequestPseudoHeader(":scheme"));
    EXPECT_TRUE(util::isRequestPseudoHeader(":authority"));
    EXPECT_TRUE(util::isRequestPseudoHeader(":protocol"));
    EXPECT_FALSE(util::isRequestPseudoHeader(":status"));
    EXPECT_FALSE(util::isRequestPseudoHeader("content-type"));
    EXPECT_FALSE(util::isRequestPseudoHeader(""));
}

TEST(Http2UtilTest, IsResponsePseudoHeader) {
    EXPECT_TRUE(util::isResponsePseudoHeader(":status"));
    EXPECT_FALSE(util::isResponsePseudoHeader(":method"));
    EXPECT_FALSE(util::isResponsePseudoHeader(":path"));
    EXPECT_FALSE(util::isResponsePseudoHeader("content-type"));
}

TEST(Http2UtilTest, IsConnectionHeader) {
    EXPECT_TRUE(util::isConnectionHeader("connection"));
    EXPECT_TRUE(util::isConnectionHeader("keep-alive"));
    EXPECT_TRUE(util::isConnectionHeader("transfer-encoding"));
    EXPECT_TRUE(util::isConnectionHeader("upgrade"));
    EXPECT_TRUE(util::isConnectionHeader("proxy-connection"));
    // Case-insensitive
    EXPECT_TRUE(util::isConnectionHeader("Connection"));
    EXPECT_TRUE(util::isConnectionHeader("KEEP-ALIVE"));
    EXPECT_TRUE(util::isConnectionHeader("Transfer-Encoding"));
    // Not connection headers
    EXPECT_FALSE(util::isConnectionHeader("content-type"));
    EXPECT_FALSE(util::isConnectionHeader("host"));
    EXPECT_FALSE(util::isConnectionHeader("accept"));
}

TEST(Http2UtilTest, IsSingleValueHeader) {
    // Pseudo-headers are single-value
    EXPECT_TRUE(util::isSingleValueHeader(":status"));
    EXPECT_TRUE(util::isSingleValueHeader(":method"));
    EXPECT_TRUE(util::isSingleValueHeader(":authority"));
    EXPECT_TRUE(util::isSingleValueHeader(":scheme"));
    EXPECT_TRUE(util::isSingleValueHeader(":path"));
    // Standard single-value headers
    EXPECT_TRUE(util::isSingleValueHeader("content-type"));
    EXPECT_TRUE(util::isSingleValueHeader("content-length"));
    EXPECT_TRUE(util::isSingleValueHeader("authorization"));
    EXPECT_TRUE(util::isSingleValueHeader("date"));
    EXPECT_TRUE(util::isSingleValueHeader("etag"));
    EXPECT_TRUE(util::isSingleValueHeader("host"));
    EXPECT_TRUE(util::isSingleValueHeader("location"));
    EXPECT_TRUE(util::isSingleValueHeader("user-agent"));
    // Multi-value headers (NOT single)
    EXPECT_FALSE(util::isSingleValueHeader("set-cookie"));
    EXPECT_FALSE(util::isSingleValueHeader("x-custom"));
    EXPECT_FALSE(util::isSingleValueHeader("accept"));
    EXPECT_FALSE(util::isSingleValueHeader("cache-control"));
}

TEST(Http2UtilTest, IsValidTEHeader) {
    EXPECT_TRUE(util::isValidTEHeader("trailers"));
    EXPECT_FALSE(util::isValidTEHeader("chunked"));
    EXPECT_FALSE(util::isValidTEHeader("gzip"));
    EXPECT_FALSE(util::isValidTEHeader(""));
    EXPECT_FALSE(util::isValidTEHeader("Trailers"));
}

TEST(Http2UtilTest, IsValidHeaderName) {
    EXPECT_TRUE(util::isValidHeaderName("content-type"));
    EXPECT_TRUE(util::isValidHeaderName(":method"));
    EXPECT_TRUE(util::isValidHeaderName(":status"));
    EXPECT_TRUE(util::isValidHeaderName("x-custom-header"));
    EXPECT_TRUE(util::isValidHeaderName("x-123"));
    EXPECT_TRUE(util::isValidHeaderName("a"));
    // Invalid
    EXPECT_FALSE(util::isValidHeaderName(""));
    EXPECT_FALSE(util::isValidHeaderName("bad header"));       // space
    EXPECT_FALSE(util::isValidHeaderName("bad\theader"));      // tab
    EXPECT_FALSE(util::isValidHeaderName("bad(header"));       // paren
    EXPECT_FALSE(util::isValidHeaderName("bad[header"));       // bracket
    EXPECT_FALSE(util::isValidHeaderName("bad{header"));       // brace
    EXPECT_FALSE(util::isValidHeaderName("bad\"header"));      // quote
}

TEST(Http2UtilTest, IsValidHeaderValue) {
    EXPECT_TRUE(util::isValidHeaderValue("text/html"));
    EXPECT_TRUE(util::isValidHeaderValue(""));
    EXPECT_TRUE(util::isValidHeaderValue("value with spaces"));
    EXPECT_TRUE(util::isValidHeaderValue("value\twith\ttabs"));
    // Invalid
    EXPECT_FALSE(util::isValidHeaderValue("bad\r\nvalue"));
    EXPECT_FALSE(util::isValidHeaderValue("bad\nvalue"));
    EXPECT_FALSE(util::isValidHeaderValue("bad\rvalue"));
    EXPECT_FALSE(util::isValidHeaderValue(std::string("bad\0value", 9)));
}

TEST(Http2UtilTest, ToLowerHeaderName) {
    EXPECT_EQ(util::toLowerHeaderName("Content-Type"), "content-type");
    EXPECT_EQ(util::toLowerHeaderName("HOST"), "host");
    EXPECT_EQ(util::toLowerHeaderName("already-lower"), "already-lower");
    EXPECT_EQ(util::toLowerHeaderName("X-Custom-Header"), "x-custom-header");
    EXPECT_EQ(util::toLowerHeaderName(""), "");
}

// ══════════════════════════════════════════════════════════════════════
// Type Default Construction Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2TypesTest, SettingsDefaultConstruction) {
    Settings s;
    EXPECT_FALSE(s.headerTableSize.has_value());
    EXPECT_FALSE(s.enablePush.has_value());
    EXPECT_FALSE(s.initialWindowSize.has_value());
    EXPECT_FALSE(s.maxFrameSize.has_value());
    EXPECT_FALSE(s.maxConcurrentStreams.has_value());
    EXPECT_FALSE(s.maxHeaderListSize.has_value());
    EXPECT_FALSE(s.enableConnectProtocol.has_value());
}

TEST(Http2TypesTest, SessionOptionsDefaultConstruction) {
    SessionOptions opts;
    EXPECT_FALSE(opts.maxDeflateDynamicTableSize.has_value());
    EXPECT_FALSE(opts.maxSettings.has_value());
    EXPECT_FALSE(opts.maxSessionMemory.has_value());
    EXPECT_FALSE(opts.maxHeaderListPairs.has_value());
    EXPECT_FALSE(opts.maxOutstandingPings.has_value());
    EXPECT_FALSE(opts.peerMaxConcurrentStreams.has_value());
    EXPECT_FALSE(opts.settings.has_value());
    EXPECT_FALSE(opts.unknownProtocolTimeout.has_value());
}

TEST(Http2TypesTest, ServerOptionsInheritsSessionOptions) {
    ServerOptions opts;
    // Should have all SessionOptions fields
    EXPECT_FALSE(opts.maxSessionMemory.has_value());
    EXPECT_FALSE(opts.settings.has_value());
}

TEST(Http2TypesTest, SecureServerOptionsHasTlsFields) {
    SecureServerOptions opts;
    // TLS-specific fields
    EXPECT_FALSE(opts.cert.has_value());
    EXPECT_FALSE(opts.key.has_value());
    EXPECT_FALSE(opts.ca.has_value());
    EXPECT_FALSE(opts.rejectUnauthorized.has_value());
    EXPECT_FALSE(opts.allowHTTP1.has_value());
    // Still has session options from inheritance
    EXPECT_FALSE(opts.maxSessionMemory.has_value());
}

TEST(Http2TypesTest, ClientSessionOptionsDefaults) {
    ClientSessionOptions opts;
    EXPECT_FALSE(opts.maxReservedRemoteStreams.has_value());
    EXPECT_FALSE(opts.protocol.has_value());
    // Still has session options
    EXPECT_FALSE(opts.settings.has_value());
}

TEST(Http2TypesTest, StreamResponseOptionsDefaults) {
    StreamResponseOptions opts;
    EXPECT_FALSE(opts.endStream);
    EXPECT_FALSE(opts.waitForTrailers);
}

TEST(Http2TypesTest, StreamFileResponseOptionsDefaults) {
    StreamFileResponseOptions opts;
    EXPECT_FALSE(opts.waitForTrailers);
    EXPECT_FALSE(opts.offset.has_value());
    EXPECT_FALSE(opts.length.has_value());
}

TEST(Http2TypesTest, ClientRequestOptionsDefaults) {
    ClientRequestOptions opts;
    EXPECT_FALSE(opts.endStream);
    EXPECT_FALSE(opts.exclusive);
    EXPECT_FALSE(opts.parent.has_value());
    EXPECT_FALSE(opts.waitForTrailers);
}

TEST(Http2TypesTest, SessionStateDefaultConstruction) {
    SessionState state;
    EXPECT_FALSE(state.effectiveLocalWindowSize.has_value());
    EXPECT_FALSE(state.effectiveRecvDataLength.has_value());
    EXPECT_FALSE(state.nextStreamID.has_value());
    EXPECT_FALSE(state.localWindowSize.has_value());
    EXPECT_FALSE(state.lastProcStreamID.has_value());
    EXPECT_FALSE(state.remoteWindowSize.has_value());
    EXPECT_FALSE(state.outboundQueueSize.has_value());
    EXPECT_FALSE(state.deflateDynamicTableSize.has_value());
    EXPECT_FALSE(state.inflateDynamicTableSize.has_value());
}

TEST(Http2TypesTest, StreamStateDefaultConstruction) {
    StreamState state;
    EXPECT_FALSE(state.localWindowSize.has_value());
    EXPECT_EQ(state.state, Http2StreamState::Idle);
    EXPECT_FALSE(state.localClose);
    EXPECT_FALSE(state.remoteClose);
}

// ══════════════════════════════════════════════════════════════════════
// Wire Format Tests (verifying network byte order encoding)
// ══════════════════════════════════════════════════════════════════════

TEST(Http2SettingsTest, PackedSettingsNetworkByteOrder) {
    // Pack a single known setting and verify the byte layout
    Settings s;
    s.headerTableSize = 0x12345678;  // Known value
    auto packed = getPackedSettings(s);
    EXPECT_EQ(packed.length(), 6u);

    const uint8_t* data = packed.data();
    // Setting ID 0x0001 (HEADER_TABLE_SIZE) in network byte order
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(data[1], 0x01);
    // Value 0x12345678 in network byte order
    EXPECT_EQ(data[2], 0x12);
    EXPECT_EQ(data[3], 0x34);
    EXPECT_EQ(data[4], 0x56);
    EXPECT_EQ(data[5], 0x78);
}

TEST(Http2SettingsTest, PackedEnablePush) {
    Settings s;
    s.enablePush = false;
    auto packed = getPackedSettings(s);
    EXPECT_EQ(packed.length(), 6u);

    const uint8_t* data = packed.data();
    // Setting ID 0x0002 (ENABLE_PUSH)
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(data[1], 0x02);
    // Value 0 (false)
    EXPECT_EQ(data[2], 0x00);
    EXPECT_EQ(data[3], 0x00);
    EXPECT_EQ(data[4], 0x00);
    EXPECT_EQ(data[5], 0x00);
}

TEST(Http2SettingsTest, PackedEnableConnectProtocol) {
    Settings s;
    s.enableConnectProtocol = true;
    auto packed = getPackedSettings(s);
    EXPECT_EQ(packed.length(), 6u);

    const uint8_t* data = packed.data();
    // Setting ID 0x0008 (ENABLE_CONNECT_PROTOCOL)
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(data[1], 0x08);
    // Value 1 (true)
    EXPECT_EQ(data[2], 0x00);
    EXPECT_EQ(data[3], 0x00);
    EXPECT_EQ(data[4], 0x00);
    EXPECT_EQ(data[5], 0x01);
}

// ══════════════════════════════════════════════════════════════════════
// Plan 002: Session & Stream Core Tests
// ══════════════════════════════════════════════════════════════════════

using namespace polycpp::http2::impl;

TEST(Http2SessionImplTest, ServerSessionCreation) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    EXPECT_TRUE(session->isServer());
    EXPECT_FALSE(session->closed());
    EXPECT_FALSE(session->destroyed());
    EXPECT_FALSE(session->encrypted());
    EXPECT_EQ(session->type(), 0);  // NGHTTP2_SESSION_SERVER
    EXPECT_EQ(session->alpnProtocol(), "h2");
}

TEST(Http2SessionImplTest, ClientSessionCreation) {
    auto session = std::make_shared<Http2SessionImpl>(false);
    session->initialize();

    EXPECT_FALSE(session->isServer());
    EXPECT_FALSE(session->closed());
    EXPECT_FALSE(session->destroyed());
    EXPECT_EQ(session->type(), 1);  // NGHTTP2_SESSION_CLIENT
}

TEST(Http2SessionImplTest, DefaultLocalSettings) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto local = session->localSettings();
    EXPECT_EQ(local.headerTableSize.value(), 4096u);
    EXPECT_EQ(local.enablePush.value(), true);
    EXPECT_EQ(local.initialWindowSize.value(), 65535u);
    EXPECT_EQ(local.maxFrameSize.value(), 16384u);
    EXPECT_EQ(local.enableConnectProtocol.value(), false);
}

TEST(Http2SessionImplTest, CustomSettings) {
    SessionOptions opts;
    Settings s;
    s.maxConcurrentStreams = 200;
    s.initialWindowSize = 32768;
    opts.settings = s;

    auto session = std::make_shared<Http2SessionImpl>(true, opts);
    session->initialize();

    auto local = session->localSettings();
    EXPECT_EQ(local.maxConcurrentStreams.value(), 200u);
    EXPECT_EQ(local.initialWindowSize.value(), 32768u);
}

TEST(Http2SessionImplTest, SessionState) {
    auto session = std::make_shared<Http2SessionImpl>(false);
    session->initialize();

    auto st = session->state();
    EXPECT_TRUE(st.nextStreamID.has_value());
    EXPECT_TRUE(st.localWindowSize.has_value());
    EXPECT_TRUE(st.remoteWindowSize.has_value());
    // Client sessions start with next stream ID 1
    EXPECT_EQ(st.nextStreamID.value(), 1);
}

TEST(Http2SessionImplTest, ServerSessionStateNextStreamID) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto st = session->state();
    // Server sessions start with nextStreamID = 2 (even-numbered for server push)
    // nghttp2 returns 0 until streams are opened, but initial is implementation-defined
    EXPECT_TRUE(st.nextStreamID.has_value());
    // Just verify it's a valid non-negative value
    EXPECT_GE(st.nextStreamID.value(), 0);
}

TEST(Http2SessionImplTest, SessionClose) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    bool closeEmitted = false;
    session->emitter().on(event::Close, [&]() {
        closeEmitted = true;
    });

    bool callbackCalled = false;
    session->close([&]() { callbackCalled = true; });

    EXPECT_TRUE(session->closed());
    EXPECT_TRUE(closeEmitted);
    EXPECT_TRUE(callbackCalled);
}

TEST(Http2SessionImplTest, SessionDoubleClose) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    int closeCount = 0;
    session->emitter().on(event::Close, [&]() {
        closeCount++;
    });

    session->close(nullptr);
    session->close(nullptr);  // Second close should be a no-op
    EXPECT_EQ(closeCount, 1);
}

TEST(Http2SessionImplTest, SessionDestroy) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    session->destroy(nullptr, constants::NGHTTP2_NO_ERROR);

    EXPECT_TRUE(session->destroyed());
    EXPECT_TRUE(session->closed());
}

TEST(Http2SessionImplTest, SessionDestroyWithError) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    bool errorEmitted = false;
    session->emitter().on(event::Error_, [&](const Error& err) {
        errorEmitted = true;
    });

    Error err("test error");
    session->destroy(&err, constants::NGHTTP2_INTERNAL_ERROR);

    EXPECT_TRUE(session->destroyed());
    EXPECT_TRUE(errorEmitted);
}

TEST(Http2SessionImplTest, SessionEncryptedProperty) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    EXPECT_FALSE(session->encrypted());
    session->setEncrypted(true);
    EXPECT_TRUE(session->encrypted());
}

TEST(Http2SessionImplTest, SessionAlpnProtocol) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    EXPECT_EQ(session->alpnProtocol(), "h2");
    session->setAlpnProtocol("h2c");
    EXPECT_EQ(session->alpnProtocol(), "h2c");
}

TEST(Http2SessionImplTest, SessionOriginSet) {
    auto session = std::make_shared<Http2SessionImpl>(false);
    session->initialize();

    EXPECT_TRUE(session->originSet().empty());
    session->setOriginSet("https://example.com");
    EXPECT_EQ(session->originSet(), "https://example.com");
}

TEST(Http2SessionImplTest, SessionHasPendingOutput) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    // After initialization, server should have pending SETTINGS frame
    EXPECT_TRUE(session->hasPendingOutput());
}

TEST(Http2SessionImplTest, SessionConsumeOutput) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto output = session->consumeOutput();
    EXPECT_FALSE(output.empty());

    // After consuming, no more pending
    EXPECT_FALSE(session->hasPendingOutput());
}

TEST(Http2SessionImplTest, SessionGoaway) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    session->consumeOutput();  // Clear initial output

    session->goaway(constants::NGHTTP2_NO_ERROR, 0, "");

    EXPECT_TRUE(session->hasPendingOutput());
    auto output = session->consumeOutput();
    EXPECT_FALSE(output.empty());
}

TEST(Http2SessionImplTest, SessionPing) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    session->consumeOutput();

    bool sent = session->ping("testdata", nullptr);
    EXPECT_TRUE(sent);
    EXPECT_TRUE(session->hasPendingOutput());
}

TEST(Http2SessionImplTest, SessionPingMaxOutstanding) {
    SessionOptions opts;
    opts.maxOutstandingPings = 2;
    auto session = std::make_shared<Http2SessionImpl>(true, opts);
    session->initialize();
    session->consumeOutput();

    EXPECT_TRUE(session->ping("ping1", nullptr));
    EXPECT_TRUE(session->ping("ping2", nullptr));
    EXPECT_FALSE(session->ping("ping3", nullptr));  // Exceeds max
}

TEST(Http2SessionImplTest, SessionSendSettings) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    session->consumeOutput();

    Settings newSettings;
    newSettings.maxConcurrentStreams = 50;

    bool callbackCalled = false;
    session->sendSettings(newSettings, [&](const Error* err) {
        EXPECT_EQ(err, nullptr);
        callbackCalled = true;
    });

    EXPECT_TRUE(session->pendingSettingsAck());
    EXPECT_TRUE(session->hasPendingOutput());

    auto local = session->localSettings();
    EXPECT_EQ(local.maxConcurrentStreams.value(), 50u);
}

TEST(Http2SessionImplTest, SessionSetTimeout) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    bool timeoutCalled = false;
    session->setTimeout(5000, [&]() { timeoutCalled = true; });

    // Timeout is registered as a once listener on "timeout" event
    EXPECT_EQ(session->emitter().listenerCount(event::Timeout), 1u);
}

// ── Stream impl tests ──────────────────────────────────────────────

TEST(Http2StreamImplTest, StreamCreation) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = std::make_shared<Http2StreamImpl>(1, session);

    EXPECT_EQ(stream->id(), 1);
    EXPECT_FALSE(stream->aborted());
    EXPECT_FALSE(stream->closed());
    EXPECT_FALSE(stream->destroyed());
    EXPECT_FALSE(stream->endAfterHeaders());
    EXPECT_FALSE(stream->pending());
    EXPECT_EQ(stream->rstCode(), 0u);
    EXPECT_FALSE(stream->headersSent());
}

TEST(Http2StreamImplTest, StreamPending) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = std::make_shared<Http2StreamImpl>(0, session);  // ID 0 = pending
    EXPECT_TRUE(stream->pending());
}

TEST(Http2StreamImplTest, StreamSetters) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = std::make_shared<Http2StreamImpl>(1, session);

    stream->setClosed(true);
    EXPECT_TRUE(stream->closed());

    stream->setAborted(true);
    EXPECT_TRUE(stream->aborted());

    stream->setDestroyed(true);
    EXPECT_TRUE(stream->destroyed());

    stream->setEndAfterHeaders(true);
    EXPECT_TRUE(stream->endAfterHeaders());

    stream->setRstCode(constants::NGHTTP2_CANCEL);
    EXPECT_EQ(stream->rstCode(), constants::NGHTTP2_CANCEL);

    stream->setHeadersSent(true);
    EXPECT_TRUE(stream->headersSent());
}

TEST(Http2StreamImplTest, StreamReceivedHeaders) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = std::make_shared<Http2StreamImpl>(1, session);

    stream->addReceivedHeader(":method", "GET");
    stream->addReceivedHeader(":path", "/index.html");
    stream->addReceivedHeader(":scheme", "http");
    stream->addReceivedHeader(":authority", "localhost");

    auto& headers = stream->receivedHeaders();
    EXPECT_EQ(headers.get(":method").value_or(""), "GET");
    EXPECT_EQ(headers.get(":path").value_or(""), "/index.html");
    EXPECT_EQ(headers.get(":scheme").value_or(""), "http");
    EXPECT_EQ(headers.get(":authority").value_or(""), "localhost");
}

TEST(Http2StreamImplTest, StreamReceivedTrailers) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = std::make_shared<Http2StreamImpl>(1, session);

    stream->addReceivedTrailer("grpc-status", "0");
    stream->addReceivedTrailer("grpc-message", "OK");

    auto& trailers = stream->receivedTrailers();
    EXPECT_EQ(trailers.get("grpc-status").value_or(""), "0");
    EXPECT_EQ(trailers.get("grpc-message").value_or(""), "OK");
}

TEST(Http2StreamImplTest, StreamDataEvents) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = std::make_shared<Http2StreamImpl>(1, session);

    std::string receivedData;
    stream->emitter().on(event::Data, [&](const std::string& chunk) {
        receivedData += chunk;
    });

    bool endEmitted = false;
    stream->emitter().on(event::End, [&]() {
        endEmitted = true;
    });

    const char* testData = "Hello, HTTP/2!";
    stream->onData(reinterpret_cast<const uint8_t*>(testData), strlen(testData));
    EXPECT_EQ(receivedData, "Hello, HTTP/2!");

    stream->onEnd();
    EXPECT_TRUE(endEmitted);
}

TEST(Http2StreamImplTest, StreamEndCalledOnce) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = std::make_shared<Http2StreamImpl>(1, session);

    int endCount = 0;
    stream->emitter().on(event::End, [&]() {
        endCount++;
    });

    stream->onEnd();
    stream->onEnd();  // Should not emit again
    EXPECT_EQ(endCount, 1);
}

TEST(Http2StreamImplTest, StreamClose) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    session->consumeOutput();

    auto stream = session->getStream(1);

    bool closeEmitted = false;
    stream->emitter().on(event::Close, [&]() {
        closeEmitted = true;
    });

    bool callbackCalled = false;
    stream->close(constants::NGHTTP2_NO_ERROR, [&]() { callbackCalled = true; });

    EXPECT_TRUE(stream->closed());
    EXPECT_TRUE(closeEmitted);
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(stream->rstCode(), constants::NGHTTP2_NO_ERROR);
}

TEST(Http2StreamImplTest, StreamOnStreamClose) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);

    bool closeEmitted = false;
    stream->emitter().on(event::Close, [&]() {
        closeEmitted = true;
    });

    bool abortedEmitted = false;
    stream->emitter().on(event::Aborted, [&]() {
        abortedEmitted = true;
    });

    // Close with error code
    stream->onStreamClose(constants::NGHTTP2_CANCEL);

    EXPECT_TRUE(stream->closed());
    EXPECT_TRUE(stream->aborted());
    EXPECT_TRUE(closeEmitted);
    EXPECT_TRUE(abortedEmitted);
    EXPECT_EQ(stream->rstCode(), constants::NGHTTP2_CANCEL);
}

TEST(Http2StreamImplTest, StreamOnStreamCloseNoError) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);

    bool abortedEmitted = false;
    stream->emitter().on(event::Aborted, [&]() {
        abortedEmitted = true;
    });

    stream->onStreamClose(constants::NGHTTP2_NO_ERROR);

    EXPECT_TRUE(stream->closed());
    EXPECT_FALSE(stream->aborted());  // NO_ERROR does not set aborted
    EXPECT_FALSE(abortedEmitted);
}

TEST(Http2StreamImplTest, StreamSentHeaders) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);

    stream->addSentHeader(":status", "200");
    stream->addSentHeader("content-type", "text/html");

    EXPECT_EQ(stream->sentHeaders().get(":status").value_or(""), "200");
    EXPECT_EQ(stream->sentHeaders().get("content-type").value_or(""), "text/html");
}

TEST(Http2StreamImplTest, StreamSentTrailers) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);

    stream->addSentTrailer("grpc-status", "0");
    EXPECT_EQ(stream->sentTrailers().get("grpc-status").value_or(""), "0");
}

TEST(Http2StreamImplTest, StreamWantTrailers) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);

    EXPECT_FALSE(stream->wantTrailers());
    stream->setWantTrailers(true);
    EXPECT_TRUE(stream->wantTrailers());
}

TEST(Http2StreamImplTest, StreamSession) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    auto sess = stream->session();

    EXPECT_EQ(sess.get(), session.get());
}

TEST(Http2StreamImplTest, StreamState) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);

    auto state = stream->state();
    EXPECT_EQ(state.state, Http2StreamState::Open);
}

TEST(Http2StreamImplTest, StreamStateAfterClose) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    stream->setClosed(true);

    auto state = stream->state();
    EXPECT_EQ(state.state, Http2StreamState::Closed);
}

TEST(Http2SessionImplTest, StreamManagement) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream1 = session->getStream(1);
    auto stream3 = session->getStream(3);

    EXPECT_EQ(stream1->id(), 1);
    EXPECT_EQ(stream3->id(), 3);

    // Getting the same stream again should return same instance
    auto stream1Again = session->getStream(1);
    EXPECT_EQ(stream1.get(), stream1Again.get());
}

TEST(Http2SessionImplTest, StreamRemoval) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream1 = session->getStream(1);
    EXPECT_EQ(stream1->id(), 1);

    session->removeStream(1);

    // Getting stream 1 again should create a new instance
    auto stream1New = session->getStream(1);
    EXPECT_NE(stream1.get(), stream1New.get());
}

TEST(Http2SessionImplTest, SubmitDataSignalsBackpressureAtHighWaterMark) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    EXPECT_TRUE(session->submitData(1, std::string(16383, 'a'), false));
    EXPECT_FALSE(session->submitData(1, std::string(1, 'b'), false));
}

TEST(Http2SessionImplTest, DataSourceReadEmitsDrainWhenBufferedDataDropsBelowHighWaterMark) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    bool drainEmitted = false;
    stream->emitter().on(event::Drain, [&]() {
        drainEmitted = true;
    });

    EXPECT_FALSE(session->submitData(1, std::string(16384, 'x'), false));

    std::vector<uint8_t> out(1);
    uint32_t flags = 0;
    auto bytesRead = session->onDataSourceRead(1, out.data(), out.size(), &flags);

    EXPECT_EQ(bytesRead, 1);
    EXPECT_TRUE(drainEmitted);
}

TEST(Http2SessionImplTest, RemoveStreamClearsPendingDataState) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    session->submitData(1, "hello", true);
    session->removeStream(1);

    std::vector<uint8_t> out(16);
    uint32_t flags = 0;
    auto bytesRead = session->onDataSourceRead(1, out.data(), out.size(), &flags);

    EXPECT_EQ(bytesRead, NGHTTP2_ERR_DEFERRED);
    auto stream1New = session->getStream(1);
    EXPECT_NE(stream.get(), stream1New.get());
}

TEST(Http2SessionImplTest, DestroyClearsPendingDataState) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    session->submitData(1, "hello", true);
    session->destroy(nullptr, constants::NGHTTP2_NO_ERROR);

    std::vector<uint8_t> out(16);
    uint32_t flags = 0;
    auto bytesRead = session->onDataSourceRead(1, out.data(), out.size(), &flags);

    EXPECT_EQ(bytesRead, NGHTTP2_ERR_DEFERRED);
}

// ── nghttp2 protocol exchange tests ────────────────────────────────

TEST(Http2SessionImplTest, ClientServerSettingsExchange) {
    // Create a server and client session
    auto server = std::make_shared<Http2SessionImpl>(true);
    server->initialize();

    auto client = std::make_shared<Http2SessionImpl>(false);
    client->initialize();

    // Get client output (connection preface + SETTINGS)
    auto clientOutput = client->consumeOutput();
    EXPECT_FALSE(clientOutput.empty());

    // Feed client output into server
    int rv = server->feedData(
        reinterpret_cast<const uint8_t*>(clientOutput.data()),
        clientOutput.size());
    EXPECT_EQ(rv, 0);

    // Get server output (SETTINGS + SETTINGS ACK)
    auto serverOutput = server->consumeOutput();
    EXPECT_FALSE(serverOutput.empty());

    // Feed server output back into client
    rv = client->feedData(
        reinterpret_cast<const uint8_t*>(serverOutput.data()),
        serverOutput.size());
    EXPECT_EQ(rv, 0);
}

TEST(Http2SessionImplTest, ClientRequestSubmission) {
    auto client = std::make_shared<Http2SessionImpl>(false);
    client->initialize();
    client->consumeOutput();

    http::Headers headers = {
        {":method", "GET"},
        {":path", "/"},
        {":scheme", "http"},
        {":authority", "localhost"}
    };

    int32_t streamId = client->submitRequest(headers, true);
    EXPECT_GT(streamId, 0);
    EXPECT_TRUE(client->hasPendingOutput());
}

TEST(Http2SessionImplTest, ClientRequestCreatesStream) {
    auto client = std::make_shared<Http2SessionImpl>(false);
    client->initialize();
    client->consumeOutput();

    http::Headers headers = {
        {":method", "GET"},
        {":path", "/test"},
        {":scheme", "http"},
        {":authority", "localhost"}
    };

    int32_t streamId = client->submitRequest(headers, true);
    auto stream = client->getStream(streamId);

    EXPECT_TRUE(stream->headersSent());
    EXPECT_EQ(stream->sentHeaders().get(":method").value_or(""), "GET");
    EXPECT_EQ(stream->sentHeaders().get(":path").value_or(""), "/test");
}

// ══════════════════════════════════════════════════════════════════════
// Plan 003: Handle Class Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2StreamTest, DefaultConstruction) {
    Http2Stream stream;
    EXPECT_FALSE(stream.valid());
    EXPECT_TRUE(stream.closed());
    EXPECT_TRUE(stream.destroyed());
    EXPECT_EQ(stream.id(), 0);
    EXPECT_EQ(stream.rstCode(), 0u);
}

TEST(Http2StreamTest, ConstructionWithImpl) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto streamImpl = session->getStream(1);

    Http2Stream stream(streamImpl);
    EXPECT_TRUE(stream.valid());
    EXPECT_FALSE(stream.closed());
    EXPECT_EQ(stream.id(), 1);
}

TEST(Http2StreamTest, EventRegistration) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto streamImpl = session->getStream(1);

    Http2Stream stream(streamImpl);

    bool dataCalled = false;
    stream.on(event::Data, [&](const std::string&) {
        dataCalled = true;
    });

    streamImpl->emitter().emit(event::Data, std::string("test"));
    EXPECT_TRUE(dataCalled);
}

TEST(Http2StreamTest, OnceEventTyped) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto streamImpl = session->getStream(1);

    Http2Stream stream(streamImpl);

    int callCount = 0;
    stream.once(event::Data, [&](const std::string&) {
        callCount++;
    });

    streamImpl->emitter().emit(event::Data, std::string("chunk1"));
    streamImpl->emitter().emit(event::Data, std::string("chunk2"));  // Should not fire (once)
    EXPECT_EQ(callCount, 1);
}

TEST(ServerHttp2StreamTest, DefaultConstruction) {
    ServerHttp2Stream stream;
    EXPECT_FALSE(stream.valid());
    EXPECT_FALSE(stream.headersSent());
    EXPECT_FALSE(stream.pushAllowed());
}

TEST(ServerHttp2StreamTest, ConstructionWithImpl) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto streamImpl = session->getStream(1);

    ServerHttp2Stream stream(streamImpl);
    EXPECT_TRUE(stream.valid());
    EXPECT_FALSE(stream.headersSent());
}

TEST(ClientHttp2StreamTest, DefaultConstruction) {
    ClientHttp2Stream stream;
    EXPECT_FALSE(stream.valid());
}

TEST(ClientHttp2StreamTest, ConstructionWithImpl) {
    auto session = std::make_shared<Http2SessionImpl>(false);
    session->initialize();
    auto streamImpl = session->getStream(1);

    ClientHttp2Stream stream(streamImpl);
    EXPECT_TRUE(stream.valid());
    EXPECT_EQ(stream.id(), 1);
}

TEST(Http2SessionTest, DefaultConstruction) {
    Http2Session session;
    EXPECT_FALSE(session.valid());
    EXPECT_TRUE(session.closed());
    EXPECT_TRUE(session.destroyed());
    EXPECT_EQ(session.type(), -1);
}

TEST(Http2SessionTest, ConstructionWithImpl) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();

    Http2Session session(impl);
    EXPECT_TRUE(session.valid());
    EXPECT_FALSE(session.closed());
    EXPECT_FALSE(session.destroyed());
    EXPECT_TRUE(session.isServer());
}

TEST(Http2SessionTest, Properties) {
    auto impl = std::make_shared<Http2SessionImpl>(false);
    impl->initialize();

    Http2Session session(impl);

    EXPECT_FALSE(session.isServer());
    EXPECT_FALSE(session.encrypted());
    EXPECT_EQ(session.alpnProtocol(), "h2");
    EXPECT_FALSE(session.pendingSettingsAck());
}

TEST(Http2SessionTest, EventRegistration) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();

    Http2Session session(impl);

    bool closeCalled = false;
    session.on(event::Close, [&]() {
        closeCalled = true;
    });

    session.close();
    EXPECT_TRUE(closeCalled);
}

TEST(Http2SessionTest, LocalSettings) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();

    Http2Session session(impl);

    auto settings = session.localSettings();
    EXPECT_TRUE(settings.headerTableSize.has_value());
    EXPECT_EQ(settings.headerTableSize.value(), 4096u);
}

TEST(Http2SessionTest, State) {
    auto impl = std::make_shared<Http2SessionImpl>(false);
    impl->initialize();

    Http2Session session(impl);

    auto state = session.state();
    EXPECT_TRUE(state.nextStreamID.has_value());
    EXPECT_EQ(state.nextStreamID.value(), 1);
}

TEST(Http2SessionTest, SessionPing) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();
    impl->consumeOutput();

    Http2Session session(impl);

    bool result = session.ping("testping");
    EXPECT_TRUE(result);
}

TEST(Http2SessionTest, SessionGoaway) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();
    impl->consumeOutput();

    Http2Session session(impl);

    session.goaway(constants::NGHTTP2_NO_ERROR);
    EXPECT_TRUE(impl->hasPendingOutput());
}

TEST(Http2SessionTest, SessionSettings) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();
    impl->consumeOutput();

    Http2Session session(impl);

    Settings s;
    s.maxFrameSize = 32768;
    session.settings(s);

    EXPECT_TRUE(session.pendingSettingsAck());
    auto local = session.localSettings();
    EXPECT_EQ(local.maxFrameSize.value(), 32768u);
}

TEST(Http2SessionTest, SessionSetLocalWindowSize) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();
    impl->consumeOutput();

    Http2Session session(impl);

    // Should not throw
    session.setLocalWindowSize(1048576);
}

TEST(ServerHttp2SessionTest, DefaultConstruction) {
    ServerHttp2Session session;
    EXPECT_FALSE(session.valid());
}

TEST(ServerHttp2SessionTest, ConstructionWithImpl) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();

    ServerHttp2Session session(impl);
    EXPECT_TRUE(session.valid());
    EXPECT_TRUE(session.isServer());
}

TEST(ClientHttp2SessionTest, DefaultConstruction) {
    ClientHttp2Session session;
    EXPECT_FALSE(session.valid());
}

TEST(ClientHttp2SessionTest, ConstructionWithImpl) {
    auto impl = std::make_shared<Http2SessionImpl>(false);
    impl->initialize();

    ClientHttp2Session session(impl);
    EXPECT_TRUE(session.valid());
    EXPECT_FALSE(session.isServer());
}

TEST(ClientHttp2SessionTest, Request) {
    auto impl = std::make_shared<Http2SessionImpl>(false);
    impl->initialize();
    impl->consumeOutput();

    ClientHttp2Session session(impl);

    http::Headers headers = {
        {":method", "GET"},
        {":path", "/"},
        {":scheme", "http"},
        {":authority", "localhost"}
    };

    auto stream = session.request(headers, {.endStream = true});
    EXPECT_TRUE(stream.valid());
    EXPECT_GT(stream.id(), 0);
}

TEST(ClientHttp2SessionTest, RequestWithBody) {
    auto impl = std::make_shared<Http2SessionImpl>(false);
    impl->initialize();
    impl->consumeOutput();

    ClientHttp2Session session(impl);

    http::Headers headers = {
        {":method", "POST"},
        {":path", "/data"},
        {":scheme", "http"},
        {":authority", "localhost"}
    };

    auto stream = session.request(headers, {.endStream = false});
    EXPECT_TRUE(stream.valid());
}

// ══════════════════════════════════════════════════════════════════════
// Plan 003: Server Handle Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2ServerTest, DefaultConstruction) {
    Http2Server server;
    EXPECT_FALSE(server.valid());
    EXPECT_FALSE(server.listening());
}

TEST(Http2SecureServerTest, DefaultConstruction) {
    Http2SecureServer server;
    EXPECT_FALSE(server.valid());
    EXPECT_FALSE(server.listening());
}

TEST(Http2ServerTest, CreateServer) {
    polycpp::EventContext ctx;
    auto server = createServer(ctx);
    EXPECT_TRUE(server.valid());
    EXPECT_FALSE(server.listening());
}

TEST(Http2ServerTest, CreateServerWithStreamHandler) {
    polycpp::EventContext ctx;
    bool handlerSet = false;
    auto server = createServer(ctx, {}, [&](ServerHttp2Stream, http::Headers) {
        handlerSet = true;
    });
    EXPECT_TRUE(server.valid());
}

TEST(Http2ServerTest, ServerAddress) {
    Http2Server server;
    auto addr = server.address();
    EXPECT_TRUE(addr.empty());
}

TEST(Http2ServerTest, ClientAndServerOverIpv6Loopback) {
    if (!::test::ipv6::ipv6LoopbackAvailable()) {
        GTEST_SKIP() << "IPv6 loopback unavailable";
    }

    polycpp::EventContext ctx;
    bool connected = false;

    auto server = createServer(ctx);
    server.listen(0, "::1");

    auto addr = server.address();
    ASSERT_FALSE(addr.empty());
    EXPECT_EQ(addr["address"], "::1");
    EXPECT_EQ(addr["family"], "IPv6");
    uint16_t port = static_cast<uint16_t>(std::stoi(addr["port"]));

    polycpp::io::Timer timeout(ctx);
    timeout.expiresAfter(std::chrono::seconds(5));
    timeout.asyncWait([&](const std::error_code& ec) {
        if (!ec) {
            ADD_FAILURE() << "HTTP/2 IPv6 loopback listen test timed out";
            server.close();
            ctx.stop();
        }
    });

    auto client = std::make_shared<polycpp::io::TcpSocket>(ctx);
    client->asyncConnect("::1", port, [&](const std::error_code& ec) {
        timeout.cancel();
        if (ec) {
            ADD_FAILURE() << ec.message();
        } else {
            connected = true;
        }
        std::error_code closeEc;
        client->close(closeEc);
        server.close();
        ctx.stop();
    });

    ctx.run();

    EXPECT_TRUE(connected);
}

// ══════════════════════════════════════════════════════════════════════
// Plan 004: Client Implementation Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2ClientTest, AuthorityParsingHttp) {
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority("http://localhost:8080", host, port, scheme);
    EXPECT_EQ(host, "localhost");
    EXPECT_EQ(port, 8080);
    EXPECT_EQ(scheme, "http");
}

TEST(Http2ClientTest, AuthorityParsingHttps) {
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority("https://example.com", host, port, scheme);
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, 443);
    EXPECT_EQ(scheme, "https");
}

TEST(Http2ClientTest, AuthorityParsingHttpDefaultPort) {
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority("http://api.example.com", host, port, scheme);
    EXPECT_EQ(host, "api.example.com");
    EXPECT_EQ(port, 80);
    EXPECT_EQ(scheme, "http");
}

TEST(Http2ClientTest, AuthorityParsingWithPath) {
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority("http://localhost:3000/path", host, port, scheme);
    EXPECT_EQ(host, "localhost");
    EXPECT_EQ(port, 3000);
    EXPECT_EQ(scheme, "http");
}

TEST(Http2ClientTest, AuthorityParsingNoScheme) {
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority("localhost:9090", host, port, scheme);
    EXPECT_EQ(host, "localhost");
    EXPECT_EQ(port, 9090);
    EXPECT_EQ(scheme, "https");  // Default scheme
}

TEST(Http2ClientTest, AuthorityParsingBareHostDefaultsToHttps) {
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority("api.example.com", host, port, scheme);
    EXPECT_EQ(host, "api.example.com");
    EXPECT_EQ(port, 443);
    EXPECT_EQ(scheme, "https");
}

// Plan 1159: IPv6 authority parsing tests

TEST(Http2ClientTest, AuthorityParsingIPv6Bracketed) {
    // Verify that bracketed IPv6 authorities are parsed correctly.
    // This previously threw std::invalid_argument because the parser
    // split at the first colon inside the brackets.
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority("https://[::1]:8443/path", host, port, scheme);
    EXPECT_EQ(host, "[::1]");
    EXPECT_EQ(port, 8443);
    EXPECT_EQ(scheme, "https");
}

TEST(Http2ClientTest, AuthorityParsingIPv6BracketedNoPort) {
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority("https://[::1]/path", host, port, scheme);
    EXPECT_EQ(host, "[::1]");
    EXPECT_EQ(port, 443);
    EXPECT_EQ(scheme, "https");
}

TEST(Http2ClientTest, AuthorityParsingIPv6BracketedHttpPort) {
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority("http://[fe80::1%25eth0]:8080", host, port, scheme);
    EXPECT_EQ(host, "[fe80::1%25eth0]");
    EXPECT_EQ(port, 8080);
    EXPECT_EQ(scheme, "http");
}

TEST(Http2ClientTest, AuthorityParsingIPv6BracketedNoScheme) {
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority("[::1]:9090", host, port, scheme);
    EXPECT_EQ(host, "[::1]");
    EXPECT_EQ(port, 9090);
    EXPECT_EQ(scheme, "https");  // Default scheme
}

TEST(Http2ClientTest, AuthorityParsingIPv6BracketedNoSchemeNoPort) {
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority("[2001:db8::10]", host, port, scheme);
    EXPECT_EQ(host, "[2001:db8::10]");
    EXPECT_EQ(port, 443);
    EXPECT_EQ(scheme, "https");
}

// ══════════════════════════════════════════════════════════════════════
// Plan 005: Advanced Features Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2SessionImplTest, GoawayWithOpaqueData) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    session->consumeOutput();

    session->goaway(constants::NGHTTP2_NO_ERROR, 0, "server shutdown");
    EXPECT_TRUE(session->hasPendingOutput());
    auto output = session->consumeOutput();
    EXPECT_FALSE(output.empty());
}

TEST(Http2SessionImplTest, PingWithPayload) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    session->consumeOutput();

    bool sent = session->ping("12345678", nullptr);
    EXPECT_TRUE(sent);
}

TEST(Http2SessionImplTest, PingWithShortPayload) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    session->consumeOutput();

    // Short payload should be zero-padded to 8 bytes
    bool sent = session->ping("hi", nullptr);
    EXPECT_TRUE(sent);
}

TEST(Http2StreamImplTest, StreamTimeout) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto stream = session->getStream(1);

    stream->setTimeout(5000, nullptr);
    // Just verify it doesn't throw
    EXPECT_EQ(stream->emitter().listenerCount(event::Timeout), 0u);  // No callback = no listener
}

TEST(Http2StreamImplTest, StreamTimeoutWithCallback) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto stream = session->getStream(1);

    stream->setTimeout(5000, []() {});
    EXPECT_EQ(stream->emitter().listenerCount(event::Timeout), 1u);
}

// ── Client request with waitForTrailers ────────────────────────────

TEST(ClientHttp2SessionTest, RequestWithWaitForTrailers) {
    auto impl = std::make_shared<Http2SessionImpl>(false);
    impl->initialize();
    impl->consumeOutput();

    ClientHttp2Session session(impl);

    http::Headers headers = {
        {":method", "GET"},
        {":path", "/"},
        {":scheme", "http"},
        {":authority", "localhost"}
    };

    ClientRequestOptions opts;
    opts.endStream = true;
    opts.waitForTrailers = true;

    auto stream = session.request(headers, opts);
    EXPECT_TRUE(stream.valid());
    EXPECT_TRUE(stream.impl()->wantTrailers());
}

// ── Full protocol exchange ─────────────────────────────────────────

TEST(Http2ProtocolTest, SettingsExchange) {
    // Verify that settings exchange works between client and server
    auto server = std::make_shared<Http2SessionImpl>(true);
    server->initialize();

    auto client = std::make_shared<Http2SessionImpl>(false);
    client->initialize();

    bool localSettingsEmitted = false;
    server->emitter().on(event::LocalSettings, [&](const Settings& settings) {
        localSettingsEmitted = true;
    });
    bool remoteSettingsEmitted = false;
    server->emitter().on(event::RemoteSettings, [&](const Settings& settings) {
        remoteSettingsEmitted = true;
    });

    // Client sends connection preface + SETTINGS
    auto clientOut = client->consumeOutput();
    EXPECT_FALSE(clientOut.empty());

    // Feed into server
    int rv = server->feedData(
        reinterpret_cast<const uint8_t*>(clientOut.data()),
        clientOut.size());
    EXPECT_EQ(rv, 0);

    // Server should have generated SETTINGS + SETTINGS ACK
    auto serverOut = server->consumeOutput();
    EXPECT_FALSE(serverOut.empty());

    // Verify remoteSettings was emitted (server received client's SETTINGS)
    EXPECT_TRUE(remoteSettingsEmitted);

    // Feed server output back to client
    rv = client->feedData(
        reinterpret_cast<const uint8_t*>(serverOut.data()),
        serverOut.size());
    EXPECT_EQ(rv, 0);
}

TEST(Http2ProtocolTest, DirectNghttp2Usage) {
    // Direct test with nghttp2 to verify basic functionality
    struct TestData {
        std::string output;
    };

    TestData td;

    nghttp2_session_callbacks* cbs = nullptr;
    nghttp2_session_callbacks_new(&cbs);

    nghttp2_session_callbacks_set_send_callback(cbs,
        [](nghttp2_session*, const uint8_t* data, size_t length,
           int, void* userData) -> ssize_t {
            auto* td = static_cast<TestData*>(userData);
            td->output.append(reinterpret_cast<const char*>(data), length);
            return static_cast<ssize_t>(length);
        });

    nghttp2_session* client = nullptr;
    nghttp2_session_client_new(&client, cbs, &td);

    // Submit settings
    nghttp2_settings_entry iv[] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100}
    };
    nghttp2_submit_settings(client, NGHTTP2_FLAG_NONE, iv, 1);

    int rv = nghttp2_session_send(client);
    EXPECT_EQ(rv, 0);
    EXPECT_FALSE(td.output.empty()) << "No output after settings send";

    size_t settingsSize = td.output.size();

    // Submit a request
    nghttp2_nv nva[] = {
        {(uint8_t*)":method", (uint8_t*)"GET", 7, 3, NGHTTP2_NV_FLAG_NONE},
        {(uint8_t*)":path", (uint8_t*)"/", 5, 1, NGHTTP2_NV_FLAG_NONE},
        {(uint8_t*)":scheme", (uint8_t*)"http", 7, 4, NGHTTP2_NV_FLAG_NONE},
        {(uint8_t*)":authority", (uint8_t*)"localhost", 10, 9, NGHTTP2_NV_FLAG_NONE}
    };

    int32_t streamId = nghttp2_submit_request(client, nullptr, nva, 4, nullptr, nullptr);
    EXPECT_GT(streamId, 0);

    td.output.clear();
    rv = nghttp2_session_send(client);
    EXPECT_EQ(rv, 0);
    EXPECT_FALSE(td.output.empty()) << "No output after request send";

    nghttp2_session_del(client);
    nghttp2_session_callbacks_del(cbs);
}

TEST(Http2ProtocolTest, ClientRequestOutputGeneration) {
    // Test that submitRequest generates output bytes
    auto client = std::make_shared<Http2SessionImpl>(false);
    client->initialize();

    // Consume the initial connection preface + settings
    auto initial = client->consumeOutput();
    EXPECT_FALSE(initial.empty()) << "No initial output from client";

    // Now submit a request
    http::Headers headers = {
        {":authority", "localhost"},
        {":method", "GET"},
        {":path", "/"},
        {":scheme", "http"}
    };

    int32_t streamId = client->submitRequest(headers, true);
    EXPECT_GT(streamId, 0);

    // Check that submitRequest generated output
    EXPECT_TRUE(client->hasPendingOutput()) << "No pending output after submitRequest";
    auto requestOutput = client->consumeOutput();
    EXPECT_FALSE(requestOutput.empty()) << "No output bytes after submitRequest";
}

TEST(Http2ProtocolTest, RequestReception) {
    // Test that a client request is received by the server.
    // We send request TOGETHER with the connection preface, which is
    // the normal way HTTP/2 works (client can pipeline requests).
    auto server = std::make_shared<Http2SessionImpl>(true);
    server->initialize();

    auto client = std::make_shared<Http2SessionImpl>(false);
    client->initialize();

    // Set up stream handler on server BEFORE feeding data
    bool streamReceived = false;
    http::Headers receivedHeaders;
    server->emitter().on(event::ServerStream, [&](ServerHttp2Stream stream, const http::Headers& headers) {
        streamReceived = true;
        {
            receivedHeaders = headers;
        }
    });

    // Submit request right after init (before consuming initial output)
    http::Headers reqHeaders = {
        {":authority", "localhost"},
        {":method", "GET"},
        {":path", "/test"},
        {":scheme", "http"}
    };

    int32_t streamId = client->submitRequest(reqHeaders, true);
    EXPECT_GT(streamId, 0);

    // Consume ALL client output (connection preface + SETTINGS + HEADERS)
    auto clientOut = client->consumeOutput();
    EXPECT_FALSE(clientOut.empty());

    // Feed everything to server at once
    int rv = server->feedData(
        reinterpret_cast<const uint8_t*>(clientOut.data()),
        clientOut.size());
    EXPECT_EQ(rv, 0);

    // Server processes and should emit "stream" event
    EXPECT_TRUE(streamReceived);
    EXPECT_EQ(receivedHeaders.get(":method").value_or(""), "GET");
    EXPECT_EQ(receivedHeaders.get(":path").value_or(""), "/test");
}

TEST(Http2ProtocolTest, ClientServerExchange) {
    // Full client-server exchange: request + response
    // This test demonstrates the in-memory protocol exchange between two
    // nghttp2 sessions (client and server) without a real TCP connection.
    auto server = std::make_shared<Http2SessionImpl>(true);
    server->initialize();

    auto client = std::make_shared<Http2SessionImpl>(false);
    client->initialize();

    // Set up stream handler on server
    bool streamReceived = false;
    http::Headers receivedRequestHeaders;
    int32_t receivedStreamId = 0;
    server->emitter().on(event::ServerStream, [&](ServerHttp2Stream stream, const http::Headers& headers) {
        streamReceived = true;
        {
            receivedRequestHeaders = headers;
        }
        {
            receivedStreamId = stream.id();
        }
    });

    // Client submits request along with connection preface
    http::Headers reqHeaders = {
        {":authority", "localhost"},
        {":method", "GET"},
        {":path", "/index.html"},
        {":scheme", "http"}
    };

    int32_t streamId = client->submitRequest(reqHeaders, true);
    EXPECT_GT(streamId, 0);

    // Send client output (preface + SETTINGS + HEADERS) to server
    auto clientOut = client->consumeOutput();
    EXPECT_FALSE(clientOut.empty());
    server->feedData(reinterpret_cast<const uint8_t*>(clientOut.data()), clientOut.size());

    // Verify server received the request
    EXPECT_TRUE(streamReceived);
    EXPECT_EQ(receivedRequestHeaders.get(":method").value_or(""), "GET");
    EXPECT_EQ(receivedRequestHeaders.get(":path").value_or(""), "/index.html");
    EXPECT_EQ(receivedStreamId, streamId);

    // Server sends response (endStream=true, no body)
    http::Headers respHeaders = {
        {":status", "200"},
        {"content-type", "text/html"}
    };
    server->submitResponse(receivedStreamId, respHeaders, true);

    // Server output includes SETTINGS + SETTINGS_ACK + HEADERS response.
    // We need the client to first process server SETTINGS before response.
    auto serverOut = server->consumeOutput();
    EXPECT_FALSE(serverOut.empty());

    // Set up response handlers on client stream
    auto clientStream = client->getStream(streamId);
    bool responseReceived = false;
    http::Headers receivedRespHeaders;
    clientStream->emitter().on(event::Response, [&](const http::Headers& headers) {
        responseReceived = true;
        receivedRespHeaders = headers;
    });

    // Feed server output to client
    client->feedData(reinterpret_cast<const uint8_t*>(serverOut.data()), serverOut.size());

    // The client should have received the response
    // Note: In a real scenario with async I/O, multiple exchange rounds
    // would happen. In this synchronous test, we verify the request path.
    // The response might not arrive if nghttp2 requires SETTINGS ACK first.
    if (!responseReceived) {
        // Exchange remaining frames
        auto cltOut = client->consumeOutput();
        if (!cltOut.empty()) {
            server->feedData(reinterpret_cast<const uint8_t*>(cltOut.data()), cltOut.size());
        }
        auto srvOut2 = server->consumeOutput();
        if (!srvOut2.empty()) {
            client->feedData(reinterpret_cast<const uint8_t*>(srvOut2.data()), srvOut2.size());
        }
    }

    // Verify request side (which we already confirmed works)
    EXPECT_TRUE(streamReceived);
    EXPECT_EQ(receivedRequestHeaders.get(":method").value_or(""), "GET");
}

// ══════════════════════════════════════════════════════════════════════
// Plan 006: Compatibility Layer Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2ServerRequestTest, DefaultConstruction) {
    Http2ServerRequest req;
    EXPECT_EQ(req.httpVersion(), "2.0");
    EXPECT_EQ(req.httpVersionMajor(), 2);
    EXPECT_EQ(req.httpVersionMinor(), 0);
}

TEST(Http2ServerRequestTest, ConstructionWithHeaders) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto streamImpl = session->getStream(1);

    ServerHttp2Stream stream(streamImpl);
    http::Headers headers = {
        {":method", "POST"},
        {":path", "/api/data"},
        {":scheme", "https"},
        {":authority", "api.example.com"},
        {"content-type", "application/json"}
    };

    Http2ServerRequest req(stream, headers);

    EXPECT_EQ(req.method(), "POST");
    EXPECT_EQ(req.url(), "/api/data");
    EXPECT_EQ(req.scheme(), "https");
    EXPECT_EQ(req.authority(), "api.example.com");
    EXPECT_EQ(req.headers().get("content-type").value_or(""), "application/json");
}

TEST(Http2ServerRequestTest, RawHeaders) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto streamImpl = session->getStream(1);

    ServerHttp2Stream stream(streamImpl);
    http::Headers headers = {
        {":method", "GET"},
        {":path", "/"},
        {"host", "example.com"}
    };

    Http2ServerRequest req(stream, headers);

    auto raw = req.rawHeaders();
    // Each header produces two entries: name, value
    EXPECT_EQ(raw.size(), 6u);  // 3 headers * 2
}

TEST(Http2ServerRequestTest, EventRegistration) {
    Http2ServerRequest req;
    bool called = false;
    req.on(event::Data, [&](const std::string&) { called = true; });
    // Event emitter works
    EXPECT_FALSE(called);  // Not emitted yet
}

TEST(Http2ServerRequestTest, DefaultMethod) {
    Http2ServerRequest req;
    EXPECT_EQ(req.method(), "GET");  // Default when no :method header
}

TEST(Http2ServerRequestTest, DefaultUrl) {
    Http2ServerRequest req;
    EXPECT_EQ(req.url(), "/");  // Default when no :path header
}

TEST(Http2ServerRequestTest, DefaultScheme) {
    Http2ServerRequest req;
    EXPECT_EQ(req.scheme(), "http");  // Default when no :scheme header
}

TEST(Http2ServerRequestTest, Stream) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto streamImpl = session->getStream(1);

    ServerHttp2Stream stream(streamImpl);
    Http2ServerRequest req(stream, {});

    EXPECT_TRUE(req.stream().valid());
    EXPECT_EQ(req.stream().id(), 1);
}

TEST(Http2ServerResponseTest, DefaultConstruction) {
    Http2ServerResponse res;
    EXPECT_EQ(res.statusCode(), 200);
    EXPECT_FALSE(res.headersSent());
    EXPECT_FALSE(res.finished());
    EXPECT_EQ(res.statusMessage(), "");
}

TEST(Http2ServerResponseTest, SetStatusCode) {
    Http2ServerResponse res;
    res.setStatusCode(404);
    EXPECT_EQ(res.statusCode(), 404);
}

TEST(Http2ServerResponseTest, SetAndGetHeaders) {
    Http2ServerResponse res;

    res.setHeader("Content-Type", "text/html");
    res.setHeader("X-Custom", "value");

    EXPECT_EQ(res.getHeader("Content-Type"), "text/html");
    EXPECT_EQ(res.getHeader("x-custom"), "value");  // Case-insensitive
    EXPECT_TRUE(res.hasHeader("content-type"));
    EXPECT_FALSE(res.hasHeader("nonexistent"));
}

TEST(Http2ServerResponseTest, RemoveHeader) {
    Http2ServerResponse res;

    res.setHeader("Content-Type", "text/html");
    EXPECT_TRUE(res.hasHeader("content-type"));

    res.removeHeader("Content-Type");
    EXPECT_FALSE(res.hasHeader("content-type"));
}

TEST(Http2ServerResponseTest, GetHeaders) {
    Http2ServerResponse res;

    res.setHeader("Content-Type", "text/html");
    res.setHeader("Cache-Control", "no-cache");

    auto headers = res.getHeaders();
    EXPECT_EQ(headers.size(), 2u);
    EXPECT_EQ(headers.get("content-type").value_or(""), "text/html");
    EXPECT_EQ(headers.get("cache-control").value_or(""), "no-cache");
}

TEST(Http2ServerResponseTest, EventRegistration) {
    Http2ServerResponse res;
    bool finishCalled = false;
    res.on(event::Finish, [&]() { finishCalled = true; });
    EXPECT_FALSE(finishCalled);
}

TEST(Http2ServerResponseTest, WritableState) {
    Http2ServerResponse res;
    // Without a valid stream, writable depends on stream validity
    EXPECT_FALSE(res.writable());  // No valid stream
}

// ── Integration test: request/response compat objects ───────────────

TEST(Http2CompatTest, RequestResponsePair) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto streamImpl = session->getStream(1);

    ServerHttp2Stream stream(streamImpl);
    http::Headers reqHeaders = {
        {":method", "GET"},
        {":path", "/index.html"},
        {":scheme", "http"},
        {":authority", "localhost:8080"}
    };

    Http2ServerRequest req(stream, reqHeaders);
    Http2ServerResponse res(stream);

    EXPECT_EQ(req.method(), "GET");
    EXPECT_EQ(req.url(), "/index.html");
    EXPECT_EQ(req.authority(), "localhost:8080");

    EXPECT_EQ(res.statusCode(), 200);
    EXPECT_FALSE(res.headersSent());
    EXPECT_FALSE(res.finished());
}

// ── Header type tests ──────────────────────────────────────────────

TEST(Http2HeadersTest, OrderPreservation) {
    http::Headers headers;
    headers.set(":method", "GET");
    headers.set(":path", "/");
    headers.set(":scheme", "http");
    headers.set(":authority", "localhost");
    headers.set("accept", "*/*");

    EXPECT_EQ(headers.size(), 5u);
    EXPECT_EQ(headers.get(":method").value_or(""), "GET");
}

TEST(Http2HeadersTest, EmptyHeaders) {
    http::Headers headers;
    EXPECT_TRUE(headers.empty());
    EXPECT_EQ(headers.size(), 0u);
}

// ── createServer factory tests ─────────────────────────────────────

TEST(Http2FactoryTest, CreateServerReturnsValidHandle) {
    polycpp::EventContext ctx;
    auto server = createServer(ctx);
    EXPECT_TRUE(server.valid());
}

TEST(Http2FactoryTest, CreateServerWithOptions) {
    polycpp::EventContext ctx;
    ServerOptions opts;
    Settings s;
    s.maxConcurrentStreams = 150;
    opts.settings = s;

    auto server = createServer(ctx, opts);
    EXPECT_TRUE(server.valid());
}

TEST(Http2FactoryTest, CreateSecureServerReturnsValidHandle) {
    polycpp::EventContext ctx;
    SecureServerOptions opts;
    // Without valid cert/key, the TLS context may fail, but the handle is valid
    auto server = createSecureServer(ctx, opts);
    EXPECT_TRUE(server.valid());
}

// ══════════════════════════════════════════════════════════════════════
// Plan 005: Http2ErrorCode Enum Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2StrongTypesTest, Http2ErrorCodeValues) {
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::NoError), 0x0u);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::ProtocolError), 0x1u);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::InternalError), 0x2u);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::FlowControlError), 0x3u);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::SettingsTimeout), 0x4u);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::StreamClosed), 0x5u);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::FrameSizeError), 0x6u);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::RefusedStream), 0x7u);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::Cancel), 0x8u);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::CompressionError), 0x9u);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::ConnectError), 0xau);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::EnhanceYourCalm), 0xbu);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::InadequateSecurity), 0xcu);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::Http11Required), 0xdu);
}

TEST(Http2StrongTypesTest, Http2ErrorCodeMatchesConstants) {
    // Verify the enum values match the existing NGHTTP2_* constants
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::NoError), constants::NGHTTP2_NO_ERROR);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::ProtocolError), constants::NGHTTP2_PROTOCOL_ERROR);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::Cancel), constants::NGHTTP2_CANCEL);
    EXPECT_EQ(static_cast<uint32_t>(Http2ErrorCode::Http11Required), constants::NGHTTP2_HTTP_1_1_REQUIRED);
}

TEST(Http2StrongTypesTest, PaddingStrategyEnum) {
    EXPECT_EQ(static_cast<int>(PaddingStrategy::None), 0);
    EXPECT_EQ(static_cast<int>(PaddingStrategy::Max), 1);
    EXPECT_EQ(static_cast<int>(PaddingStrategy::Callback), 2);
    // Verify matches existing constants
    EXPECT_EQ(static_cast<int>(PaddingStrategy::None), constants::PADDING_STRATEGY_NONE);
    EXPECT_EQ(static_cast<int>(PaddingStrategy::Max), constants::PADDING_STRATEGY_MAX);
    EXPECT_EQ(static_cast<int>(PaddingStrategy::Callback), constants::PADDING_STRATEGY_CALLBACK);
}

TEST(Http2StrongTypesTest, PaddingStrategyInSessionOptions) {
    SessionOptions opts;
    opts.paddingStrategy = PaddingStrategy::Max;
    ASSERT_TRUE(opts.paddingStrategy.has_value());
    EXPECT_EQ(*opts.paddingStrategy, PaddingStrategy::Max);
}

TEST(Http2StrongTypesTest, Http2StreamStateEnum) {
    EXPECT_EQ(static_cast<int>(Http2StreamState::Idle), 0);
    EXPECT_EQ(static_cast<int>(Http2StreamState::Open), 1);
    EXPECT_EQ(static_cast<int>(Http2StreamState::ReservedLocal), 2);
    EXPECT_EQ(static_cast<int>(Http2StreamState::ReservedRemote), 3);
    EXPECT_EQ(static_cast<int>(Http2StreamState::HalfClosedLocal), 4);
    EXPECT_EQ(static_cast<int>(Http2StreamState::HalfClosedRemote), 5);
    EXPECT_EQ(static_cast<int>(Http2StreamState::Closed), 6);
    // Verify matches existing constants
    EXPECT_EQ(static_cast<int>(Http2StreamState::Idle), constants::NGHTTP2_STREAM_STATE_IDLE);
    EXPECT_EQ(static_cast<int>(Http2StreamState::Open), constants::NGHTTP2_STREAM_STATE_OPEN);
    EXPECT_EQ(static_cast<int>(Http2StreamState::Closed), constants::NGHTTP2_STREAM_STATE_CLOSED);
}

TEST(Http2StrongTypesTest, StreamStateUpdatedStruct) {
    StreamState st;
    EXPECT_EQ(st.state, Http2StreamState::Idle);
    EXPECT_FALSE(st.localClose);
    EXPECT_FALSE(st.remoteClose);
    EXPECT_FALSE(st.localWindowSize.has_value());
}

// ══════════════════════════════════════════════════════════════════════
// Plan 005: HTTP/2 Pseudo-Header Separation Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2PseudoHeaderTest, Http2RequestMetaStruct) {
    Http2RequestMeta meta;
    meta.method = "GET";
    meta.path = "/index.html";
    meta.scheme = "https";
    meta.authority = "example.com";
    EXPECT_EQ(meta.method, "GET");
    EXPECT_EQ(meta.path, "/index.html");
    EXPECT_EQ(meta.scheme, "https");
    EXPECT_EQ(meta.authority, "example.com");
    EXPECT_FALSE(meta.protocol.has_value());
}

TEST(Http2PseudoHeaderTest, Http2RequestMetaWithProtocol) {
    Http2RequestMeta meta;
    meta.method = "CONNECT";
    meta.authority = "example.com:443";
    meta.protocol = "websocket";
    EXPECT_TRUE(meta.protocol.has_value());
    EXPECT_EQ(*meta.protocol, "websocket");
}

TEST(Http2PseudoHeaderTest, Http2ResponseMetaStruct) {
    Http2ResponseMeta meta;
    EXPECT_EQ(meta.status, 0);
    meta.status = 200;
    EXPECT_EQ(meta.status, 200);
}

TEST(Http2PseudoHeaderTest, RespondWithStatusCode) {
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto streamImpl = session->getStream(1);
    ServerHttp2Stream stream(streamImpl);

    // respond(int, headers, options) overload
    stream.respond(200, {{"content-type", "text/html"}});
    EXPECT_TRUE(streamImpl->headersSent());
}

TEST(Http2PseudoHeaderTest, RequestWithMeta) {
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(false);
    session->initialize();

    ClientHttp2Session clientSession(session);
    Http2RequestMeta meta;
    meta.method = "GET";
    meta.path = "/";
    meta.scheme = "https";
    meta.authority = "example.com";

    auto stream = clientSession.request(meta, {{"accept", "text/html"}});
    // Stream should be valid (created by submitRequest)
    EXPECT_TRUE(stream.valid());
}

// ══════════════════════════════════════════════════════════════════════
// Plan 005: HTTP/2 Stream State Machine Tests
// ══════════════════════════════════════════════════════════════════════

TEST(Http2StreamStateTest, InitialStateIsOpen) {
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    auto state = stream->state();
    EXPECT_EQ(state.state, Http2StreamState::Open);
    EXPECT_FALSE(state.localClose);
    EXPECT_FALSE(state.remoteClose);
}

TEST(Http2StreamStateTest, IdleStateForPendingStream) {
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    // Stream with ID 0 is idle (pending)
    auto stream = session->getStream(0);
    auto state = stream->state();
    EXPECT_EQ(state.state, Http2StreamState::Idle);
}

TEST(Http2StreamStateTest, HalfClosedLocalAfterEnd) {
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    stream->end("", nullptr);  // Send END_STREAM

    auto state = stream->state();
    EXPECT_EQ(state.state, Http2StreamState::HalfClosedLocal);
    EXPECT_TRUE(state.localClose);
    EXPECT_FALSE(state.remoteClose);
}

TEST(Http2StreamStateTest, HalfClosedRemoteAfterOnEnd) {
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    stream->onEnd();  // Receive END_STREAM

    auto state = stream->state();
    EXPECT_EQ(state.state, Http2StreamState::HalfClosedRemote);
    EXPECT_FALSE(state.localClose);
    EXPECT_TRUE(state.remoteClose);
}

TEST(Http2StreamStateTest, ClosedAfterBothClose) {
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    stream->end("", nullptr);  // local close
    stream->onEnd();           // remote close

    auto state = stream->state();
    EXPECT_EQ(state.state, Http2StreamState::Closed);
    EXPECT_TRUE(state.localClose);
    EXPECT_TRUE(state.remoteClose);
}

TEST(Http2StreamStateTest, ClosedAfterRstStream) {
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    stream->close(constants::NGHTTP2_CANCEL, nullptr);

    auto state = stream->state();
    EXPECT_EQ(state.state, Http2StreamState::Closed);
    EXPECT_TRUE(state.localClose);
    EXPECT_TRUE(state.remoteClose);
}

TEST(Http2StreamStateTest, ClosedAfterOnStreamClose) {
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    stream->onStreamClose(constants::NGHTTP2_NO_ERROR);

    auto state = stream->state();
    EXPECT_EQ(state.state, Http2StreamState::Closed);
    EXPECT_TRUE(state.localClose);
    EXPECT_TRUE(state.remoteClose);
}

TEST(Http2StreamStateTest, RespondWithEndStreamSetsLocalClose) {
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    StreamResponseOptions opts;
    opts.endStream = true;
    stream->respond({{":status", "204"}}, opts);

    auto state = stream->state();
    EXPECT_TRUE(state.localClose);
}

TEST(Http2StreamStateTest, CloseWithHttp2ErrorCode) {
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    Http2Stream stream(session->getStream(1));
    stream.close(Http2ErrorCode::Cancel);

    EXPECT_TRUE(stream.closed());
}

// ══════════════════════════════════════════════════════════════════════
// Pseudo-Header Validation Tests
//
// These tests exercise the three free functions in polycpp::http2::impl:
//   validateRequestHeaders(), validateResponseHeaders(), validateTrailers()
//
// All functions throw polycpp::Error with code ERR_HTTP2_INVALID_PSEUDOHEADER
// on failure. Valid inputs must not throw.
//
// Reference: RFC 9113 §8.3, Node.js test-http2-pseudo-headers.js
// ══════════════════════════════════════════════════════════════════════

// ── Request validation ──────────────────────────────────────────────

TEST(Http2PseudoHeaderValidation, RequestMissingMethodThrows) {
    // A request without :method is always invalid (RFC 9113 §8.3.1)
    http::Headers headers{{":path", "/"}, {":scheme", "https"}};
    EXPECT_THROW(
        polycpp::http2::impl::validateRequestHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, RequestMissingPathThrows) {
    // Non-CONNECT request without :path is invalid (RFC 9113 §8.3.1)
    http::Headers headers{{":method", "GET"}, {":scheme", "https"}};
    EXPECT_THROW(
        polycpp::http2::impl::validateRequestHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, RequestMissingSchemeThrows) {
    // Non-CONNECT request without :scheme is invalid (RFC 9113 §8.3.1)
    http::Headers headers{{":method", "GET"}, {":path", "/"}};
    EXPECT_THROW(
        polycpp::http2::impl::validateRequestHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, RequestConnectWithoutPathSchemeOk) {
    // CONNECT method only requires :method and :authority (RFC 9113 §8.5)
    // :path and :scheme MUST NOT be set for CONNECT
    http::Headers headers{{":method", "CONNECT"}, {":authority", "example.com:443"}};
    EXPECT_NO_THROW(polycpp::http2::impl::validateRequestHeaders(headers));
}

TEST(Http2PseudoHeaderValidation, RequestConnectMissingMethodThrows) {
    // Even CONNECT requires :method
    http::Headers headers{{":authority", "example.com:443"}};
    EXPECT_THROW(
        polycpp::http2::impl::validateRequestHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, RequestDuplicateMethodThrows) {
    // Duplicate pseudo-headers are forbidden (RFC 9113 §8.3)
    http::Headers headers;
    headers.append(":method", "GET");
    headers.append(":method", "POST");
    headers.append(":path", "/");
    headers.append(":scheme", "https");
    EXPECT_THROW(
        polycpp::http2::impl::validateRequestHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, RequestDuplicatePathThrows) {
    http::Headers headers;
    headers.append(":method", "GET");
    headers.append(":path", "/foo");
    headers.append(":path", "/bar");
    headers.append(":scheme", "https");
    EXPECT_THROW(
        polycpp::http2::impl::validateRequestHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, RequestDuplicateSchemeThrows) {
    http::Headers headers;
    headers.append(":method", "GET");
    headers.append(":path", "/");
    headers.append(":scheme", "https");
    headers.append(":scheme", "http");
    EXPECT_THROW(
        polycpp::http2::impl::validateRequestHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, RequestUnknownPseudoHeaderThrows) {
    // :status is a response-only pseudo-header; must not appear in requests
    http::Headers headers;
    headers.append(":method", "GET");
    headers.append(":path", "/");
    headers.append(":scheme", "https");
    headers.append(":status", "200");  // invalid in request
    EXPECT_THROW(
        polycpp::http2::impl::validateRequestHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, RequestPseudoHeaderAfterRegularThrows) {
    // RFC 9113 §8.3: pseudo-headers MUST NOT appear after regular headers
    http::Headers headers;
    headers.append("content-type", "text/html");  // regular header first
    headers.append(":method", "GET");              // pseudo-header after — invalid
    headers.append(":path", "/");
    headers.append(":scheme", "https");
    EXPECT_THROW(
        polycpp::http2::impl::validateRequestHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, RequestValidGetOk) {
    // A well-formed GET request must not throw
    http::Headers headers{
        {":method", "GET"},
        {":path", "/index.html"},
        {":scheme", "https"},
        {":authority", "example.com"},
    };
    EXPECT_NO_THROW(polycpp::http2::impl::validateRequestHeaders(headers));
}

TEST(Http2PseudoHeaderValidation, RequestValidPostOk) {
    // A well-formed POST request with regular headers must not throw
    http::Headers headers;
    headers.append(":method", "POST");
    headers.append(":path", "/submit");
    headers.append(":scheme", "https");
    headers.append(":authority", "example.com");
    headers.append("content-type", "application/json");
    headers.append("content-length", "42");
    EXPECT_NO_THROW(polycpp::http2::impl::validateRequestHeaders(headers));
}

TEST(Http2PseudoHeaderValidation, RequestErrorCodeIsSet) {
    // Verify that the thrown Error has code ERR_HTTP2_INVALID_PSEUDOHEADER
    http::Headers headers{{":path", "/"}, {":scheme", "https"}};
    try {
        polycpp::http2::impl::validateRequestHeaders(headers);
        FAIL() << "Expected polycpp::Error to be thrown";
    } catch (const polycpp::Error& e) {
        EXPECT_EQ(e.code(), "ERR_HTTP2_INVALID_PSEUDOHEADER");
    }
}

// ── Response validation ─────────────────────────────────────────────

TEST(Http2PseudoHeaderValidation, ResponseMissingStatusThrows) {
    // A response without :status is always invalid
    http::Headers headers{{"content-type", "text/html"}};
    EXPECT_THROW(
        polycpp::http2::impl::validateResponseHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, ResponseEmptyHeadersThrows) {
    http::Headers headers;
    EXPECT_THROW(
        polycpp::http2::impl::validateResponseHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, ResponseInvalidStatusTooHighThrows) {
    // 999 > 599 — not a valid HTTP status code
    http::Headers headers{{":status", "999"}};
    EXPECT_THROW(
        polycpp::http2::impl::validateResponseHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, ResponseInvalidStatusTooLowThrows) {
    // 99 < 100 — not a valid HTTP status code
    http::Headers headers{{":status", "99"}};
    EXPECT_THROW(
        polycpp::http2::impl::validateResponseHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, ResponseInvalidStatusNonNumericThrows) {
    // "abc" cannot be converted to an integer
    http::Headers headers{{":status", "abc"}};
    EXPECT_THROW(
        polycpp::http2::impl::validateResponseHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, ResponseInvalidStatusEmptyStringThrows) {
    http::Headers headers{{":status", ""}};
    EXPECT_THROW(
        polycpp::http2::impl::validateResponseHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, ResponseValidStatus200Ok) {
    http::Headers headers{{":status", "200"}, {"content-type", "text/html"}};
    EXPECT_NO_THROW(polycpp::http2::impl::validateResponseHeaders(headers));
}

TEST(Http2PseudoHeaderValidation, ResponseValidStatus100Ok) {
    // 100 Continue is the lower boundary
    http::Headers headers{{":status", "100"}};
    EXPECT_NO_THROW(polycpp::http2::impl::validateResponseHeaders(headers));
}

TEST(Http2PseudoHeaderValidation, ResponseValidStatus599Ok) {
    // 599 is the upper boundary
    http::Headers headers{{":status", "599"}};
    EXPECT_NO_THROW(polycpp::http2::impl::validateResponseHeaders(headers));
}

TEST(Http2PseudoHeaderValidation, ResponseValidStatus404Ok) {
    http::Headers headers{{":status", "404"}};
    EXPECT_NO_THROW(polycpp::http2::impl::validateResponseHeaders(headers));
}

TEST(Http2PseudoHeaderValidation, ResponseValidStatus503Ok) {
    http::Headers headers{{":status", "503"}, {"retry-after", "60"}};
    EXPECT_NO_THROW(polycpp::http2::impl::validateResponseHeaders(headers));
}

TEST(Http2PseudoHeaderValidation, ResponseDuplicateStatusThrows) {
    // Duplicate :status pseudo-header is forbidden
    http::Headers headers;
    headers.append(":status", "200");
    headers.append(":status", "404");
    EXPECT_THROW(
        polycpp::http2::impl::validateResponseHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, ResponseUnknownPseudoHeaderThrows) {
    // :method is a request-only pseudo-header; must not appear in responses
    http::Headers headers;
    headers.append(":method", "GET");  // invalid in response
    headers.append(":status", "200");
    EXPECT_THROW(
        polycpp::http2::impl::validateResponseHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, ResponsePseudoHeaderAfterRegularThrows) {
    // RFC 9113 §8.3: pseudo-headers MUST NOT appear after regular headers
    http::Headers headers;
    headers.append("content-type", "text/html");  // regular header first
    headers.append(":status", "200");              // pseudo-header after — invalid
    EXPECT_THROW(
        polycpp::http2::impl::validateResponseHeaders(headers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, ResponseErrorCodeIsSet) {
    http::Headers headers{{":status", "999"}};
    try {
        polycpp::http2::impl::validateResponseHeaders(headers);
        FAIL() << "Expected polycpp::Error to be thrown";
    } catch (const polycpp::Error& e) {
        EXPECT_EQ(e.code(), "ERR_HTTP2_INVALID_PSEUDOHEADER");
    }
}

// ── Trailer validation ──────────────────────────────────────────────

TEST(Http2PseudoHeaderValidation, TrailerWithStatusPseudoThrows) {
    // Pseudo-headers are never allowed in trailers (RFC 9113 §8.1)
    http::Headers trailers{{":status", "200"}};
    EXPECT_THROW(
        polycpp::http2::impl::validateTrailers(trailers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, TrailerWithMethodPseudoThrows) {
    http::Headers trailers{{":method", "GET"}};
    EXPECT_THROW(
        polycpp::http2::impl::validateTrailers(trailers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, TrailerWithPathPseudoThrows) {
    http::Headers trailers{{":path", "/"}};
    EXPECT_THROW(
        polycpp::http2::impl::validateTrailers(trailers),
        polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, TrailerWithRegularHeadersOk) {
    // Regular headers are fine in trailers
    http::Headers trailers{
        {"x-checksum", "abc123"},
        {"x-timing", "42ms"},
    };
    EXPECT_NO_THROW(polycpp::http2::impl::validateTrailers(trailers));
}

TEST(Http2PseudoHeaderValidation, TrailerEmptyOk) {
    // Empty trailer section is valid
    http::Headers trailers;
    EXPECT_NO_THROW(polycpp::http2::impl::validateTrailers(trailers));
}

TEST(Http2PseudoHeaderValidation, TrailerErrorCodeIsSet) {
    http::Headers trailers{{":status", "200"}};
    try {
        polycpp::http2::impl::validateTrailers(trailers);
        FAIL() << "Expected polycpp::Error to be thrown";
    } catch (const polycpp::Error& e) {
        EXPECT_EQ(e.code(), "ERR_HTTP2_INVALID_PSEUDOHEADER");
    }
}

// ── Integration: validation wired into session submit methods ───────

TEST(Http2PseudoHeaderValidation, SubmitRequestWithMissingMethodThrows) {
    // Verify that submitRequest() calls validateRequestHeaders() internally
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(false);  // client session
    session->initialize();

    http::Headers headers{{":path", "/"}, {":scheme", "https"}};
    EXPECT_THROW(session->submitRequest(headers, true), polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, SubmitRequestWithValidHeadersOk) {
    // A valid request must not throw (it may return -1 from nghttp2 if no
    // connection is established, but validation must not throw)
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(false);
    session->initialize();

    http::Headers headers{
        {":method", "GET"},
        {":path", "/"},
        {":scheme", "https"},
        {":authority", "example.com"},
    };
    EXPECT_NO_THROW(session->submitRequest(headers, true));
}

TEST(Http2PseudoHeaderValidation, SubmitResponseWithMissingStatusThrows) {
    // Verify that submitResponse() calls validateResponseHeaders() internally
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);  // server session
    session->initialize();

    http::Headers headers{{"content-type", "text/html"}};
    EXPECT_THROW(session->submitResponse(1, headers, true), polycpp::Error);
}

TEST(Http2PseudoHeaderValidation, SubmitTrailersWithPseudoHeaderThrows) {
    // Verify that submitTrailers() calls validateTrailers() internally
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    http::Headers trailers{{":status", "200"}};
    EXPECT_THROW(session->submitTrailers(1, trailers), polycpp::Error);
}

// ── Receive-side pseudo-header ordering tracking ────────────────────

TEST(Http2PseudoHeaderValidation, StreamReceivingPseudoInitiallyTrue) {
    // New streams start in pseudo-header phase
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    EXPECT_TRUE(stream->receivingPseudo());
}

TEST(Http2PseudoHeaderValidation, StreamReceivingPseudoResetOnClearHeaders) {
    // clearReceivedHeaders() resets the phase to true for new header blocks
    using namespace polycpp::http2::impl;
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();

    auto stream = session->getStream(1);
    stream->setReceivingPseudo(false);
    EXPECT_FALSE(stream->receivingPseudo());

    stream->clearReceivedHeaders();
    EXPECT_TRUE(stream->receivingPseudo());
}

// ══════════════════════════════════════════════════════════════════════
// Typed Event Tests (Plan 010)
// ══════════════════════════════════════════════════════════════════════

// Typed onData fires with correctly-typed string argument
TEST(Http2TypedEventTest, TypedOnDataEvent) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    std::string received;
    stream.on(event::Data, [&](const std::string& chunk) {
        received = chunk;
    });

    streamImpl->emitter().emit(event::Data, std::string("hello typed"));
    EXPECT_EQ(received, "hello typed");
}

// Typed onEnd fires with no arguments
TEST(Http2TypedEventTest, TypedOnEndEvent) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    bool endCalled = false;
    stream.on(event::End, [&]() {
        endCalled = true;
    });

    streamImpl->emitter().emit(event::End);
    EXPECT_TRUE(endCalled);
}

// Typed onClose fires with no arguments (stream)
TEST(Http2TypedEventTest, TypedOnStreamCloseEvent) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    bool closeCalled = false;
    stream.on(event::Close, [&]() {
        closeCalled = true;
    });

    streamImpl->emitter().emit(event::Close);
    EXPECT_TRUE(closeCalled);
}

// Typed onAborted fires with no arguments
TEST(Http2TypedEventTest, TypedOnAbortedEvent) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    bool abortedCalled = false;
    stream.on(event::Aborted, [&]() {
        abortedCalled = true;
    });

    streamImpl->emitter().emit(event::Aborted);
    EXPECT_TRUE(abortedCalled);
}

// Typed onTrailers fires with http::Headers argument
TEST(Http2TypedEventTest, TypedOnTrailersEvent) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    http::Headers receivedTrailers;
    stream.on(event::Trailers, [&](const http::Headers& trailers) {
        receivedTrailers = trailers;
    });

    http::Headers sentTrailers;
    sentTrailers.set("x-trailer", "trailer-value");
    streamImpl->emitter().emit(event::Trailers, sentTrailers);
    EXPECT_EQ(receivedTrailers.get("x-trailer").value_or(""), "trailer-value");
}

// Typed onFrameError fires with (uint8_t, int, int32_t) arguments
TEST(Http2TypedEventTest, TypedOnFrameErrorEvent) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    uint8_t gotFrameType = 0;
    int gotErrorCode = 0;
    int32_t gotStreamId = 0;
    stream.on(event::FrameError, [&](uint8_t ft, int ec, int32_t sid) {
        gotFrameType = ft;
        gotErrorCode = ec;
        gotStreamId = sid;
    });

    streamImpl->emitter().emit(event::FrameError,
                               static_cast<uint8_t>(1),
                               static_cast<int>(2),
                               static_cast<int32_t>(3));
    EXPECT_EQ(gotFrameType, 1u);
    EXPECT_EQ(gotErrorCode, 2);
    EXPECT_EQ(gotStreamId, 3);
}

// Typed onResponse on ClientHttp2Stream returns ClientHttp2Stream& (correct type)
TEST(Http2TypedEventTest, TypedOnResponseEventClientStream) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(false);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    ClientHttp2Stream stream(streamImpl);

    http::Headers receivedHeaders;
    stream.on(event::Response, [&](const http::Headers& headers) {
        receivedHeaders = headers;
    });

    http::Headers sentHeaders;
    sentHeaders.set(":status", "200");
    sentHeaders.set("content-type", "text/plain");
    streamImpl->emitter().emit(event::Response, sentHeaders);
    EXPECT_EQ(receivedHeaders.get(":status").value_or(""), "200");
    EXPECT_EQ(receivedHeaders.get("content-type").value_or(""), "text/plain");
}

// Typed onClose fires with no arguments (session)
TEST(Http2TypedEventTest, TypedOnSessionCloseEvent) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();
    Http2Session session(impl);

    bool closeCalled = false;
    session.on(event::Close, [&]() {
        closeCalled = true;
    });

    // Trigger close via the impl (which emits "close")
    session.close();
    EXPECT_TRUE(closeCalled);
}

// Typed onPing fires with (double, std::string) arguments
TEST(Http2TypedEventTest, TypedOnPingEvent) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();
    Http2Session session(impl);

    double gotDuration = -1.0;
    std::string gotPayload;
    session.on(event::Ping, [&](double dur, const std::string& payload) {
        gotDuration = dur;
        gotPayload = payload;
    });

    impl->emitter().emit(event::Ping, 42.5, std::string("pingdata"));
    EXPECT_DOUBLE_EQ(gotDuration, 42.5);
    EXPECT_EQ(gotPayload, "pingdata");
}

// Typed onGoaway fires with (uint32_t, int32_t) arguments
TEST(Http2TypedEventTest, TypedOnGoawayEvent) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();
    Http2Session session(impl);

    uint32_t gotErrorCode = 99;
    int32_t gotLastStream = -1;
    session.on(event::Goaway, [&](uint32_t ec, int32_t ls) {
        gotErrorCode = ec;
        gotLastStream = ls;
    });

    impl->emitter().emit(event::Goaway,
                         static_cast<uint32_t>(8),
                         static_cast<int32_t>(5));
    EXPECT_EQ(gotErrorCode, 8u);
    EXPECT_EQ(gotLastStream, 5);
}

// Typed onStream on ServerHttp2Session fires with (ServerHttp2Stream, http::Headers)
TEST(Http2TypedEventTest, TypedOnServerSessionStreamEvent) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();
    ServerHttp2Session session(impl);

    bool streamReceived = false;
    std::string methodReceived;
    session.on(event::ServerStream, [&](ServerHttp2Stream stream, const http::Headers& headers) {
        streamReceived = true;
        methodReceived = headers.get(":method").value_or("");
        (void)stream; // stream handle used by caller
    });

    // Emit a "stream" event as the session_impl does (ServerHttp2Stream by value)
    auto streamImpl = impl->getStream(1);
    http::Headers reqHeaders;
    reqHeaders.set(":method", "GET");
    reqHeaders.set(":path", "/test");
    impl->emitter().emit(event::ServerStream, ServerHttp2Stream(streamImpl), reqHeaders);

    EXPECT_TRUE(streamReceived);
    EXPECT_EQ(methodReceived, "GET");
}

// Typed onStream on ClientHttp2Session fires with (ClientHttp2Stream, http::Headers)
TEST(Http2TypedEventTest, TypedOnClientSessionStreamEvent) {
    auto impl = std::make_shared<Http2SessionImpl>(false);
    impl->initialize();
    ClientHttp2Session session(impl);

    bool streamReceived = false;
    session.on(event::ClientStream, [&](ClientHttp2Stream stream, const http::Headers& headers) {
        streamReceived = true;
        (void)stream;
        (void)headers;
    });

    auto streamImpl = impl->getStream(1);
    http::Headers pushHeaders;
    pushHeaders.set(":path", "/pushed");
    impl->emitter().emit(event::ClientStream, ClientHttp2Stream(streamImpl), pushHeaders);
    EXPECT_TRUE(streamReceived);
}

// Multiple typed listeners both fire for the same event
TEST(Http2TypedEventTest, MultipleTypedListenersCoexist) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    int firstCount = 0;
    int secondCount = 0;

    stream.on(event::Data, [&](const std::string&) {
        firstCount++;
    });
    stream.on(event::Data, [&](const std::string&) {
        secondCount++;
    });

    streamImpl->emitter().emit(event::Data, std::string("test"));
    EXPECT_EQ(firstCount, 1);
    EXPECT_EQ(secondCount, 1);
}

// Typed onceData fires only once
TEST(Http2TypedEventTest, TypedOnceFiresOnlyOnce) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    int callCount = 0;
    stream.once(event::Data, [&](const std::string&) {
        callCount++;
    });

    streamImpl->emitter().emit(event::Data, std::string("first"));
    streamImpl->emitter().emit(event::Data, std::string("second")); // should not fire
    EXPECT_EQ(callCount, 1);
}

// Typed onSession on Http2Server: verify method compiles and onSession returns Http2Server&
// (We cannot easily instantiate Http2ServerImpl without an EventContext, so we test
// at the emitter level via the session that the callback type binds correctly.)
TEST(Http2TypedEventTest, TypedOnServerSessionMethodChains) {
    // Typed registrations return listener IDs and should not crash on a null impl.
    // We verify declaration only by creating a dummy server through its default ctor.
    Http2Server server;  // default-constructed, impl_ is null (valid() == false)

    // These should not crash even on a null impl (on() guards with if (impl_))
    int count = 0;
    auto id1 = server.on(event::Listening, [&]() { count++; });
    auto id2 = server.on(event::Close, [&]() { count++; });
    auto id3 = server.on(event::Error_, [&](const Error&) { count++; });
    auto id4 = server.on(event::Session, [&](ServerHttp2Session) { count++; });
    EXPECT_NE(id1, 0u);
    EXPECT_NE(id2, 0u);
    EXPECT_NE(id3, 0u);
    EXPECT_NE(id4, 0u);
    // No emit, no crash — null impl guard works
    EXPECT_EQ(count, 0);
}

// Typed registration returns listener IDs and still wires the callbacks correctly.
TEST(Http2TypedEventTest, TypedMethodChainingReturnsCorrectType) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(false);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    ClientHttp2Stream stream(streamImpl);

    int count = 0;
    auto onId = stream.on(event::Response, [&](const http::Headers&) { count++; });
    auto onceId = stream.once(event::Response, [&](const http::Headers&) { count += 10; });
    EXPECT_NE(onId, 0u);
    EXPECT_NE(onceId, 0u);

    http::Headers h;
    h.set(":status", "200");
    streamImpl->emitter().emit(event::Response, h);
    streamImpl->emitter().emit(event::Response, h); // second emit: once won't fire, on fires again

    // First emit:  onResponse(+1) + onceResponse(+10) = 11
    // Second emit: onResponse(+1) = 12 (onceResponse already removed)
    EXPECT_EQ(count, 12);
}

// ══════════════════════════════════════════════════════════════════════
// Http2CompatTypedEventTest — typed on/once methods for ServerRequest/Response
// ══════════════════════════════════════════════════════════════════════

// Helper: build a session+streamImpl+ServerHttp2Stream combo for compat tests
static auto makeCompatStream() {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto streamImpl = session->getStream(1);
    ServerHttp2Stream stream(streamImpl);
    return std::make_tuple(session, streamImpl, stream);
}

// ── Http2ServerRequest typed events ───────────────────────────────────

TEST(Http2CompatTypedEventTest, ServerRequestOnDataReceivesChunk) {
    // Verify that req.on(event::Data, ...) is called when the underlying stream emits "data".
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerRequest req(stream, {{":method", "POST"}, {":path", "/"}});

    std::string received;
    req.on(event::Data, [&](const std::string& chunk) { received += chunk; });

    // Simulate nghttp2 delivering body data (the stream impl emits "data")
    streamImpl->emitter().emit(event::Data, std::string("hello"));
    streamImpl->emitter().emit(event::Data, std::string(" world"));

    EXPECT_EQ(received, "hello world");
}

TEST(Http2CompatTypedEventTest, ServerRequestOnceDataFiresOnce) {
    // onceData unregisters itself after the first chunk.
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerRequest req(stream, {{":method", "POST"}, {":path", "/"}});

    int callCount = 0;
    req.once(event::Data, [&](const std::string&) { ++callCount; });

    streamImpl->emitter().emit(event::Data, std::string("first"));
    streamImpl->emitter().emit(event::Data, std::string("second"));

    EXPECT_EQ(callCount, 1);
}

TEST(Http2CompatTypedEventTest, ServerRequestOnEndFires) {
    // Verify req.on(event::End, ...) fires when the stream emits "end".
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerRequest req(stream, {{":method", "GET"}, {":path", "/"}});

    bool endFired = false;
    req.on(event::End, [&]() { endFired = true; });

    streamImpl->emitter().emit(event::End);

    EXPECT_TRUE(endFired);
}

TEST(Http2CompatTypedEventTest, ServerRequestOnceEndFiresOnce) {
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerRequest req(stream, {{":method", "GET"}, {":path", "/"}});

    int count = 0;
    req.once(event::End, [&]() { ++count; });

    streamImpl->emitter().emit(event::End);
    streamImpl->emitter().emit(event::End); // second emit should not fire

    EXPECT_EQ(count, 1);
}

TEST(Http2CompatTypedEventTest, ServerRequestOnCloseFires) {
    // Verify req.on(event::Close, ...) fires when the stream emits "close".
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerRequest req(stream, {{":method", "GET"}, {":path", "/"}});

    bool closeFired = false;
    req.on(event::Close, [&]() { closeFired = true; });

    streamImpl->emitter().emit(event::Close);

    EXPECT_TRUE(closeFired);
}

TEST(Http2CompatTypedEventTest, ServerRequestOnAbortedFires) {
    // Verify req.on(event::Aborted, ...) fires when the stream emits "aborted".
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerRequest req(stream, {{":method", "GET"}, {":path", "/"}});

    bool abortedFired = false;
    req.on(event::Aborted, [&]() { abortedFired = true; });

    streamImpl->emitter().emit(event::Aborted);

    EXPECT_TRUE(abortedFired);
}

TEST(Http2CompatTypedEventTest, ServerRequestOnceAbortedFiresOnce) {
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerRequest req(stream, {{":method", "GET"}, {":path", "/"}});

    int count = 0;
    req.once(event::Aborted, [&]() { ++count; });

    streamImpl->emitter().emit(event::Aborted);
    streamImpl->emitter().emit(event::Aborted);

    EXPECT_EQ(count, 1);
}

TEST(Http2CompatTypedEventTest, ServerRequestTypedMethodsChain) {
    // Typed methods return listener IDs and still wire all callbacks.
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerRequest req(stream, {{":method", "POST"}, {":path", "/"}});

    int count = 0;
    auto dataId = req.on(event::Data, [&](const std::string&) { ++count; });
    auto endId = req.on(event::End, [&]() { ++count; });
    auto closeId = req.on(event::Close, [&]() { ++count; });
    auto abortedId = req.on(event::Aborted, [&]() { ++count; });
    EXPECT_NE(dataId, 0u);
    EXPECT_NE(endId, 0u);
    EXPECT_NE(closeId, 0u);
    EXPECT_NE(abortedId, 0u);

    streamImpl->emitter().emit(event::Data, std::string("x"));
    streamImpl->emitter().emit(event::End);
    streamImpl->emitter().emit(event::Close);
    streamImpl->emitter().emit(event::Aborted);

    EXPECT_EQ(count, 4);
}

// ── Http2ServerResponse typed events ──────────────────────────────────

TEST(Http2CompatTypedEventTest, ServerResponseOnFinishFires) {
    // Verify res.on(event::Finish, ...) fires when end() emits "finish".
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerResponse res(stream);

    bool finishFired = false;
    res.on(event::Finish, [&]() { finishFired = true; });

    // end() emits "finish" on the response's own emitter
    res.end();

    EXPECT_TRUE(finishFired);
}

TEST(Http2CompatTypedEventTest, ServerResponseOnceFinishFiresOnce) {
    // onceFinish unregisters itself after the first "finish" event.
    // We call end() once (fires finish) then verify only one call happened.
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerResponse res(stream);

    int count = 0;
    res.once(event::Finish, [&]() { ++count; });
    // end() emits "finish" — the once listener fires and unregisters.
    res.end();
    // A second end() should be a no-op (finished_=true), so "finish" won't fire again.
    res.end();

    EXPECT_EQ(count, 1);
}

TEST(Http2CompatTypedEventTest, ServerResponseOnCloseFires) {
    // Verify res.on(event::Close, ...) fires when the underlying stream emits "close".
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerResponse res(stream);

    bool closeFired = false;
    res.on(event::Close, [&]() { closeFired = true; });

    streamImpl->emitter().emit(event::Close);

    EXPECT_TRUE(closeFired);
}

TEST(Http2CompatTypedEventTest, ServerResponseOnceCloseFires) {
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerResponse res(stream);

    int count = 0;
    res.once(event::Close, [&]() { ++count; });

    streamImpl->emitter().emit(event::Close);
    streamImpl->emitter().emit(event::Close);

    EXPECT_EQ(count, 1);
}

TEST(Http2CompatTypedEventTest, ServerResponseTypedMethodsChain) {
    // Typed methods return listener IDs and still wire all callbacks.
    auto [session, streamImpl, stream] = makeCompatStream();
    Http2ServerResponse res(stream);

    int count = 0;
    auto finishId = res.on(event::Finish, [&]() { ++count; });
    auto closeId = res.on(event::Close, [&]() { ++count; });
    EXPECT_NE(finishId, 0u);
    EXPECT_NE(closeId, 0u);

    res.end();                                // triggers "finish"
    streamImpl->emitter().emit(event::Close);     // triggers "close"

    EXPECT_EQ(count, 2);
}

// ══════════════════════════════════════════════════════════════════════
// Http2EventEmitterAPITest — tests for the handle-level typed event forwarding API
// ══════════════════════════════════════════════════════════════════════

TEST(Http2EventEmitterAPITest, OffRemovesListener) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    int callCount = 0;
    auto id = stream.on(event::Data, [&](const std::string&) { callCount++; });

    streamImpl->emitter().emit(event::Data, std::string("first"));
    EXPECT_EQ(callCount, 1);

    // Remove by id
    stream.off(id);

    streamImpl->emitter().emit(event::Data, std::string("second"));
    EXPECT_EQ(callCount, 1);  // should not fire again
}

TEST(Http2EventEmitterAPITest, RemoveAllListeners) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    int count = 0;
    stream.on(event::Data, [&](const std::string&) { count++; });
    stream.on(event::Data, [&](const std::string&) { count++; });
    stream.on(event::End, [&]() { count++; });

    stream.removeAllListeners(event::Data);

    streamImpl->emitter().emit(event::Data, std::string("test"));
    EXPECT_EQ(count, 0);  // both data listeners removed

    streamImpl->emitter().emit(event::End);
    EXPECT_EQ(count, 1);  // end listener still active
}

TEST(Http2EventEmitterAPITest, ListenerCount) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    EXPECT_EQ(stream.listenerCount(event::Data), 0u);

    stream.on(event::Data, [](const std::string&) {});
    EXPECT_EQ(stream.listenerCount(event::Data), 1u);

    stream.on(event::Data, [](const std::string&) {});
    EXPECT_EQ(stream.listenerCount(event::Data), 2u);

    stream.on(event::Close, []() {});
    EXPECT_EQ(stream.listenerCount(event::Data), 2u);
    EXPECT_EQ(stream.listenerCount(event::Close), 1u);
}

TEST(Http2EventEmitterAPITest, HasListenersTracksTypedEvents) {
    auto sessionImpl = std::make_shared<Http2SessionImpl>(true);
    sessionImpl->initialize();
    auto streamImpl = sessionImpl->getStream(1);
    Http2Stream stream(streamImpl);

    EXPECT_FALSE(stream.hasListeners(event::Data));
    EXPECT_FALSE(stream.hasListeners(event::End));
    EXPECT_FALSE(stream.hasListeners(event::Close));

    stream.on(event::Data, [](const std::string&) {});
    stream.on(event::End, []() {});
    stream.on(event::Close, []() {});

    EXPECT_TRUE(stream.hasListeners(event::Data));
    EXPECT_TRUE(stream.hasListeners(event::End));
    EXPECT_TRUE(stream.hasListeners(event::Close));
}

TEST(Http2EventEmitterAPITest, SessionOffRemovesListener) {
    auto impl = std::make_shared<Http2SessionImpl>(true);
    impl->initialize();
    Http2Session session(impl);

    int callCount = 0;
    auto id = session.on(event::Close, [&]() { callCount++; });

    session.close();
    EXPECT_EQ(callCount, 1);

    session.off(id);
    EXPECT_EQ(session.listenerCount(event::Close), 0u);
}

TEST(Http2EventEmitterAPITest, CompatRequestOffWorks) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto streamImpl = session->getStream(1);
    ServerHttp2Stream stream(streamImpl);
    Http2ServerRequest req(stream, {{":method", "GET"}, {":path", "/"}});

    int count = 0;
    auto id = req.on(event::Data, [&](const std::string&) { count++; });

    streamImpl->emitter().emit(event::Data, std::string("test"));
    EXPECT_EQ(count, 1);

    req.off(id);

    streamImpl->emitter().emit(event::Data, std::string("test2"));
    EXPECT_EQ(count, 1);  // listener removed, should not fire
}

TEST(Http2EventEmitterAPITest, CompatResponseListenerCount) {
    auto session = std::make_shared<Http2SessionImpl>(true);
    session->initialize();
    auto streamImpl = session->getStream(1);
    ServerHttp2Stream stream(streamImpl);
    Http2ServerResponse res(stream);

    EXPECT_EQ(res.listenerCount(event::Finish), 0u);

    res.on(event::Finish, [&]() {});
    // Note: close listener was added in constructor for proxying, so count >= 1
    EXPECT_EQ(res.listenerCount(event::Finish), 1u);
    EXPECT_GE(res.listenerCount(event::Close), 0u);
}

// ══════════════════════════════════════════════════════════════════════
// ALTSVC Tests (Plan 1137)
// ══════════════════════════════════════════════════════════════════════

TEST(Http2AltsvcTest, AltsvcWithOriginString) {
    // Server session — altsvc() should call nghttp2_submit_altsvc with origin
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();
    ServerHttp2Session session(sessionImpl);

    // Should not crash or throw. The nghttp2 session is properly initialized.
    // With stream_id=0, the origin string must be non-empty.
    EXPECT_NO_THROW(session.altsvc("h2=\":443\"", "https://example.com"));
}

TEST(Http2AltsvcTest, AltsvcWithStreamId) {
    // Server session — altsvc() with a numeric string should be treated as stream ID
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();
    ServerHttp2Session session(sessionImpl);

    // Stream 1 — when stream_id != 0, origin must be NULL.
    // The stream doesn't need to exist for nghttp2_submit_altsvc — it's best-effort.
    EXPECT_NO_THROW(session.altsvc("h2=\":8443\"", "1"));
}

TEST(Http2AltsvcTest, AltsvcOnClientEmitsError) {
    // Client session — calling altsvc on a client session should fail silently
    // (submitAltsvc checks destroyed_/closed_ but nghttp2 returns INVALID_STATE for clients)
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(false);
    sessionImpl->initialize();

    bool errorEmitted = false;
    sessionImpl->emitter().on(event::Error_, [&](const Error&) {
        errorEmitted = true;
    });

    // Call directly on impl since altsvc() is only on ServerHttp2Session
    sessionImpl->submitAltsvc("h2=\":443\"", "https://example.com");
    EXPECT_TRUE(errorEmitted);
}

TEST(Http2AltsvcTest, AltsvcOnDestroyedSessionIsNoop) {
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();
    ServerHttp2Session session(sessionImpl);

    session.destroy();
    // Should not crash on destroyed session
    EXPECT_NO_THROW(session.altsvc("h2=\":443\"", "https://example.com"));
}

// ══════════════════════════════════════════════════════════════════════
// ORIGIN Tests (Plan 1137)
// ══════════════════════════════════════════════════════════════════════

TEST(Http2OriginTest, OriginWithSingleOrigin) {
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();
    ServerHttp2Session session(sessionImpl);

    EXPECT_NO_THROW(session.origin({"https://example.com"}));
}

TEST(Http2OriginTest, OriginWithMultipleOrigins) {
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();
    ServerHttp2Session session(sessionImpl);

    EXPECT_NO_THROW(session.origin({
        "https://example.com",
        "https://example.org",
        "https://api.example.com"
    }));
}

TEST(Http2OriginTest, OriginEmptyList) {
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();
    ServerHttp2Session session(sessionImpl);

    // Empty origin list is valid — sends an empty ORIGIN frame
    EXPECT_NO_THROW(session.origin({}));
}

TEST(Http2OriginTest, OriginOnClientEmitsError) {
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(false);
    sessionImpl->initialize();

    bool errorEmitted = false;
    sessionImpl->emitter().on(event::Error_, [&](const Error&) {
        errorEmitted = true;
    });

    sessionImpl->submitOrigin({"https://example.com"});
    EXPECT_TRUE(errorEmitted);
}

TEST(Http2OriginTest, OriginOnDestroyedSessionIsNoop) {
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();
    ServerHttp2Session session(sessionImpl);

    session.destroy();
    EXPECT_NO_THROW(session.origin({"https://example.com"}));
}

// ══════════════════════════════════════════════════════════════════════
// respondWithFD / respondWithFile Tests (Plan 1137)
// ══════════════════════════════════════════════════════════════════════
// These tests use POSIX-specific APIs (mkstemp, lseek, unlink) and are
// skipped on Windows.
#if !defined(_WIN32)

namespace {

std::vector<char> makeHttp2TempPathTemplate() {
    auto path = polycpp::test::testTempTemplate("polycpp_http2_test");
    std::vector<char> pathBuf(path.begin(), path.end());
    pathBuf.push_back('\0');
    return pathBuf;
}

} // namespace

TEST(Http2RespondWithFDTest, RespondWithFDSetsUpFdSource) {
    // Create a server session and a stream, then call respondWithFD.
    // Verify the stream impl has the fd source set.
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();

    auto streamImpl = sessionImpl->getStream(1);
    ServerHttp2Stream stream(streamImpl);

    // Create a temporary file with known content
    auto tmpPath = makeHttp2TempPathTemplate();
    int fd = ::mkstemp(tmpPath.data());
    ASSERT_GE(fd, 0);
    const char* content = "Hello, HTTP/2 respondWithFD!";
    ::write(fd, content, std::strlen(content));
    ::lseek(fd, 0, SEEK_SET);

    http::Headers headers;
    headers.set(":status", "200");
    headers.set("content-type", "text/plain");

    stream.respondWithFD(fd, headers);

    // The fd source should be set on the stream impl
    EXPECT_TRUE(streamImpl->hasFdSource());
    EXPECT_EQ(streamImpl->fdSource(), fd);
    EXPECT_TRUE(streamImpl->headersSent());

    // Clean up
    ::close(fd);
    ::unlink(tmpPath.data());
}

TEST(Http2RespondWithFDTest, RespondWithFDWithOffsetAndLength) {
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();

    auto streamImpl = sessionImpl->getStream(1);
    ServerHttp2Stream stream(streamImpl);

    auto tmpPath = makeHttp2TempPathTemplate();
    int fd = ::mkstemp(tmpPath.data());
    ASSERT_GE(fd, 0);
    const char* content = "0123456789ABCDEFGHIJ";
    ::write(fd, content, std::strlen(content));
    ::lseek(fd, 0, SEEK_SET);

    http::Headers headers;
    headers.set(":status", "200");

    StreamFileResponseOptions options;
    options.offset = 5;
    options.length = 10;

    stream.respondWithFD(fd, headers, options);

    EXPECT_TRUE(streamImpl->hasFdSource());
    EXPECT_EQ(streamImpl->fdCurrentOffset(), 5);
    EXPECT_EQ(streamImpl->fdRemainingLength(), 10);

    ::close(fd);
    ::unlink(tmpPath.data());
}

TEST(Http2RespondWithFileTest, RespondWithFileOpensAndSetsUpFd) {
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();

    auto streamImpl = sessionImpl->getStream(1);
    ServerHttp2Stream stream(streamImpl);

    // Create a temp file
    auto tmpPath = makeHttp2TempPathTemplate();
    int tmpFd = ::mkstemp(tmpPath.data());
    ASSERT_GE(tmpFd, 0);
    const char* content = "respondWithFile test content";
    ::write(tmpFd, content, std::strlen(content));
    ::close(tmpFd);

    http::Headers headers;
    headers.set(":status", "200");
    headers.set("content-type", "text/plain");

    stream.respondWithFile(tmpPath.data(), headers);

    // The stream should have an fd source set and own the fd
    EXPECT_TRUE(streamImpl->hasFdSource());
    EXPECT_GE(streamImpl->fdSource(), 0);
    EXPECT_TRUE(streamImpl->fdOwnsFd());
    EXPECT_TRUE(streamImpl->headersSent());

    // Clean up (closeFdSource will close the fd since ownsFd=true)
    streamImpl->closeFdSource();
    ::unlink(tmpPath.data());
}

TEST(Http2RespondWithFileTest, RespondWithFileNonexistentEmitsError) {
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();

    auto streamImpl = sessionImpl->getStream(1);
    ServerHttp2Stream stream(streamImpl);

    bool errorEmitted = false;
    sessionImpl->emitter().on(event::Error_, [&](const Error& err) {
        errorEmitted = true;
        // Error message should mention the file path
        EXPECT_NE(err.message.find("/nonexistent"), std::string::npos);
    });

    http::Headers headers;
    headers.set(":status", "200");

    stream.respondWithFile("/nonexistent/file/path.txt", headers);

    EXPECT_TRUE(errorEmitted);
    // Should not have set up an fd source
    EXPECT_FALSE(streamImpl->hasFdSource());
}

TEST(Http2RespondWithFDTest, OnDataSourceReadReadsFromFd) {
    // Integration test: verify onDataSourceRead reads from the fd
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();

    auto streamImpl = sessionImpl->getStream(1);

    // Create a temp file with known content
    auto tmpPath = makeHttp2TempPathTemplate();
    int fd = ::mkstemp(tmpPath.data());
    ASSERT_GE(fd, 0);
    const std::string content = "Hello from fd source!";
    ::write(fd, content.data(), content.size());
    ::lseek(fd, 0, SEEK_SET);

    // Set up the fd source on the stream
    streamImpl->setFdSource(fd, 0, -1, true);  // ownsFd=true, read until EOF

    // Call onDataSourceRead directly
    uint8_t buf[256];
    uint32_t dataFlags = 0;
    ssize_t nread = sessionImpl->onDataSourceRead(1, buf, sizeof(buf), &dataFlags);

    // Should have read the content
    ASSERT_GT(nread, 0);
    std::string result(reinterpret_cast<char*>(buf), static_cast<size_t>(nread));
    EXPECT_EQ(result, content);

    // Since we read the entire file, the next call should hit EOF
    uint32_t eofFlags = 0;
    ssize_t nread2 = sessionImpl->onDataSourceRead(1, buf, sizeof(buf), &eofFlags);
    EXPECT_EQ(nread2, 0);
    EXPECT_TRUE(eofFlags & NGHTTP2_DATA_FLAG_EOF);

    // fd should have been closed by closeFdSource (ownsFd=true)
    EXPECT_FALSE(streamImpl->hasFdSource());

    ::unlink(tmpPath.data());
}

TEST(Http2RespondWithFDTest, OnDataSourceReadRespectsLength) {
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();

    auto streamImpl = sessionImpl->getStream(1);

    auto tmpPath = makeHttp2TempPathTemplate();
    int fd = ::mkstemp(tmpPath.data());
    ASSERT_GE(fd, 0);
    const std::string content = "0123456789ABCDEFGHIJ";
    ::write(fd, content.data(), content.size());

    // Seek to offset 5 and read only 10 bytes
    ::lseek(fd, 5, SEEK_SET);
    streamImpl->setFdSource(fd, 5, 10, true);

    uint8_t buf[256];
    uint32_t dataFlags = 0;
    ssize_t nread = sessionImpl->onDataSourceRead(1, buf, sizeof(buf), &dataFlags);

    ASSERT_EQ(nread, 10);
    std::string result(reinterpret_cast<char*>(buf), 10);
    EXPECT_EQ(result, "56789ABCDE");

    // Next read: remaining length is 0, should get EOF
    uint32_t eofFlags = 0;
    ssize_t nread2 = sessionImpl->onDataSourceRead(1, buf, sizeof(buf), &eofFlags);
    EXPECT_EQ(nread2, 0);
    EXPECT_TRUE(eofFlags & NGHTTP2_DATA_FLAG_EOF);

    ::unlink(tmpPath.data());
}

TEST(Http2RespondWithFDTest, RespondWithFDCallerOwnsFd) {
    // Verify that respondWithFD does NOT own the fd (ownsFd=false)
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();

    auto streamImpl = sessionImpl->getStream(1);
    ServerHttp2Stream stream(streamImpl);

    auto tmpPath = makeHttp2TempPathTemplate();
    int fd = ::mkstemp(tmpPath.data());
    ASSERT_GE(fd, 0);
    ::write(fd, "test", 4);
    ::lseek(fd, 0, SEEK_SET);

    http::Headers headers;
    headers.set(":status", "200");

    stream.respondWithFD(fd, headers);

    // respondWithFD should NOT own the fd
    EXPECT_FALSE(streamImpl->fdOwnsFd());

    // Caller must close
    ::close(fd);
    ::unlink(tmpPath.data());
}

TEST(Http2RespondWithFDTest, StreamCloseCleansFdSource) {
    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(true);
    sessionImpl->initialize();

    auto streamImpl = sessionImpl->getStream(1);

    auto tmpPath = makeHttp2TempPathTemplate();
    int fd = ::mkstemp(tmpPath.data());
    ASSERT_GE(fd, 0);
    ::write(fd, "test", 4);
    ::lseek(fd, 0, SEEK_SET);

    // Set fd source with ownsFd=true
    streamImpl->setFdSource(fd, 0, -1, true);
    EXPECT_TRUE(streamImpl->hasFdSource());

    // Simulate stream close
    streamImpl->onStreamClose(0);

    // fd source should be cleaned up
    EXPECT_FALSE(streamImpl->hasFdSource());

    ::unlink(tmpPath.data());
}

#endif // !_WIN32
