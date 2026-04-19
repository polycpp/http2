#pragma once

/**
 * @file http2.hpp
 * @brief HTTP/2 module declarations.
 *
 * Provides HTTP/2 client and server support using nghttp2.
 * Mimics Node.js `http2` module API.
 *
 * Public API summary:
 *   Constants: polycpp::http2::constants namespace
 *   Types:     Settings, SessionOptions, ServerOptions, SecureServerOptions,
 *              ClientSessionOptions, SecureClientSessionOptions,
 *              StreamResponseOptions, StreamFileResponseOptions,
 *              ClientRequestOptions, StreamState, SessionState
 *   Classes:   Http2Session, ServerHttp2Session, ClientHttp2Session,
 *              Http2Stream, ServerHttp2Stream, ClientHttp2Stream,
 *              Http2Server, Http2SecureServer,
 *              Http2ServerRequest, Http2ServerResponse
 *   Functions: getDefaultSettings, getPackedSettings, getUnpackedSettings,
 *              validateSettings, createServer, createSecureServer, connect
 *
 * @see https://nodejs.org/api/http2.html
 */

#include <polycpp/http2/detail/constants.hpp>
#include <polycpp/http2/detail/types.hpp>
#include <polycpp/http2/events.hpp>
#include <polycpp/events.hpp>
#include <polycpp/core/json.hpp>
#include <polycpp/http/headers.hpp>

#include <any>
#include <functional>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

// Forward-declare dependencies to avoid circular includes in declarations
namespace polycpp {
    class EventContext;
    class Error;
    namespace buffer { class Buffer; }
    namespace events { class EventEmitter; }
}

namespace polycpp {
namespace http2 {

// Forward declarations for impl classes
namespace impl {
    class Http2SessionImpl;
    class Http2StreamImpl;
    class Http2ServerImpl;
} // namespace impl

/** @defgroup http2 HTTP/2
 *  @brief HTTP/2 server and client classes and utilities.
 *  @{ */

// ── Pseudo-header structs ─────────────────────────────────────────────

/**
 * @brief Parsed HTTP/2 request pseudo-headers.
 *
 * Separates pseudo-headers (:method, :path, :scheme, :authority) from
 * regular headers, providing type-safe access to required HTTP/2
 * request metadata.
 *
 * @see https://www.rfc-editor.org/rfc/rfc9113#section-8.3.1
 */
struct Http2RequestMeta {
    std::string method;     ///< :method (required)
    std::string path;       ///< :path (required for non-CONNECT)
    std::string scheme;     ///< :scheme (required for non-CONNECT)
    std::string authority;  ///< :authority (optional, like Host)
    std::optional<std::string> protocol;  ///< :protocol (for extended CONNECT)
};

/**
 * @brief Parsed HTTP/2 response pseudo-header.
 *
 * Contains the :status pseudo-header from an HTTP/2 response.
 *
 * @see https://www.rfc-editor.org/rfc/rfc9113#section-8.3.2
 */
struct Http2ResponseMeta {
    int status = 0;  ///< :status (required)
};

// ── Module-level functions (Settings utilities) ────────────────────────

/**
 * @brief Returns the default HTTP/2 settings.
 *
 * Mimics Node.js `http2.getDefaultSettings()`.
 *
 * @return Settings struct with default values.
 *
 * @par Example
 * @code{.cpp}
 *   auto settings = http2::getDefaultSettings();
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#http2getdefaultsettings
 */
Settings getDefaultSettings();

/**
 * @brief Serializes settings into a packed binary buffer.
 *
 * Mimics Node.js `http2.getPackedSettings(settings)`.
 *
 * @param settings The settings to serialize.
 * @return Buffer containing the packed settings.
 * @note This function does not validate setting ranges. Call validateSettings()
 *       first if you need local validation before packing.
 *
 * @par Example
 * @code{.cpp}
 *   auto packed = http2::getPackedSettings(http2::getDefaultSettings());
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#http2getpackedsettingssettings
 */
buffer::Buffer getPackedSettings(const Settings& settings);

/**
 * @brief Deserializes a packed settings buffer back into a Settings struct.
 *
 * @param buf The packed settings buffer (must be multiple of 6 bytes).
 * @return Settings struct with deserialized values.
 * @throws Error if buffer length is not a multiple of 6.
 *
 * @par Example
 * @code{.cpp}
 *   auto packed = http2::getPackedSettings(http2::getDefaultSettings());
 *   auto settings = http2::getUnpackedSettings(packed);
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#http2getunpackedsettingsbuf
 */
Settings getUnpackedSettings(const buffer::Buffer& buf);

/**
 * @brief Validates certain Settings field values.
 *
 * Only `initialWindowSize` and `maxFrameSize` are checked; other fields are
 * accepted without validation by this routine.
 *
 * @param settings The settings to validate.
 * @throws Error if `initialWindowSize` exceeds 2^31-1, or if `maxFrameSize`
 *         is outside the range [16384, 16777215].
 *
 * @par Example
 * @code{.cpp}
 *   http2::validateSettings(http2::getDefaultSettings());
 * @endcode
 *
 * @see https://www.rfc-editor.org/rfc/rfc9113#section-6.5.2
 */
void validateSettings(const Settings& settings);

// ════════════════════════════════════════════════════════════════════════
// Http2Stream — base stream handle
// ════════════════════════════════════════════════════════════════════════

/**
 * @class Http2Stream
 * @brief Represents an HTTP/2 stream within a session.
 *
 * Each instance of Http2Stream represents a bidirectional HTTP/2
 * communications stream over an Http2Session. There may be up to 2^31-1
 * Http2Stream instances over the lifetime of an Http2Session.
 *
 * User code will not construct Http2Stream instances directly. Rather,
 * these are created, managed, and provided to user code through the
 * Http2Session instance.
 *
 * Events: "aborted", "close", "data", "end", "error", "frameError",
 *         "ready", "timeout", "trailers", "wantTrailers"
 *
 * @par Example
 * @code{.cpp}
 *   polycpp::EventContext ctx;
 *   auto server = http2::createServer(ctx, {}, [](auto stream, auto) {
 *       stream.close();
 *   });
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#class-http2stream
 * @see ServerHttp2Stream
 * @see ClientHttp2Stream
 * @see Http2Session
 * @see Http2ServerRequest
 * @see Http2ServerResponse
 */
class Http2Stream : public EventEmitterForwarder {
public:
    /**
     * @brief Default-constructs an empty stream wrapper.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     * @endcode
     */
    Http2Stream() noexcept;

    /**
     * @brief Wraps an existing stream implementation.
     * @param impl Shared stream implementation.
     *
     * @par Example
     * @code{.cpp}
     *   std::shared_ptr<http2::impl::Http2StreamImpl> impl;
     *   auto stream = http2::Http2Stream(impl);
     * @endcode
     */
    explicit Http2Stream(std::shared_ptr<impl::Http2StreamImpl> impl) noexcept;

    /**
     * @brief Destroys the stream wrapper.
     */
    ~Http2Stream();

    /**
     * @brief Copy-constructs a stream wrapper.
     */
    Http2Stream(const Http2Stream&) = default;

    /**
     * @brief Copy-assigns a stream wrapper.
     * @return A reference to this object.
     */
    Http2Stream& operator=(const Http2Stream&) = default;

    /**
     * @brief Move-constructs a stream wrapper.
     */
    Http2Stream(Http2Stream&&) = default;

    /**
     * @brief Move-assigns a stream wrapper.
     * @return A reference to this object.
     */
    Http2Stream& operator=(Http2Stream&&) = default;

    // Event registration is provided by EventEmitterForwarder.

    // ── Properties ─────────────────────────────────────────────────

    /**
     * @brief Returns true if the stream was aborted.
     * @return true if the stream was aborted.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   bool aborted = stream.aborted();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamaborted
     */
    bool aborted() const noexcept;

    /**
     * @brief Returns the numeric stream identifier of this Http2Stream.
     * @return The numeric stream identifier.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   int32_t id = stream.id();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamid
     */
    int32_t id() const noexcept;

    /**
     * @brief Returns true if the stream has been closed.
     * @return true if the stream has been closed.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   bool closed = stream.closed();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamclosed
     */
    bool closed() const noexcept;

