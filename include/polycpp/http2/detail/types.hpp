#pragma once

/**
 * @file types.hpp
 * @brief HTTP/2 type definitions: Settings, Options, State structs.
 *
 * All option and state types matching Node.js's HTTP/2 API surface.
 *
 * @see https://nodejs.org/api/http2.html
 */

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace polycpp {

// Forward declarations
class Error;
namespace buffer { class Buffer; }

namespace http2 {

// ── HTTP/2 Settings ────────────────────────────────────────────────────

/**
 * @brief HTTP/2 settings matching Node.js `http2.Settings`.
 *
 * Each field is optional; only set fields are serialized to the wire.
 *
 * @see https://nodejs.org/api/http2.html#settings-object
 */
struct Settings {
    std::optional<uint32_t> headerTableSize;        ///< 0-4294967295, default 4096
    std::optional<bool>     enablePush;              ///< default true
    std::optional<uint32_t> initialWindowSize;       ///< 0-2147483647, default 65535
    std::optional<uint32_t> maxFrameSize;            ///< 16384-16777215, default 16384
    std::optional<uint32_t> maxConcurrentStreams;     ///< 0-4294967295
    std::optional<uint32_t> maxHeaderListSize;        ///< 0-4294967295
    std::optional<bool>     enableConnectProtocol;   ///< default false
};

// ── Http2ErrorCode — Strong enum for HTTP/2 error codes ───────────────

/**
 * @brief HTTP/2 error codes per RFC 9113 Section 7.
 *
 * Provides strongly-typed error code values matching the NGHTTP2_*
 * constants. Use with `Http2Session::destroy()`, `Http2Stream::close()`,
 * and `Http2Session::goaway()`.
 *
 * @see https://www.rfc-editor.org/rfc/rfc9113#section-7
 */
enum class Http2ErrorCode : uint32_t {
    NoError            = 0x0,  ///< No error.
    ProtocolError      = 0x1,  ///< Protocol error.
    InternalError      = 0x2,  ///< Internal error.
    FlowControlError   = 0x3,  ///< Flow control error.
    SettingsTimeout    = 0x4,  ///< SETTINGS timeout.
    StreamClosed       = 0x5,  ///< Stream closed.
    FrameSizeError     = 0x6,  ///< Frame size error.
    RefusedStream      = 0x7,  ///< Stream refused.
    Cancel             = 0x8,  ///< Stream canceled.
    CompressionError   = 0x9,  ///< Compression error.
    ConnectError       = 0xa,  ///< CONNECT error.
    EnhanceYourCalm    = 0xb,  ///< Enhance your calm.
    InadequateSecurity = 0xc,  ///< Inadequate security.
    Http11Required     = 0xd   ///< HTTP/1.1 required.
};

// ── PaddingStrategy — Strong enum for HTTP/2 padding strategy ─────────

/**
 * @brief HTTP/2 padding strategy.
 *
 * Controls how padding bytes are added to DATA and HEADERS frames.
 *
 * @see https://nodejs.org/api/http2.html#http2constants
 */
enum class PaddingStrategy : int {
    None     = 0,  ///< No padding
    Max      = 1,  ///< Always pad to max frame size
    Callback = 2   ///< Use a user-supplied callback
};

// ── Http2StreamState — Strong enum for stream states per RFC 9113 ─────

/**
 * @brief HTTP/2 stream states per RFC 9113 Section 5.1.
 *
 * @see https://www.rfc-editor.org/rfc/rfc9113#section-5.1
 */
enum class Http2StreamState : int {
    Idle             = 0,  ///< Stream has not been opened yet.
    Open             = 1,  ///< Stream is open in both directions.
    ReservedLocal    = 2,  ///< Locally reserved for a future push.
    ReservedRemote   = 3,  ///< Remotely reserved for a future push.
    HalfClosedLocal  = 4,  ///< Local side has closed its send half.
    HalfClosedRemote = 5,  ///< Remote side has closed its send half.
    Closed           = 6   ///< Stream is fully closed.
};

// ── Session Options ────────────────────────────────────────────────────

/**
 * @brief Base session configuration shared by server and client.
 *
 * @see https://nodejs.org/api/http2.html#http2createsecureserveroptions-onrequesthandler
 */
struct SessionOptions {
    std::optional<uint32_t> maxDeflateDynamicTableSize;   ///< default 4096
    std::optional<uint32_t> maxSettings;                   ///< min 1, default 32
    std::optional<uint32_t> maxSessionMemory;              ///< MB, min 1, default 10
    std::optional<uint32_t> maxHeaderListPairs;            ///< min 1, default 128
    std::optional<uint32_t> maxOutstandingPings;           ///< default 10
    std::optional<uint32_t> maxSendHeaderBlockLength;      ///< max serialized header block
    std::optional<PaddingStrategy> paddingStrategy;          ///< PADDING_STRATEGY_*
    std::optional<uint32_t> peerMaxConcurrentStreams;       ///< default 100
    std::optional<Settings> settings;                       ///< initial settings to send
    std::optional<std::vector<int32_t>> remoteCustomSettings; ///< custom setting IDs
    std::optional<uint32_t> unknownProtocolTimeout;        ///< ms, default 10000
};

// ── Server Options ─────────────────────────────────────────────────────

/**
 * @brief Options for http2.createServer().
 * @see https://nodejs.org/api/http2.html#http2createserveroptions-onrequesthandler
 */
struct ServerOptions : public SessionOptions {
    // Inherits all SessionOptions fields
};

// ── Secure Server Options ──────────────────────────────────────────────

/**
 * @brief Options for http2.createSecureServer().
 *
 * Includes TLS-shaped options for the secure server.
 *
 * Current implementation notes:
 * - `cert`, `key`, and `ca` are forwarded into the internal TLS context.
 * - `pfx`, `passphrase`, `requestCert`, `rejectUnauthorized`,
 *   `ALPNProtocols`, and `allowHTTP1` are currently accepted as part of the
 *   public options shape but are not wired into server behavior yet.
 *
 * @see https://nodejs.org/api/http2.html#http2createsecureserveroptions-onrequesthandler
 */
struct SecureServerOptions : public ServerOptions {
    std::optional<std::string> cert;                ///< TLS certificate (PEM)
    std::optional<std::string> key;                 ///< TLS private key (PEM)
    std::optional<std::string> ca;                  ///< CA certificate (PEM)
    std::optional<std::string> pfx;                 ///< PKCS#12 bundle
    std::optional<std::string> passphrase;          ///< Private key passphrase
    std::optional<bool>        requestCert;         ///< Request client certificate
    std::optional<bool>        rejectUnauthorized;  ///< Reject unauthorized clients
    std::optional<std::vector<std::string>> ALPNProtocols;  ///< ALPN protocols
    std::optional<bool>        allowHTTP1;          ///< Allow HTTP/1.1 fallback
};

// ── Client Session Options ─────────────────────────────────────────────

/**
 * @brief Options for http2.connect().
 *
 * Current implementation notes:
 * - The connection path derives host/port/scheme from the `authority`
 *   string.
 * - `protocol` and `maxReservedRemoteStreams` are currently stored in the
 *   public options type but not consulted by the base `connect()` transport
 *   path.
 *
 * @see https://nodejs.org/api/http2.html#http2connectauthority-options-listener
 */
struct ClientSessionOptions : public SessionOptions {
    std::optional<uint32_t> maxReservedRemoteStreams;   ///< default 200
    std::optional<std::string> protocol;                 ///< "http:" or "https:", default "https:"
};

// ── Secure Client Session Options ──────────────────────────────────────

/**
 * @brief Options for http2.connect() with TLS.
 *
 * Current implementation notes:
 * - This library currently routes the secure overload through the same plain
 *   TCP connection path as the base overload.
 * - `ca`, `cert`, `key`, `rejectUnauthorized`, and `servername` are exposed
 *   for API-shape parity but are not yet wired into transport setup.
 * - `protocol` and `maxReservedRemoteStreams` inherit the same limitations as
 *   `ClientSessionOptions`.
 *
 * @see https://nodejs.org/api/http2.html#http2connectauthority-options-listener
 */
struct SecureClientSessionOptions : public ClientSessionOptions {
    std::optional<std::string> ca;                  ///< CA certificate (PEM)
    std::optional<std::string> cert;                ///< TLS certificate (PEM)
    std::optional<std::string> key;                 ///< TLS private key (PEM)
    std::optional<bool>        rejectUnauthorized;  ///< Reject unauthorized servers
    std::optional<std::string> servername;           ///< SNI hostname
};

// ── Stream Response Options ────────────────────────────────────────────

/**
 * @brief Options for ServerHttp2Stream.respond().
 * @see https://nodejs.org/api/http2.html#http2streamrespondheaders-options
 */
struct StreamResponseOptions {
    bool endStream = false;          ///< Close stream after headers
    bool waitForTrailers = false;    ///< Wait for trailers before closing
};

// ── Stream File Response Options ───────────────────────────────────────

/**
 * @brief Options for ServerHttp2Stream.respondWithFile().
 * @see https://nodejs.org/api/http2.html#http2streamrespondwithfilepath-headers-options
 */
struct StreamFileResponseOptions {
    bool waitForTrailers = false;
    std::optional<int64_t> offset;    ///< Byte offset in file
    std::optional<int64_t> length;    ///< Number of bytes to send
};

// ── Client Request Options ─────────────────────────────────────────────

/**
 * @brief Options for ClientHttp2Session.request().
 * @see https://nodejs.org/api/http2.html#clienthttp2sessionrequestheaders-options
 */
struct ClientRequestOptions {
    bool endStream = false;          ///< Send without body (GET-like)
    bool exclusive = false;          ///< Set exclusive dependency flag
    std::optional<int32_t> parent;   ///< Parent stream ID for prioritization
    bool waitForTrailers = false;
};

// ── Stream State ───────────────────────────────────────────────────────

/**
 * @brief HTTP/2 stream state matching Node.js `http2.StreamState`.
 *
 * Updated to use Http2StreamState enum. The `localClose` and
 * `remoteClose` fields track half-close transitions independently.
 *
 * @see https://nodejs.org/api/http2.html#http2streamstate
 */
struct StreamState {
    std::optional<int32_t>  localWindowSize;
    Http2StreamState        state = Http2StreamState::Idle;  ///< Stream state per RFC 9113
    bool                    localClose = false;    ///< Local side has sent END_STREAM
    bool                    remoteClose = false;   ///< Remote side has sent END_STREAM
};

// ── Session State ──────────────────────────────────────────────────────

/**
 * @brief HTTP/2 session state matching Node.js `http2.SessionState`.
 * @see https://nodejs.org/api/http2.html#http2sessionstate
 */
struct SessionState {
    std::optional<int32_t>  effectiveLocalWindowSize;
    std::optional<int32_t>  effectiveRecvDataLength;
    std::optional<int32_t>  nextStreamID;
    std::optional<int32_t>  localWindowSize;
    std::optional<int32_t>  lastProcStreamID;
    std::optional<int32_t>  remoteWindowSize;
    std::optional<size_t>   outboundQueueSize;
    std::optional<size_t>   deflateDynamicTableSize;
    std::optional<size_t>   inflateDynamicTableSize;
};

// ── Alternative Service Options ────────────────────────────────────────

/**
 * @brief Options for alternative service (ALTSVC frame).
 */
struct AlternativeServiceOptions {
    std::string origin;
};

} // namespace http2
} // namespace polycpp