    /**
     * @brief Returns true if the stream has been destroyed.
     * @return true if the stream has been destroyed.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   bool destroyed = stream.destroyed();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamdestroyed
     */
    bool destroyed() const noexcept;

    /**
     * @brief Returns true if END_STREAM flag was set on the received HEADERS frame.
     * @return true if END_STREAM was observed on the received HEADERS frame.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   bool endAfterHeaders = stream.endAfterHeaders();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamendafterheaders
     */
    bool endAfterHeaders() const noexcept;

    /**
     * @brief Returns true if the stream is pending (not yet assigned an ID).
     * @return true if the stream is pending.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   bool pending = stream.pending();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streampending
     */
    bool pending() const noexcept;

    /**
     * @brief Returns the RST_STREAM error code received from the peer.
     * @return The received RST_STREAM error code.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   uint32_t code = stream.rstCode();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamrstcode
     */
    uint32_t rstCode() const noexcept;

    /**
     * @brief Returns the headers sent by this stream.
     * @return The headers sent by this stream.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   auto headers = stream.sentHeaders();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamsentHeaders
     */
    const http::Headers& sentHeaders() const;

    /**
     * @brief Returns the informational headers sent by this stream.
     * @return The informational headers sent by this stream.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   auto info = stream.sentInfoHeaders();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamsentinfoheaders
     */
    const std::vector<http::Headers>& sentInfoHeaders() const noexcept;

    /**
     * @brief Returns the sent trailers.
     * @return The sent trailers.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   auto trailers = stream.sentTrailers();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamsenttrailers
     */
    const http::Headers& sentTrailers() const noexcept;

    /**
     * @brief Returns the session this stream belongs to.
     * @return The owning session implementation.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   auto session = stream.session();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2stream
     */
    std::shared_ptr<impl::Http2SessionImpl> session() const noexcept;

    /**
     * @brief Returns the parsed request pseudo-headers, if this is a request stream.
     * @return Optional containing Http2RequestMeta if pseudo-headers were received.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   auto meta = stream.requestMeta();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2stream
     */
    const std::optional<Http2RequestMeta>& requestMeta() const noexcept;

    /**
     * @brief Returns the parsed response pseudo-headers, if this is a response stream.
     * @return Optional containing Http2ResponseMeta if :status was received.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   auto meta = stream.responseMeta();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2stream
     */
    const std::optional<Http2ResponseMeta>& responseMeta() const noexcept;

    /**
     * @brief Returns the stream state.
     * @return The current stream state.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   auto state = stream.state();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#http2streamstate
     */
    StreamState state() const;

    // ── Methods ────────────────────────────────────────────────────

    /**
     * @brief Closes the stream by sending RST_STREAM with the given code.
     * @param code The RST_STREAM error code. Defaults to NGHTTP2_NO_ERROR.
     * @param callback Called when the close completes.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   stream.close();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamclosecode-callback
     */
    void close(uint32_t code = constants::NGHTTP2_NO_ERROR,
               std::function<void()> callback = nullptr);

    /**
     * @brief Closes the stream using a strongly-typed Http2ErrorCode.
     * @param code The Http2ErrorCode enum value.
     * @param callback Called when the close completes.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   stream.close(http2::Http2ErrorCode::NoError);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamclosecode-callback
     */
    void close(Http2ErrorCode code,
               std::function<void()> callback = nullptr);

    /**
     * @brief Sets the stream priority.
     *
     * The implementation accepts the call for compatibility, but it does not
     * apply any priority change.
     *
     * @param options Unused priority options.
     * @note Priority updates are ignored.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   stream.priority({});
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streampriorityoptions
     */
    void priority(const ClientRequestOptions& options) noexcept;

    /**
     * @brief Sets the timeout for the stream.
     * @param msecs Timeout in milliseconds.
     * @param callback Called when timeout fires.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   stream.setTimeout(30'000);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamsettimeoutmsecs-callback
     */
    void setTimeout(uint32_t msecs, std::function<void()> callback = nullptr);

    /**
     * @brief Sends trailing headers.
     * @param headers The trailing headers to send.
     * @throws Error if the trailer headers contain any pseudo-headers.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   stream.sendTrailers({{"x-checksum", "abc123"}});
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamsendtrailersheaders
     */
    void sendTrailers(const http::Headers& headers);

    /**
     * @brief Returns true if the impl is valid/non-null.
     * @return true if the stream handle is bound to a live implementation.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   if (stream.valid()) {
     *       stream.close();
     *   }
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2stream
     */
    bool valid() const noexcept;

    /**
     * @brief Access the underlying implementation.
     * @return The shared stream implementation.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Stream stream;
     *   auto impl = stream.impl();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2stream
     */
    std::shared_ptr<impl::Http2StreamImpl> impl() const noexcept;

private:
    std::shared_ptr<impl::Http2StreamImpl> impl_;
};

// ════════════════════════════════════════════════════════════════════════
// ServerHttp2Stream — server-side stream
// ════════════════════════════════════════════════════════════════════════

/**
 * @class ServerHttp2Stream
 * @brief Server-side HTTP/2 stream.
 *
 * Instances of ServerHttp2Stream are created by the Http2Server and
 * passed to the user through the 'stream' event. On the server side,
 * instances provide methods like respond(), respondWithFD(), and
 * pushStream().
 *
 * @par Example
 * @code{.cpp}
 *   polycpp::EventContext ctx;
 *   auto server = http2::createServer(ctx, {}, [](http2::ServerHttp2Stream stream, auto) {
 *       stream.respond(200, {{"content-type", "text/plain"}});
 *       stream.end("ok");
 *   });
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#class-serverhttp2stream
 * @see Http2Stream
 * @see Http2Server
 * @see Http2ServerRequest
 * @see Http2ServerResponse
 */
class ServerHttp2Stream : public Http2Stream {
public:
    /**
     * @brief Default-constructs an empty server stream wrapper.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     * @endcode
     */
    ServerHttp2Stream() noexcept;

    /**
     * @brief Wraps an existing server stream implementation.
     * @param impl Shared stream implementation.
     *
     * @par Example
     * @code{.cpp}
     *   std::shared_ptr<http2::impl::Http2StreamImpl> impl;
     *   auto stream = http2::ServerHttp2Stream(impl);
     * @endcode
     */
    explicit ServerHttp2Stream(std::shared_ptr<impl::Http2StreamImpl> impl) noexcept;

    /**
     * @brief Returns true if headers were already sent.
     * @return true if headers were already sent.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   if (!stream.headersSent()) {
     *       stream.respond(200, {{"content-type", "text/plain"}});
     *   }
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamheaderssent
     */
    bool headersSent() const noexcept;

    /**
     * @brief Returns true if push streams are allowed by the client.
     * @return true if push streams are allowed.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   if (stream.pushAllowed()) {
     *       stream.pushStream({{":path", "/app.css"}}, [](const Error*, auto, auto) {});
     *   }
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streampushallowed
     */
    bool pushAllowed() const;

    /**
     * @brief Sends a response to the client.
     *
     * @param headers Response headers. Must include ":status" pseudo-header.
     * @param options Response options.
     * @throws Error if the response headers omit `:status` or contain
     *         invalid HTTP/2 pseudo-headers.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   stream.respond({{":status", "200"}, {"content-type", "text/html"}});
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamrespondheaders-options
     */
    void respond(const http::Headers& headers = {},
                 const StreamResponseOptions& options = {});

    /**
     * @brief Sends a response with an explicit status code.
     *
     * Convenience overload that builds the `:status` pseudo-header
     * automatically, keeping regular headers separate. The status code is
     * forwarded as-is without range validation.
     *
     * @param statusCode Status code to serialize into `:status`.
     * @param headers Regular response headers (no pseudo-headers needed).
     * @param options Response options.
     * @throws Error if the synthesized response headers violate HTTP/2
     *         pseudo-header rules.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   stream.respond(200, {{"content-type", "text/html"}});
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamrespondheaders-options
     */
    void respond(int statusCode,
                 const http::Headers& headers = {},
                 const StreamResponseOptions& options = {});

    /**
     * @brief Sends informational (1xx) headers.
     *
     * @param headers Informational headers to send.
     * @throws Error if the headers are not valid informational response
     *         headers.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   stream.additionalHeaders({{":status", "103"},
     *                            {"link", "</app.css>; rel=preload"}});
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamadditionalhEaders
     */
    void additionalHeaders(const http::Headers& headers);

    /**
     * @brief Sends a response using a file descriptor as the data source.
     *
     * The file descriptor is read lazily — only the bytes that nghttp2
     * can send are read from disk, preventing OOM for large files.
     * The caller retains ownership of the fd and must close it.
     *
     * @param fd Open file descriptor to read from.
     * @param headers Response headers. Must include ":status" pseudo-header.
     * @param options File response options (offset, length, waitForTrailers).
     * @throws Error if the response headers omit `:status` or contain
     *         invalid HTTP/2 pseudo-headers.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   int fd = ::open("large-file.bin", O_RDONLY);
     *   stream.respondWithFD(fd, {{":status", "200"},
     *       {"content-type", "application/octet-stream"}});
     *   // Caller closes fd after stream ends
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamrespondwithfdfd-headers-options
     */
    void respondWithFD(int fd, const http::Headers& headers = {},
                       const StreamFileResponseOptions& options = {});

    /**
     * @brief Sends a response reading data from a file path.
     *
     * Opens the file, reads lazily in the nghttp2 data source callback,
     * and closes the fd when the stream ends.
     *
     * @param path File path to send.
     * @param headers Response headers. Must include ":status" pseudo-header.
     * @param options File response options (offset, length, waitForTrailers).
     * @throws Error if the response headers omit `:status` or contain
     *         invalid HTTP/2 pseudo-headers. File-open failures are reported
     *         through the stream's `error` event.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   stream.respondWithFile("/var/www/index.html",
     *       {{":status", "200"}, {"content-type", "text/html"}});
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamrespondwithfilepath-headers-options
     */
    void respondWithFile(const std::string& path, const http::Headers& headers = {},
                         const StreamFileResponseOptions& options = {});

    /**
     * @brief Initiates a push stream.
     *
     * @param headers Push promise headers.
     * @param callback Called with the push stream: (err, pushStream, headers).
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   stream.pushStream({{":path", "/assets/app.css"}}, [](auto err, auto push, auto) {
     *       if (!err) {
     *           push.respond(200, {{"content-type", "text/css"}});
     *           push.end("body { color: #222; }");
     *       }
     *   });
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streampushstreamheaders-options-callback
     */
    void pushStream(const http::Headers& headers,
                    std::function<void(const Error*, ServerHttp2Stream, const http::Headers&)> callback);

    /**
     * @brief Writes data to the stream.
     *
     * @param data Data to write.
     * @param callback Called when write completes.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   stream.write("chunk");
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamwritechunk-encoding-callback
     */
    void write(const std::string& data, std::function<void()> callback = nullptr);

    /**
     * @brief Writes data and ends the stream.
     *
     * @param data Final data to write (empty for no data).
     * @param callback Called when end completes.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   stream.end("done");
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamenddata-encoding-callback
     */
    void end(const std::string& data = "", std::function<void()> callback = nullptr);
};

// ════════════════════════════════════════════════════════════════════════
// ClientHttp2Stream — client-side stream
// ════════════════════════════════════════════════════════════════════════

/**
 * @class ClientHttp2Stream
 * @brief Client-side HTTP/2 stream.
 *
 * Instances of ClientHttp2Stream are created when the
 * ClientHttp2Session.request() method is called. On the client side,
 * instances provide events for received response headers, data, and trailers.
 *
 * Events: "continue", "headers", "push", "response"
 *
 * @par Example
 * @code{.cpp}
 *   // Assume `session` is an already-connected client session.
 *   http2::ClientHttp2Session session;
 *   auto stream = session.request(http::Headers{
 *       {":method", "GET"},
 *       {":path", "/"},
 *       {":scheme", "https"},
 *       {":authority", "example.com"}
 *   });
 *   stream.end();
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#class-clienthttp2stream
 * @see ClientHttp2Session
 * @see Http2Stream
 */
class ClientHttp2Stream : public Http2Stream {
public:
    /**
     * @brief Default-constructs an empty client stream wrapper.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ClientHttp2Stream stream;
     * @endcode
     */
    ClientHttp2Stream() noexcept;

    /**
     * @brief Wraps an existing client stream implementation.
     * @param impl Shared stream implementation.
     *
     * @par Example
     * @code{.cpp}
     *   std::shared_ptr<http2::impl::Http2StreamImpl> impl;
     *   auto stream = http2::ClientHttp2Stream(impl);
     * @endcode
     */
    explicit ClientHttp2Stream(std::shared_ptr<impl::Http2StreamImpl> impl) noexcept;

    /**
     * @brief Writes data to the stream.
     *
     * @param data Data to write.
     * @param callback Called when write completes.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ClientHttp2Stream stream;
     *   stream.write("chunk");
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-clienthttp2stream
     */
    void write(const std::string& data, std::function<void()> callback = nullptr);

    /**
     * @brief Writes data and ends the stream.
     *
     * @param data Final data to write (empty for no data).
     * @param callback Called when end completes.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ClientHttp2Stream stream;
     *   stream.end("done");
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-clienthttp2stream
     */
    void end(const std::string& data = "", std::function<void()> callback = nullptr);

    // Event registration is inherited from Http2Stream / EventEmitterForwarder.
    //   on(event::Response, cb), once(event::Push, cb), etc.
};

// ════════════════════════════════════════════════════════════════════════
// Http2Session — base session handle
// ════════════════════════════════════════════════════════════════════════

/**
 * @class Http2Session
 * @brief Represents an HTTP/2 session (connection).
 *
 * Each Http2Session instance is associated with exactly one net.Socket
 * or tls.TLSSocket when created. When either the Socket or the
 * Http2Session are destroyed, both will be destroyed.
 *
 * Events: "close", "connect", "error", "frameError", "goaway",
 *         "localSettings", "ping", "remoteSettings", "stream", "timeout"
 *
 * @par Example
 * @code{.cpp}
 *   polycpp::EventContext ctx;
 *   auto session = http2::connect(ctx, "http://localhost:8080");
 *   session.ping();
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#class-http2session
 * @see Http2Stream
 * @see ServerHttp2Session
 * @see ClientHttp2Session
 * @see Http2Server
 * @see Http2SecureServer
 */
class Http2Session : public EventEmitterForwarder {
public:
    /**
     * @brief Default-constructs an empty session wrapper.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     * @endcode
     */
    Http2Session();

    /**
     * @brief Wraps an existing session implementation.
     * @param impl Shared session implementation.
     *
     * @par Example
     * @code{.cpp}
     *   std::shared_ptr<http2::impl::Http2SessionImpl> impl;
     *   auto session = http2::Http2Session(impl);
     * @endcode
     */
    explicit Http2Session(std::shared_ptr<impl::Http2SessionImpl> impl) noexcept;

    /**
     * @brief Destroys the session wrapper.
     */
    virtual ~Http2Session();

    /**
     * @brief Copy-constructs a session wrapper.
     */
    Http2Session(const Http2Session&) = default;

    /**
     * @brief Copy-assigns a session wrapper.
     * @return A reference to this object.
     */
    Http2Session& operator=(const Http2Session&) = default;

    /**
     * @brief Move-constructs a session wrapper.
     */
    Http2Session(Http2Session&&) = default;

    /**
     * @brief Move-assigns a session wrapper.
     * @return A reference to this object.
     */
    Http2Session& operator=(Http2Session&&) = default;

    // Event registration is provided by EventEmitterForwarder.
    //   on(event::Close, cb), once(event::Error_, cb), off(listenerId), etc.

    // ── Properties ─────────────────────────────────────────────────

    /**
     * @brief Returns true if the session has been closed.
     * @return true if the session has been closed.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   bool closed = session.closed();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#http2sessionclosed
     */
    bool closed() const;

    /**
     * @brief Returns true if the session has been destroyed.
     * @return true if the session has been destroyed.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   bool destroyed = session.destroyed();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#http2sessiondestroyed
     */
    bool destroyed() const;

    /**
     * @brief Returns true if this is a server session.
     * @return true if this is a server session.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   bool serverSide = session.isServer();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2session
     */
    bool isServer() const;

    /**
     * @brief Returns the local settings.
     * @return A copy of the local settings snapshot.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   auto settings = session.localSettings();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#http2sessionlocalsettings
     */
    Settings localSettings() const;

    /**
     * @brief Returns the remote peer's settings.
     * @return A copy of the remote peer's settings snapshot.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   auto settings = session.remoteSettings();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#http2sessionremotesettings
     */
    Settings remoteSettings() const;

    /**
     * @brief Returns true if the session is waiting for a settings ack.
     * @return true if a SETTINGS acknowledgment is pending.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   bool pending = session.pendingSettingsAck();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#http2sessionpendingsettingsack
     */
    bool pendingSettingsAck() const;

    /**
     * @brief Returns true if the session is encrypted (TLS).
     * @return true if the session is encrypted.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   bool encrypted = session.encrypted();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#http2sessionencrypted
     */
    bool encrypted() const;

    /**
     * @brief Returns the ALPN protocol negotiated.
     * @return The negotiated ALPN protocol string.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   auto protocol = session.alpnProtocol();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#http2sessionalpnprotocol
     */
    std::string alpnProtocol() const;

    /**
     * @brief Returns the origin URL string for this session.
     * @return The origin set string for this session.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   auto origin = session.originSet();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#http2sessionoriginset
     */
    const std::string& originSet() const noexcept;

    /**
     * @brief Returns the session state.
     * @return The current session state.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   auto state = session.state();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#http2sessionstate
     */
    SessionState state() const;

    /**
     * @brief Returns the session type discriminator.
     * @return `constants::NGHTTP2_SESSION_SERVER` (0) for server sessions,
     *         `constants::NGHTTP2_SESSION_CLIENT` (1) for client sessions, or
     *         -1 when the session handle is not bound to an implementation.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   int type = session.type();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2session
     */
    int type() const noexcept;

    // ── Methods ────────────────────────────────────────────────────

    /**
     * @brief Gracefully closes the session.
     *
     * @param callback Called when the session is fully closed.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   session.close();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2sessionclosecallback
     */
    void close(std::function<void()> callback = nullptr);

    /**
     * @brief Immediately terminates the session.
     *
     * @param error Optional error to emit via the 'error' event before shutdown.
     * @param code HTTP/2 error code forwarded to the underlying session
     *             implementation.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   session.destroy();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2sessiondestroyerror-code
     */
    void destroy(const Error* error = nullptr,
                 uint32_t code = constants::NGHTTP2_NO_ERROR);

    /**
     * @brief Strongly-typed overload of destroy() that converts Http2ErrorCode
     *        to `uint32_t` and delegates to the base overload.
     *
     * @param error Optional error to emit via the 'error' event before shutdown.
     * @param code Http2ErrorCode value; converted to `uint32_t` and forwarded
     *             to the base overload.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   session.destroy(nullptr, http2::Http2ErrorCode::NoError);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2sessiondestroyerror-code
     */
    void destroy(const Error* error, Http2ErrorCode code);

    /**
     * @brief Sends a GOAWAY frame to the remote peer.
     *
     * @param code Error code. Defaults to NGHTTP2_NO_ERROR.
     * @param lastStreamID Last stream ID the sender might have processed.
     * @param opaqueData Optional opaque data to include.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   session.goaway();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2sessiongoawaycode-laststreamid-opaquedata
     */
    void goaway(uint32_t code = constants::NGHTTP2_NO_ERROR,
                int32_t lastStreamID = 0,
                const std::string& opaqueData = "");

    /**
     * @brief Sends a GOAWAY frame (strongly-typed error code).
     * @param code Http2ErrorCode enum value.
     * @param lastStreamID Last stream ID the sender might have processed.
     * @param opaqueData Optional opaque data to include.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   session.goaway(http2::Http2ErrorCode::NoError);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2sessiongoawaycode-laststreamid-opaquedata
     */
    void goaway(Http2ErrorCode code,
                int32_t lastStreamID = 0,
                const std::string& opaqueData = "");

    /**
     * @brief Submits a PING frame to the remote peer.
     *
     * @param payload Optional 8-byte payload; truncated or zero-padded to 8 bytes.
     * @param callback Called with (err, durationMs, payload) when the PING ack
     *                 is received.
     * @return true if the ping was submitted to the underlying session; false
     *         when the session handle is not bound to an implementation
     *         (i.e. `valid() == false`).
     * @note The payload is truncated or zero-padded to 8 bytes.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   session.ping("12345678", [](const Error*, double, const std::string&) {});
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2sessionpingpayload-callback
     */
    bool ping(const std::string& payload = "",
              std::function<void(const Error*, double, const std::string&)> callback = nullptr);

    /**
     * @brief Sends a SETTINGS frame.
     *
     * @param settings The settings to send.
     * @param callback Called when SETTINGS_ACK is received.
     * @note This is a best-effort wrapper over the underlying session.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   http2::Settings settings;
     *   settings.maxConcurrentStreams = 128;
     *   session.settings(settings);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2sessionsettingssettings-callback
     */
    void settings(const Settings& settings,
                  std::function<void(const Error*)> callback = nullptr);

    /**
     * @brief Sets the timeout for the session.
     *
     * @param msecs Timeout in milliseconds.
     * @param callback Called when timeout fires.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   session.setTimeout(30'000);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2sessionsettimeoutmsecs-callback
     */
    void setTimeout(uint32_t msecs, std::function<void()> callback = nullptr);

    /**
     * @brief Sets the local window size for flow control.
     *
     * @param windowSize New window size.
     * @note This updates the underlying session state immediately.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   session.setLocalWindowSize(1 << 20);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2sessionsetlocalwindowsizewindowsize
     */
    void setLocalWindowSize(int32_t windowSize);

    /**
     * @brief Returns true if the impl is valid/non-null.
     * @return true if the session handle is bound to a live implementation.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   if (session.valid()) {
     *       session.ping();
     *   }
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2session
     */
    bool valid() const noexcept;

    /**
     * @brief Access the underlying implementation.
     * @return The shared session implementation.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Session session;
     *   auto impl = session.impl();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2session
     */
    std::shared_ptr<impl::Http2SessionImpl> impl() const noexcept;

private:
protected:
    std::shared_ptr<impl::Http2SessionImpl> impl_;
};

// ════════════════════════════════════════════════════════════════════════
// ServerHttp2Session — server-side session
// ════════════════════════════════════════════════════════════════════════

/**
 * @class ServerHttp2Session
 * @brief Server-side HTTP/2 session.
 *
 * Server sessions own server streams and server-push primitives.
 *
 * @par Example
 * @code{.cpp}
 *   polycpp::EventContext ctx;
 *   auto server = http2::createServer(ctx, {}, [](http2::ServerHttp2Stream stream, auto) {
 *       stream.respond(200, {{"content-type", "text/plain"}});
 *       stream.end("ok");
 *   });
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#class-serverhttp2session
 * @see Http2Session
 * @see ServerHttp2Stream
 * @see Http2Server
 * @see Http2SecureServer
 */
class ServerHttp2Session : public Http2Session {
public:
    /**
     * @brief Default-constructs an empty server session wrapper.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Session session;
     * @endcode
     */
    ServerHttp2Session() noexcept;

    /**
     * @brief Wraps an existing server session implementation.
     * @param impl Shared session implementation.
     *
     * @par Example
     * @code{.cpp}
     *   std::shared_ptr<http2::impl::Http2SessionImpl> impl;
     *   auto session = http2::ServerHttp2Session(impl);
     * @endcode
     */
    explicit ServerHttp2Session(std::shared_ptr<impl::Http2SessionImpl> impl);

    /**
     * @brief Submits an ALTSVC frame via nghttp2.
     *
     * @param alt Alt-Svc field value forwarded as the frame payload.
     * @param originOrStream Either an origin URI string or a purely numeric
     *                       stream identifier. When the entire string parses as
     *                       an integer it is treated as the stream ID and the
     *                       origin is sent empty; otherwise the string is sent
     *                       as the origin with stream_id=0.
     * @note This method is a no-op if the session is unbound, destroyed, or
     *       closed. Failures from the underlying submission are reported by
     *       emitting the 'error' event on the session; no exception is thrown.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Session session;
     *   session.altsvc("h2=\"example.com:443\"", "https://example.com");
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2sessionaltsvcalt-originorstream
     */
    void altsvc(const std::string& alt, const std::string& originOrStream);

    /**
     * @brief Submits an ORIGIN frame via nghttp2.
     *
     * @param origins Vector of origin strings passed as nghttp2 origin entries.
     * @note This method is a no-op if the session is unbound, destroyed, or
     *       closed. Failures from the underlying submission are reported by
     *       emitting the 'error' event on the session; no exception is thrown.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Session session;
     *   session.origin({"https://example.com", "https://api.example.com"});
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2sessionoriginorigins
     */
    void origin(const std::vector<std::string>& origins);

    // Event registration is inherited from Http2Session / EventEmitterForwarder.
    //   on(event::ServerStream, cb), once(event::Close, cb), etc.
};

// ════════════════════════════════════════════════════════════════════════
// ClientHttp2Session — client-side session
// ════════════════════════════════════════════════════════════════════════

/**
 * @class ClientHttp2Session
 * @brief Client-side HTTP/2 session.
 *
 * Client sessions are usually created by `connect()` and produce
 * `ClientHttp2Stream` instances through `request()`.
 *
 * @par Example
 * @code{.cpp}
 *   polycpp::EventContext ctx;
 *   auto session = http2::connect(ctx, "http://localhost:8080");
 *   auto stream = session.request(http::Headers{
 *       {":method", "GET"},
 *       {":path", "/"},
 *       {":scheme", "http"},
 *       {":authority", "localhost:8080"}
 *   });
 *   stream.end();
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#class-clienthttp2session
 * @see Http2Session
 * @see ClientHttp2Stream
 * @see Http2ServerResponse
 */
class ClientHttp2Session : public Http2Session {
public:
    /**
     * @brief Default-constructs an empty client session wrapper.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ClientHttp2Session session;
     * @endcode
     */
    ClientHttp2Session() noexcept;

    /**
     * @brief Wraps an existing client session implementation.
     * @param impl Shared session implementation.
     *
     * @par Example
     * @code{.cpp}
     *   std::shared_ptr<http2::impl::Http2SessionImpl> impl;
     *   auto session = http2::ClientHttp2Session(impl);
     * @endcode
     */
    explicit ClientHttp2Session(std::shared_ptr<impl::Http2SessionImpl> impl) noexcept;

    /**
     * @brief Sends a request on this session.
     *
     * @param headers Request headers. Must include ":method" and ":path".
     * @param options Request options. Only `endStream` and `waitForTrailers`
     * are used by this implementation.
     * @return A ClientHttp2Stream representing the request, or a default-constructed
     * stream if submission fails.
     * @note `exclusive` and `parent` are ignored.
     * @throws Error if the headers violate HTTP/2 pseudo-header rules or
     *         omit required request pseudo-headers.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ClientHttp2Session session;
     *   auto stream = session.request(http::Headers{
     *       {":method", "GET"},
     *       {":path", "/"},
     *       {":scheme", "https"},
     *       {":authority", "example.com"}
     *   });
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#clienthttp2sessionrequestheaders-options
     */
    ClientHttp2Stream request(const http::Headers& headers,
                              const ClientRequestOptions& options = {});

    /**
     * @brief Sends a request using separated pseudo-headers.
     *
     * Convenience overload that takes an Http2RequestMeta struct for
     * pseudo-headers and regular headers separately, avoiding mixing
     * pseudo-headers into the headers map.
     *
     * @param meta Request pseudo-headers (:method, :path, :scheme, :authority).
     * @param headers Regular request headers.
     * @param options Request options. Only `endStream` and `waitForTrailers`
     * are used by the delegated overload.
     * @return A ClientHttp2Stream representing the request, or a default-constructed
     * stream if submission fails.
     * @note `exclusive` and `parent` are ignored.
     * @throws Error if `meta` omits required request pseudo-headers or the
     *         combined headers violate HTTP/2 pseudo-header rules.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ClientHttp2Session session;
     *   http2::Http2RequestMeta meta;
     *   meta.method = "GET";
     *   meta.path = "/";
     *   meta.scheme = "https";
     *   meta.authority = "example.com";
     *   auto stream = session.request(meta, {{"accept", "text/html"}});
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#clienthttp2sessionrequestheaders-options
     */
    ClientHttp2Stream request(const Http2RequestMeta& meta,
                              const http::Headers& headers = {},
                              const ClientRequestOptions& options = {});

    // Event registration is inherited from Http2Session / EventEmitterForwarder.
    //   on(event::ClientStream, cb), once(event::Connect, cb), etc.
};

// ════════════════════════════════════════════════════════════════════════
// Http2Server — plain-text HTTP/2 server
// ════════════════════════════════════════════════════════════════════════

/**
 * @class Http2Server
 * @brief HTTP/2 server that accepts cleartext HTTP/2 connections.
 *
 * Created using http2::createServer().
 *
 * Events: "checkContinue", "connection", "request", "session",
 *         "sessionError", "stream", "timeout"
 *
 * @par Example
 * @code{.cpp}
 *   polycpp::EventContext ctx;
 *   auto server = http2::createServer(ctx, {}, [](auto stream, auto headers) {
 *       stream.respond(200, {{"content-type", "text/plain"}});
 *       stream.end("ok");
 *   });
 *   server.listen(8080);
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#class-http2server
 * @see Http2ServerRequest
 * @see Http2ServerResponse
 * @see ServerHttp2Stream
 * @see createServer
 * @see createSecureServer
 */
class Http2Server : public EventEmitterForwarder {
public:
    /**
     * @brief Default-constructs an empty server wrapper.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Server server;
     * @endcode
     */
    Http2Server() noexcept;

    /**
     * @brief Wraps an existing server implementation.
     * @param impl Shared server implementation.
     *
     * @par Example
     * @code{.cpp}
     *   std::shared_ptr<http2::impl::Http2ServerImpl> impl;
     *   auto server = http2::Http2Server(impl);
     * @endcode
     */
    explicit Http2Server(std::shared_ptr<impl::Http2ServerImpl> impl) noexcept;

    /**
     * @brief Destroys the server wrapper.
     */
    ~Http2Server();

    /**
     * @brief Copy-constructs a server wrapper.
     */
    Http2Server(const Http2Server&) = default;

    /**
     * @brief Copy-assigns a server wrapper.
     * @return A reference to this object.
     */
    Http2Server& operator=(const Http2Server&) = default;

    /**
     * @brief Move-constructs a server wrapper.
     */
    Http2Server(Http2Server&&) = default;

    /**
     * @brief Move-assigns a server wrapper.
     * @return A reference to this object.
     */
    Http2Server& operator=(Http2Server&&) = default;

    // ── Methods ────────────────────────────────────────────────────

    /**
     * @brief Starts listening for connections.
     *
     * @param port Port to listen on.
     * @param host Host to bind to. Defaults to "0.0.0.0".
     * @param callback Called when listening starts.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Server server;
     *   server.listen(8080);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#serverlisten
     */
    void listen(uint16_t port, const std::string& host = "",
                std::function<void()> callback = nullptr);

    /**
     * @brief Closes the server.
     *
     * @param callback Called when all connections are closed.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Server server;
     *   server.close();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#serverclose
     */
    void close(std::function<void()> callback = nullptr);

    /**
     * @brief Sets the timeout for the server.
     * @param msecs Timeout in milliseconds.
     * @param callback Called when timeout fires.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Server server;
     *   server.setTimeout(30'000);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#server-settimeoutmsecs-callback
     */
    void setTimeout(uint32_t msecs, std::function<void()> callback = nullptr);

    /**
     * @brief Returns true if the server is listening.
     * @return true if the server is listening.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Server server;
     *   bool listening = server.listening();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2server
     */
    bool listening() const noexcept;

    /**
     * @brief Returns the address the server is bound to.
     * @return A map with {address, family, port} entries.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Server server;
     *   auto addr = server.address();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2server
     */
    std::unordered_map<std::string, std::string> address() const;

    /**
     * @brief Returns true if the impl is valid.
     * @return true if the server handle is bound to a live implementation.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Server server;
     *   if (server.valid()) {
     *       server.listen(8080);
     *   }
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2server
     */
    bool valid() const noexcept;

    /**
     * @brief Access the underlying implementation.
     * @return The shared server implementation.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2Server server;
     *   auto impl = server.impl();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2server
     */
    std::shared_ptr<impl::Http2ServerImpl> impl() const;

    // Event registration is provided by EventEmitterForwarder.

private:
    std::shared_ptr<impl::Http2ServerImpl> impl_;
};

// ════════════════════════════════════════════════════════════════════════
// Http2SecureServer — TLS HTTP/2 server
// ════════════════════════════════════════════════════════════════════════

/**
 * @class Http2SecureServer
 * @brief HTTP/2 server that accepts TLS-encrypted HTTP/2 connections.
 *
 * Created using http2::createSecureServer().
 *
 * @par Example
 * @code{.cpp}
 *   // Assume `options` contains TLS material.
 *   polycpp::EventContext ctx;
 *   http2::SecureServerOptions options;
 *   auto server = http2::createSecureServer(ctx, options, [](auto stream, auto) {
 *       stream.respond(200, {{"content-type", "text/plain"}});
 *       stream.end("ok");
 *   });
 *   server.listen(8443);
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#class-http2secureserver
 * @see Http2Server
 * @see Http2ServerRequest
 * @see Http2ServerResponse
 * @see createSecureServer
 */
class Http2SecureServer : public EventEmitterForwarder {
public:
    /**
     * @brief Default-constructs an empty secure server wrapper.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2SecureServer server;
     * @endcode
     */
    Http2SecureServer() noexcept;

    /**
     * @brief Wraps an existing secure server implementation.
     * @param impl Shared server implementation.
     *
     * @par Example
     * @code{.cpp}
     *   std::shared_ptr<http2::impl::Http2ServerImpl> impl;
     *   auto server = http2::Http2SecureServer(impl);
     * @endcode
     */
    explicit Http2SecureServer(std::shared_ptr<impl::Http2ServerImpl> impl) noexcept;

    /**
     * @brief Destroys the secure server wrapper.
     */
    ~Http2SecureServer();

    /**
     * @brief Copy-constructs a secure server wrapper.
     */
    Http2SecureServer(const Http2SecureServer&) = default;

    /**
     * @brief Copy-assigns a secure server wrapper.
     * @return A reference to this object.
     */
    Http2SecureServer& operator=(const Http2SecureServer&) = default;

    /**
     * @brief Move-constructs a secure server wrapper.
     */
    Http2SecureServer(Http2SecureServer&&) = default;

    /**
     * @brief Move-assigns a secure server wrapper.
     * @return A reference to this object.
     */
    Http2SecureServer& operator=(Http2SecureServer&&) = default;

    // ── Methods ────────────────────────────────────────────────────

    /**
     * @brief Starts listening for connections.
     * @param port Port to listen on.
     * @param host Host to bind to. Defaults to "0.0.0.0".
     * @param callback Called when listening starts.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2SecureServer server;
     *   server.listen(8443);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#serverlisten
     */
    void listen(uint16_t port, const std::string& host = "",
                std::function<void()> callback = nullptr);

    /**
     * @brief Closes the server.
     * @param callback Called when all connections are closed.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2SecureServer server;
     *   server.close();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#serverclose
     */
    void close(std::function<void()> callback = nullptr);

    /**
     * @brief Sets the timeout for the server.
     * @param msecs Timeout in milliseconds.
     * @param callback Called when timeout fires.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2SecureServer server;
     *   server.setTimeout(30'000);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#server-settimeoutmsecs-callback
     */
    void setTimeout(uint32_t msecs, std::function<void()> callback = nullptr);

    /**
     * @brief Returns true if the server is listening.
     * @return true if the server is listening.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2SecureServer server;
     *   bool listening = server.listening();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2server
     */
    bool listening() const noexcept;

    /**
     * @brief Returns the address the server is bound to.
     * @return A map with {address, family, port} entries.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2SecureServer server;
     *   auto addr = server.address();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2server
     */
    std::unordered_map<std::string, std::string> address() const;

    /**
     * @brief Returns true if the impl is valid.
     * @return true if the secure server handle is bound to a live implementation.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2SecureServer server;
     *   if (server.valid()) {
     *       server.listen(8443);
     *   }
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2server
     */
    bool valid() const;

    /**
     * @brief Access the underlying implementation.
     * @return The shared server implementation.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2SecureServer server;
     *   auto impl = server.impl();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2server
     */
    std::shared_ptr<impl::Http2ServerImpl> impl() const noexcept;

    // Event registration is provided by EventEmitterForwarder.

private:
    std::shared_ptr<impl::Http2ServerImpl> impl_;
};

// ════════════════════════════════════════════════════════════════════════
// Http2ServerRequest — compatibility layer (IncomingMessage-like)
// ════════════════════════════════════════════════════════════════════════

/**
 * @class Http2ServerRequest
 * @brief Compatibility API wrapping Http2Stream as an IncomingMessage-like object.
 *
 * Created automatically by the Http2Server compatibility layer when using
 * the 'request' event instead of the 'stream' event.
 *
 * @par Example
 * @code{.cpp}
 *   // Assume `req` is the request object from the compatibility layer.
 *   http2::Http2ServerRequest req;
 *   auto method = req.method();
 *   auto target = req.url();
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#class-http2http2serverrequest
 * @see Http2ServerResponse
 * @see ServerHttp2Stream
 * @see Http2Server
 */
class Http2ServerRequest : public EventEmitterForwarder {
public:
    /**
     * @brief Default-constructs an empty request wrapper.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     * @endcode
     */
    Http2ServerRequest();

    /**
     * @brief Wraps a server stream and parsed request headers.
     * @param stream The backing server stream.
     * @param headers The parsed request headers.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   http::Headers headers;
     *   http2::Http2ServerRequest req(stream, headers);
     * @endcode
     */
    explicit Http2ServerRequest(ServerHttp2Stream stream, const http::Headers& headers);

    // Event registration is provided by EventEmitterForwarder.
    //   on(event::Data, cb), once(event::End, cb), off(listenerId), etc.

    // ── Properties ─────────────────────────────────────────────────

    /**
     * @brief Returns the request authority (host).
     * @return The request authority pseudo-header value.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   auto authority = req.authority();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverrequest
     */
    std::string authority() const;

    /**
     * @brief Returns true once the request is considered fully received.
     * @return true if the underlying stream is closed or if END_STREAM was
     *         observed on the HEADERS frame.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   bool complete = req.complete();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#requestcomplete
     */
    bool complete() const noexcept;

    /**
     * @brief Returns the parsed request headers captured at construction.
     * @return const reference to the headers supplied when the request wrapper
     *         was constructed.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   auto headers = req.headers();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#requestheaders
     */
    const http::Headers& headers() const noexcept;

    /**
     * @brief Returns the constant HTTP version string "2.0".
     * @return Always the string "2.0".
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   auto version = req.httpVersion();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#requesthttpversion
     */
    std::string httpVersion() const;

    /**
     * @brief Returns the constant major HTTP version 2.
     * @return Always 2.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   int major = req.httpVersionMajor();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#requesthttpversion
     */
    int httpVersionMajor() const noexcept;

    /**
     * @brief Returns the constant minor HTTP version 0.
     * @return Always 0.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   int minor = req.httpVersionMinor();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#requesthttpversion
     */
    int httpVersionMinor() const;

    /**
     * @brief Returns the :method pseudo-header, defaulting to "GET" when absent.
     * @return The :method pseudo-header value, or "GET" if the header map does
     *         not contain :method.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   auto method = req.method();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#requestmethod
     */
    std::string method() const;

    /**
     * @brief Returns a flattened [name0, value0, name1, value1, ...] list of
     *        the raw headers.
     * @return A vector that alternates header names and values as received,
     *         preserving insertion order. Matches the Node.js `rawHeaders`
     *         layout.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   auto raw = req.rawHeaders();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#requestrawheaders
     */
    std::vector<std::string> rawHeaders() const;

    /**
     * @brief Returns raw trailer name-value pairs.
     *
     * The compatibility request object never stores received trailers, so
     * this always returns an empty vector.
     *
     * @return An empty vector.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   auto trailers = req.rawTrailers();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverrequest
     */
    std::vector<std::string> rawTrailers() const noexcept;

    /**
     * @brief Returns the :scheme pseudo-header, defaulting to "http" when absent.
     * @return The :scheme pseudo-header value, or "http" if the header map does
     *         not contain :scheme.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   auto scheme = req.scheme();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#requestscheme
     */
    std::string scheme() const;

    /**
     * @brief Returns the underlying Http2Stream.
     * @return The backing server stream.
     *
     * @see Http2ServerResponse
     * @see ServerHttp2Stream
     * @see http::IncomingMessage
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   auto stream = req.stream();
     *   stream.end();
     * @endcode
     */
    ServerHttp2Stream stream() const;

    /**
     * @brief Returns the :path pseudo-header, defaulting to "/" when absent.
     * @return The :path pseudo-header value, or "/" if the header map does not
     *         contain :path.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   auto url = req.url();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#requesturl
     */
    std::string url() const;

    /**
     * @brief Sets the timeout for the underlying stream.
     * @param msecs Timeout in milliseconds.
     * @param callback Called when timeout fires.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   req.setTimeout(15'000);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamsettimeoutmsecs-callback
     */
    void setTimeout(uint32_t msecs, std::function<void()> callback = nullptr);

    /**
     * @brief Closes the underlying server stream.
     *
     * Sends RST_STREAM with `NGHTTP2_NO_ERROR` on the backing stream; the
     * stream is closed gracefully rather than forcibly destroyed.
     *
     * @param error Ignored by the implementation.
     * @note The error argument is accepted for API parity with Node.js but is
     *       not propagated; this method always performs a graceful close.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerRequest req;
     *   req.destroy();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#http2streamdestroyerror
     */
    void destroy(const Error* error = nullptr);

private:
    ServerHttp2Stream stream_;
    http::Headers headers_;
    events::EventEmitter* emitter_ = nullptr;
    std::unique_ptr<events::EventEmitter> ownedEmitter_;
};

// ════════════════════════════════════════════════════════════════════════
// Http2ServerResponse — compatibility layer (ServerResponse-like)
// ════════════════════════════════════════════════════════════════════════

/**
 * @class Http2ServerResponse
 * @brief Compatibility API wrapping Http2Stream as a ServerResponse-like object.
 *
 * Created automatically by the Http2Server compatibility layer.
 *
 * @par Example
 * @code{.cpp}
 *   // Assume `res` is the response object from the compatibility layer.
 *   http2::Http2ServerResponse res;
 *   res.writeHead(200, {{"content-type", "text/plain"}});
 *   res.end("hello");
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
 * @see Http2ServerRequest
 * @see ServerHttp2Stream
 * @see Http2Server
 */
class Http2ServerResponse : public EventEmitterForwarder {
public:
    /**
     * @brief Default-constructs an empty response wrapper.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     * @endcode
     */
    Http2ServerResponse();

    /**
     * @brief Wraps a server stream for response handling.
     * @param stream The backing server stream.
     *
     * @par Example
     * @code{.cpp}
     *   http2::ServerHttp2Stream stream;
     *   http2::Http2ServerResponse res(stream);
     * @endcode
     */
    explicit Http2ServerResponse(ServerHttp2Stream stream);

    // Event registration is provided by EventEmitterForwarder.
    //   on(event::Finish, cb), once(event::Close, cb), etc.

    // ── Properties ─────────────────────────────────────────────────

    /**
     * @brief Returns true if headers have been sent.
     * @return true if headers have been sent.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   if (!res.headersSent()) {
     *       res.writeHead(200, {{"content-type", "text/plain"}});
     *   }
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
     */
    bool headersSent() const noexcept;

    /**
     * @brief Returns the response status code.
     * @return The response status code.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   int code = res.statusCode();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
     */
    int statusCode() const noexcept;

    /**
     * @brief Sets the response status code.
     * @param code The status code to store.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   res.setStatusCode(204);
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
     */
    void setStatusCode(int code) noexcept;

    /**
     * @brief Returns an empty string — HTTP/2 has no status reason phrase.
     * @return Always the empty string.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   auto message = res.statusMessage();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
     */
    std::string statusMessage() const;

    /**
     * @brief Returns true if the stream is writable and hasn't ended.
     * @return true if the response is writable.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   if (res.writable()) {
     *       res.write("chunk");
     *   }
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
     */
    bool writable() const noexcept;

    /**
     * @brief Returns true if the response has ended.
     * @return true if the response has finished.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   bool done = res.finished();
     * @endcode
     *
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
     */
    bool finished() const;

    /**
     * @brief Returns the underlying Http2Stream.
     * @return The backing server stream.
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   auto stream = res.stream();
     *   stream.write("chunk");
     * @endcode
     */
    ServerHttp2Stream stream() const;

    // ── Methods ────────────────────────────────────────────────────

    /**
     * @brief Sets a response header (replaces any previous value with that name).
     * @param name Header name (case-insensitive; stored lowercased by the
     *             underlying Headers type).
     * @param value Header value.
     * @note Setting a name that already exists replaces the prior value rather
     *       than appending a new one.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   res.setHeader("content-type", "text/plain");
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#responsesetheadername-value
     */
    void setHeader(const std::string& name, const std::string& value);

    /**
     * @brief Gets the value of a response header by name.
     * @param name Header name (case-insensitive).
     * @return The header value, or an empty string if the header has not been
     *         set.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   auto value = res.getHeader("content-type");
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#responsegetheadername
     */
    std::string getHeader(const std::string& name) const;

    /**
     * @brief Removes a response header by name (no-op if not present).
     * @param name Header name (case-insensitive).
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   res.removeHeader("content-length");
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#responseremoveheadername
     */
    void removeHeader(const std::string& name);

    /**
     * @brief Returns a copy of the currently-staged response headers.
     * @return A copy of the internal http::Headers map holding any
     *         `setHeader()` calls made so far.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   auto headers = res.getHeaders();
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#responsegetheaders
     */
    http::Headers getHeaders() const;

    /**
     * @brief Tests whether a response header with the given name has been set.
     * @param name Header name (case-insensitive).
     * @return true when the header has been set via `setHeader()` or
     *         `writeHead()`.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   if (res.hasHeader("etag")) {
     *       res.removeHeader("etag");
     *   }
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#responsehasheadername
     */
    bool hasHeader(const std::string& name) const;

    /**
     * @brief Writes the response head (status + headers).
     * @param statusCode Status code.
     * @param headers Optional additional headers.
     * @throws Error if the status or headers violate HTTP/2 response header
     *         rules.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   res.writeHead(200, {{"content-type", "text/plain"}});
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
     * @see Http3ServerResponse::writeHead
     */
    void writeHead(int statusCode, const http::Headers& headers = {});

    /**
     * @brief Writes a chunk to the response, auto-flushing headers if needed.
     *
     * The first write triggers `writeHead()` with the currently-staged
     * `statusCode` and headers when headers have not yet been sent.
     *
     * @param data Data to write.
     * @param callback Called when the write completes.
     * @return false if the response has already finished; otherwise true.
     * @note This return value does not indicate backpressure or flush status.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   res.write("chunk");
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#responsewritechunk-encoding-callback
     */
    bool write(const std::string& data, std::function<void()> callback = nullptr);

    /**
     * @brief Ends the response, flushing headers if needed and emitting 'finish'.
     *
     * If headers have not yet been sent, this auto-calls `writeHead()` with the
     * currently-staged status code and headers. On completion the internal
     * `finished` flag is set and the 'finish' event is emitted on this
     * response. Subsequent calls to `write()` and `end()` become no-ops.
     *
     * @param data Optional final payload.
     * @param callback Called when the underlying stream end completes.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   res.end("done");
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#responseenddata-encoding-callback
     */
    void end(const std::string& data = "", std::function<void()> callback = nullptr);

    /**
     * @brief Adds trailing headers.
     * @param headers Trailing headers.
     * @throws Error if the trailing headers contain any pseudo-headers.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   res.addTrailers({{"x-checksum", "abc123"}});
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
     */
    void addTrailers(const http::Headers& headers);

    /**
     * @brief Sets timeout for the underlying stream.
     * @param msecs Timeout in milliseconds.
     * @param callback Called when timeout fires.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   res.setTimeout(15'000);
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
     */
    void setTimeout(uint32_t msecs, std::function<void()> callback = nullptr);

    /**
     * @brief Creates a server push stream.
     *
     * The callback receives the error status and a wrapped response for the
     * promised push stream.
     *
     * @param headers Headers used for the push promise.
     * @param callback Called with `(error, response)`.
     *
     * @par Example
     * @code{.cpp}
     *   http2::Http2ServerResponse res;
     *   res.createPushResponse({{":path", "/app.css"}}, [](const Error* err, http2::Http2ServerResponse push) {
     *       if (!err) {
     *           push.writeHead(200, {{"content-type", "text/css"}});
     *           push.end("body { color: #222; }");
     *       }
     *   });
     * @endcode
     *
     * @see https://nodejs.org/api/http2.html#class-http2http2serverresponse
     * @see Http3ServerResponse
     */
    void createPushResponse(const http::Headers& headers,
                            std::function<void(const Error*, Http2ServerResponse)> callback);

private:
    ServerHttp2Stream stream_;
    http::Headers headers_;
    int statusCode_ = 200;
    bool headersSent_ = false;
    bool finished_ = false;
    events::EventEmitter* emitter_ = nullptr;
    std::unique_ptr<events::EventEmitter> ownedEmitter_;
};

// ════════════════════════════════════════════════════════════════════════
// Module-level factory functions
// ════════════════════════════════════════════════════════════════════════

/**
 * @brief Creates an HTTP/2 server (cleartext).
 *
 * @param ctx The event context to use.
 * @param options Server options.
 * @param onStream Called when a new stream is received.
 * @return An Http2Server instance.
 *
 * @par Example
 * @code{.cpp}
 *   polycpp::EventContext ctx;
 *   auto server = http2::createServer(ctx, {}, [](auto stream, auto headers) {
 *       stream.respond({{":status", "200"}});
 *       stream.end("Hello World");
 *   });
 *   server.listen(8080);
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#http2createserveroptions-onrequesthandler
 * @see Http2Server
 * @see ServerHttp2Stream
 * @see Http2ServerRequest
 * @see Http2ServerResponse
 */
Http2Server createServer(EventContext& ctx,
                         const ServerOptions& options = {},
                         std::function<void(ServerHttp2Stream, http::Headers)> onStream = nullptr);

/**
 * @brief Creates an HTTP/2 secure server (TLS).
 *
 * @param ctx The event context to use.
 * @param options Secure server options. The current implementation consumes
 * `cert`, `key`, and `ca`; the remaining TLS-shaped fields are accepted but
 * not yet wired into server behavior.
 * @param onStream Called when a new stream is received.
 * @return An Http2SecureServer wrapping a new server implementation.
 * @note TLS material is configured when present, but it is not required up
 * front. ALPN is currently hard-wired to `h2`.
 *
 * @par Example
 * @code{.cpp}
 *   polycpp::EventContext ctx;
 *   http2::SecureServerOptions options;
 *   auto server = http2::createSecureServer(ctx, options, [](auto stream, auto) {
 *       stream.respond(200, {{"content-type", "text/plain"}});
 *       stream.end("ok");
 *   });
 *   server.listen(8443);
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#http2createsecureserveroptions-onrequesthandler
 * @see Http2SecureServer
 * @see Http2Server
 * @see ServerHttp2Stream
 * @see Http2ServerRequest
 * @see Http2ServerResponse
 */
Http2SecureServer createSecureServer(
    EventContext& ctx,
    const SecureServerOptions& options,
    std::function<void(ServerHttp2Stream, http::Headers)> onStream = nullptr);

/**
 * @brief Creates an HTTP/2 client session by connecting to a server.
 *
 * @param ctx The event context to use.
 * @param authority The authority to connect to (e.g., "http://localhost:8080").
 * @param options Client session options. The current implementation derives
 * host, port, and secure/plain mode from `authority`; `protocol` and
 * `maxReservedRemoteStreams` are accepted in the public type but are not
 * consulted by this connect path.
 * @param callback Called after the TCP connect succeeds and before the read loop starts.
 * @return A ClientHttp2Session wrapper that may still be connecting asynchronously.
 * @note This implementation uses a plain TCP socket and does not perform a TLS handshake.
 * @throws std::invalid_argument if the authority port cannot be parsed as a
 *         number.
 * @throws std::out_of_range if the authority port is outside the `uint16_t`
 *         range.
 *
 * @par Example
 * @code{.cpp}
 *   polycpp::EventContext ctx;
 *   http2::ClientSessionOptions options;
 *   auto session = http2::connect(ctx, "http://localhost:8080", options, [](http2::ClientHttp2Session& s) {
 *       auto stream = s.request(http::Headers{
 *           {":method", "GET"},
 *           {":path", "/"},
 *           {":scheme", "http"},
 *           {":authority", "localhost:8080"}
 *       });
 *   });
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#http2connectauthority-options-listener
 * @see ClientHttp2Session
 * @see Http2Session
 * @see ClientHttp2Stream
 */
ClientHttp2Session connect(EventContext& ctx,
                           const std::string& authority,
                           const ClientSessionOptions& options = {},
                           std::function<void(ClientHttp2Session&)> callback = nullptr);

/**
 * @brief Overload for secure client connections.
 *
 * @param ctx The event context to use.
 * @param authority The authority to connect to.
 * @param options Secure client session options. TLS-specific fields are
 * currently ignored; this overload reuses the same plain TCP path as the base
 * overload.
 * @param callback Called when the session is connected.
 * @return A ClientHttp2Session wrapper using the same plain TCP path as the base overload.
 * @note This overload preserves a caller-supplied `options.protocol` and only
 * defaults it to `https:` when absent before delegating. The delegated connect
 * path still derives secure/plain mode from `authority`, not from the options
 * object.
 * @throws std::invalid_argument if the authority port cannot be parsed as a
 *         number.
 * @throws std::out_of_range if the authority port is outside the `uint16_t`
 *         range.
 *
 * @par Example
 * @code{.cpp}
 *   polycpp::EventContext ctx;
 *   http2::SecureClientSessionOptions options;
 *   auto session = http2::connect(ctx, "https://example.com", options);
 * @endcode
 *
 * @see https://nodejs.org/api/http2.html#http2connectauthority-options-listener
 * @see ClientHttp2Session
 * @see Http2Session
 * @see ClientHttp2Stream
 */
ClientHttp2Session connect(EventContext& ctx,
                           const std::string& authority,
                           const SecureClientSessionOptions& options,
                           std::function<void(ClientHttp2Session&)> callback = nullptr);

/** @} */ // end of http2 group

} // namespace http2

namespace events {

/**
 * @brief Specialization of polycpp::events::ErrorEventOf for all HTTP/2 handle
 *        classes.
 *
 * Binds the 'error' typed event for the Http2Stream, Http2Session, Http2Server,
 * Http2SecureServer, Http2ServerRequest, and Http2ServerResponse hierarchies so
 * that generic event helpers resolve `error` to a `TypedEvent<"error", const
 * polycpp::Error&>`.
 *
 * @tparam Emitter Any class derived from one of the HTTP/2 handle types.
 *
 * @see polycpp::events::TypedEvent
 */
template <typename Emitter>
    requires (std::is_base_of_v<http2::Http2Stream, std::remove_cvref_t<Emitter>> ||
              std::is_base_of_v<http2::Http2Session, std::remove_cvref_t<Emitter>> ||
              std::is_base_of_v<http2::Http2Server, std::remove_cvref_t<Emitter>> ||
              std::is_base_of_v<http2::Http2SecureServer, std::remove_cvref_t<Emitter>> ||
              std::is_base_of_v<http2::Http2ServerRequest, std::remove_cvref_t<Emitter>> ||
              std::is_base_of_v<http2::Http2ServerResponse, std::remove_cvref_t<Emitter>>)
struct ErrorEventOf<Emitter> {
    static constexpr TypedEvent<"error", const ::polycpp::Error&> value{};
};

} // namespace events

} // namespace polycpp
