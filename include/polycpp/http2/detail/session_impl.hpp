#pragma once
#include <polycpp/config.hpp>

/**
 * @file session_impl.hpp
 * @brief HTTP/2 session implementation using nghttp2.
 *
 * Http2SessionImpl manages the nghttp2_session, handles callbacks,
 * and bridges between the transport layer and the HTTP/2 framing layer.
 *
 */

#include <polycpp/http2/http2.hpp>
#include <polycpp/http2/detail/nghttp2_allocator.hpp>
#include <polycpp/events.hpp>
#include <polycpp/http2/events.hpp>
#include <polycpp/core/error.hpp>
#include <polycpp/buffer.hpp>

#include <nghttp2/nghttp2.h>

#include <any>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <deque>
#include <functional>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// POSIX headers for fd-based responses (respondWithFD/respondWithFile)
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <sys/stat.h>

namespace polycpp {
namespace http2 {
namespace impl {

// Forward declaration
class Http2StreamImpl;

/**
 * @brief Internal implementation for HTTP/2 sessions.
 *
 * Wraps an nghttp2_session and provides all protocol logic. The public
 * Http2Session handle class delegates to this implementation.
 *
 */
class Http2SessionImpl : public std::enable_shared_from_this<Http2SessionImpl> {
public:
    /**
     * @brief Constructs a session implementation.
     * @param isServer true for server sessions, false for client sessions.
     * @param options Session options.
     */
    inline Http2SessionImpl(bool isServer, const SessionOptions& options = {});
    inline ~Http2SessionImpl();

    // Non-copyable
    Http2SessionImpl(const Http2SessionImpl&) = delete;
    Http2SessionImpl& operator=(const Http2SessionImpl&) = delete;

    // ── Initialization ─────────────────────────────────────────────

    /** @brief Initialize the nghttp2 session. Must be called after construction. */
    inline void initialize();

    // ── Properties ─────────────────────────────────────────────────

    inline bool isServer() const noexcept { return isServer_; }
    inline bool closed() const noexcept { return closed_; }
    inline bool destroyed() const noexcept { return destroyed_; }
    inline bool encrypted() const noexcept { return encrypted_; }
    inline void setEncrypted(bool enc) noexcept { encrypted_ = enc; }
    inline const std::string& alpnProtocol() const noexcept { return alpnProtocol_; }
    inline void setAlpnProtocol(const std::string& proto) { alpnProtocol_ = proto; }
    inline const std::string& originSet() const noexcept { return originSet_; }
    inline void setOriginSet(const std::string& origin) { originSet_ = origin; }
    inline int type() const noexcept { return isServer_ ? 0 : 1; }

    inline Settings localSettings() const;
    inline Settings remoteSettings() const;
    inline bool pendingSettingsAck() const noexcept { return pendingSettingsAck_; }
    inline SessionState state() const;

    // ── Event Emitter ──────────────────────────────────────────────

    inline events::EventEmitter& emitter() noexcept { return emitter_; }
    inline const events::EventEmitter& emitter() const noexcept { return emitter_; }

    // ── Methods ────────────────────────────────────────────────────

    inline void close(std::function<void()> callback);
    inline void destroy(const Error* error, uint32_t code);
    inline void goaway(uint32_t code, int32_t lastStreamID, const std::string& opaqueData);
    inline bool ping(const std::string& payload,
                     std::function<void(const Error*, double, const std::string&)> callback);
    inline void sendSettings(const Settings& settings,
                             std::function<void(const Error*)> callback);
    inline void setTimeout(uint32_t msecs, std::function<void()> callback);
    inline void setLocalWindowSize(int32_t windowSize);

    // ── Stream management ──────────────────────────────────────────

    /** @brief Get or create a stream impl for a given stream ID. */
    inline std::shared_ptr<Http2StreamImpl> getStream(int32_t streamId);

    /** @brief Remove a stream from the session. */
    inline void removeStream(int32_t streamId);

    /** @brief Submit a request (client side). Returns stream ID. */
    inline int32_t submitRequest(const http::Headers& headers, bool endStream);

    /** @brief Submit a response (server side). */
    inline void submitResponse(int32_t streamId, const http::Headers& headers, bool endStream);

    /** @brief Submit additional (informational) headers. */
    inline void submitInfo(int32_t streamId, const http::Headers& headers);

    /** @brief Submit a push promise. Returns the promised stream ID. */
    inline int32_t submitPushPromise(int32_t streamId, const http::Headers& headers);

    /** @brief Submit RST_STREAM for a stream. */
    inline void submitRstStream(int32_t streamId, uint32_t code);

    /** @brief Submit trailing headers. */
    inline void submitTrailers(int32_t streamId, const http::Headers& headers);

    /** @brief Submit data on a stream. Returns false when backpressure applies. */
    inline bool submitData(int32_t streamId, const std::string& data, bool endStream);

    /** @brief Submit an ALTSVC frame. Server-side only. */
    inline void submitAltsvc(const std::string& alt, const std::string& originOrStream);

    /** @brief Submit an ORIGIN frame. Server-side only. */
    inline void submitOrigin(const std::vector<std::string>& origins);

    /**
     * @brief Submit a response that reads body data from a file descriptor.
     *
     * The fd is read lazily in onDataSourceRead(): only the bytes nghttp2
     * can send are read from disk, preventing OOM for large files.
     *
     * @param streamId  The stream to respond on.
     * @param headers   Response headers (must include ":status").
     * @param fd        Open file descriptor to read from.
     * @param options   Offset, length, and waitForTrailers options.
     * @param ownsFd    If true, the fd will be closed when the stream closes.
     */
    inline void submitResponseWithFD(int32_t streamId, const http::Headers& headers,
                                     int fd, const StreamFileResponseOptions& options,
                                     bool ownsFd);

    // ── Transport interface ────────────────────────────────────────

    /** @brief Feed received data from the transport into nghttp2. */
    inline int feedData(const uint8_t* data, size_t len);

    /** @brief Send pending frames to the transport. Returns serialized bytes. */
    inline std::string serializedOutput();

    /** @brief Check if there is pending output. */
    inline bool hasPendingOutput() const noexcept { return !outputBuffer_.empty(); }

    /** @brief Get and clear the output buffer. */
    inline std::string consumeOutput() noexcept;

    // ── nghttp2 callbacks (called by the static callback bridge) ───

    inline int onFrameRecv(const nghttp2_frame* frame);
    inline int onFrameNotSend(const nghttp2_frame* frame, int errorCode);
    inline int onStreamClose(int32_t streamId, uint32_t errorCode);
    inline int onBeginHeaders(const nghttp2_frame* frame);
    inline int onHeader(const nghttp2_frame* frame,
                        const uint8_t* name, size_t namelen,
                        const uint8_t* value, size_t valuelen,
                        uint8_t flags);
    inline int onDataChunkRecv(uint8_t flags, int32_t streamId,
                               const uint8_t* data, size_t len);
    inline ssize_t onSend(const uint8_t* data, size_t length, int flags);
    inline ssize_t onDataSourceRead(int32_t streamId, uint8_t* buf,
                                    size_t length, uint32_t* dataFlags);

private:
    bool isServer_;
    bool closed_ = false;
    bool destroyed_ = false;
    bool encrypted_ = false;
    bool pendingSettingsAck_ = false;
    std::string alpnProtocol_ = "h2";
    std::string originSet_;
    SessionOptions options_;
    Settings localSettings_;
    Settings remoteSettings_;

    nghttp2_session* session_ = nullptr;
    events::EventEmitter emitter_;

    // Stream map: streamId -> stream impl
    std::unordered_map<int32_t, std::shared_ptr<Http2StreamImpl>> streams_;

    // Output buffer for serialized frames
    std::string outputBuffer_;

    // Per-stream high-water mark for outbound data buffering (bytes).
    // Matches Node.js Http2Stream writableHighWaterMark default.
    static constexpr size_t kPendingDataHighWaterMark = 16384;

    // Pending data for data source reads (per stream)
    std::unordered_map<int32_t, std::deque<std::string>> pendingData_;
    std::unordered_map<int32_t, bool> pendingDataEnd_;

    // Ping tracking
    struct PingEntry {
        std::string payload;
        std::chrono::steady_clock::time_point sentAt;
        std::function<void(const Error*, double, const std::string&)> callback;
    };
    std::vector<PingEntry> pendingPings_;

    // Settings ACK callback
    std::function<void(const Error*)> settingsCallback_;

    // Close callback
    std::function<void()> closeCallback_;

    // Timeout
    uint32_t timeoutMs_ = 0;

    // ── Internal helpers ───────────────────────────────────────────

    /**
     * @brief Owns nghttp2_nv array together with the strings it points into.
     *
     * The nva pointers reference data inside pairs. Both are moved together
     * so the pointers remain valid across function returns (vector move
     * preserves element addresses).
     */
    struct NvHolder {
        std::vector<std::pair<std::string, std::string>> pairs;
        std::vector<nghttp2_nv> nva;
    };

    inline void setupCallbacks(nghttp2_session_callbacks* callbacks);
    /// Convert http::Headers to an NvHolder that owns string data and nghttp2_nv array.
    inline NvHolder headersToNv(const http::Headers& headers);
    inline void flushOutput();
    inline size_t pendingDataSize_(int32_t streamId) const;

    // ── Pseudo-header validation helpers (also exposed as free functions) ──
    inline void validateRequestHeaders_(const http::Headers& headers);
    inline void validateResponseHeaders_(const http::Headers& headers);
    inline void validateTrailers_(const http::Headers& headers);
};

// ════════════════════════════════════════════════════════════════════════
// Http2StreamImpl
// ════════════════════════════════════════════════════════════════════════

/**
 * @brief Internal implementation for HTTP/2 streams.
 *
 */
class Http2StreamImpl : public std::enable_shared_from_this<Http2StreamImpl> {
public:
    inline Http2StreamImpl(int32_t streamId, std::shared_ptr<Http2SessionImpl> session) noexcept;
    inline ~Http2StreamImpl() = default;

    // Non-copyable
    Http2StreamImpl(const Http2StreamImpl&) = delete;
    Http2StreamImpl& operator=(const Http2StreamImpl&) = delete;

    // ── Properties ─────────────────────────────────────────────────

    inline int32_t id() const noexcept { return streamId_; }
    inline bool aborted() const noexcept { return aborted_; }
    inline bool closed() const noexcept { return closed_; }
    inline bool destroyed() const noexcept { return destroyed_; }
    inline bool endAfterHeaders() const noexcept { return endAfterHeaders_; }
    inline bool pending() const noexcept { return streamId_ == 0; }
    inline uint32_t rstCode() const noexcept { return rstCode_; }
    inline bool headersSent() const noexcept { return headersSent_; }
    inline bool pushAllowed() const;

    inline const http::Headers& sentHeaders() const noexcept { return sentHeaders_; }
    inline const std::vector<http::Headers>& sentInfoHeaders() const noexcept { return sentInfoHeaders_; }
    inline const http::Headers& sentTrailers() const noexcept { return sentTrailers_; }
    inline const http::Headers& receivedHeaders() const noexcept { return receivedHeaders_; }
    inline const http::Headers& receivedTrailers() const noexcept { return receivedTrailers_; }

    inline std::shared_ptr<Http2SessionImpl> session() const noexcept { return session_.lock(); }
    inline StreamState state() const;

    inline const std::optional<Http2RequestMeta>& requestMeta() const noexcept { return requestMeta_; }
    inline const std::optional<Http2ResponseMeta>& responseMeta() const noexcept { return responseMeta_; }

    // ── Event Emitter ──────────────────────────────────────────────

    inline events::EventEmitter& emitter() noexcept { return emitter_; }
    inline const events::EventEmitter& emitter() const noexcept { return emitter_; }

    // ── Methods ────────────────────────────────────────────────────

    inline void close(uint32_t code, std::function<void()> callback);
    inline void respond(const http::Headers& headers, const StreamResponseOptions& options);
    inline void respondWithFD(int fd, const http::Headers& headers,
                              const StreamFileResponseOptions& options);
    inline void respondWithFile(const std::string& path, const http::Headers& headers,
                                const StreamFileResponseOptions& options);
    inline void additionalHeaders(const http::Headers& headers);
    inline void pushStream(const http::Headers& headers,
                           std::function<void(const Error*, ServerHttp2Stream, const http::Headers&)> callback);
    inline void write(const std::string& data, std::function<void()> callback);
    inline void end(const std::string& data, std::function<void()> callback);
    inline void sendTrailers(const http::Headers& headers);
    inline void setTimeout(uint32_t msecs, std::function<void()> callback);

    // ── Internal setters (called by session callbacks) ─────────────

    inline void setStreamId(int32_t id) noexcept { streamId_ = id; }
    inline void setClosed(bool c) noexcept { closed_ = c; }
    inline void setAborted(bool a) noexcept { aborted_ = a; }
    inline void setDestroyed(bool d) noexcept { destroyed_ = d; }
    inline void setEndAfterHeaders(bool e) noexcept { endAfterHeaders_ = e; }
    inline void setRstCode(uint32_t code) noexcept { rstCode_ = code; }
    inline void setHeadersSent(bool h) noexcept { headersSent_ = h; }

    inline void addReceivedHeader(const std::string& name, const std::string& value);
    inline void addReceivedTrailer(const std::string& name, const std::string& value);
    inline void clearReceivedHeaders() {
        // Headers has no clear() method; reset via move-assignment from default instance
        receivedHeaders_ = http::Headers{};
        requestMeta_.reset();
        responseMeta_.reset();
        // Reset pseudo-header phase for the new header block
        receivingPseudo_ = true;
    }

    // ── Pseudo-header setters (called by session onHeader) ────────

    inline void setRequestMeta(const Http2RequestMeta& meta) noexcept { requestMeta_ = meta; }
    inline void setResponseMeta(const Http2ResponseMeta& meta) noexcept { responseMeta_ = meta; }
    inline void updateRequestMeta(const std::string& name, const std::string& value) {
        if (!requestMeta_.has_value()) return;
        auto& meta = requestMeta_.value();
        if (name == ":method") meta.method = value;
        else if (name == ":path") meta.path = value;
        else if (name == ":scheme") meta.scheme = value;
        else if (name == ":authority") meta.authority = value;
        else if (name == ":protocol") meta.protocol = value;
    }

    inline void onData(const uint8_t* data, size_t len);
    inline void onEnd();
    inline void onStreamClose(uint32_t errorCode);

    // ── Flags for trailers support ─────────────────────────────────

    inline bool wantTrailers() const noexcept { return wantTrailers_; }
    inline void setWantTrailers(bool w) noexcept { wantTrailers_ = w; }

    // ── Receive-side pseudo-header ordering tracking ───────────────
    //
    // RFC 9113 §8.3 requires that all pseudo-headers appear before regular
    // headers in a header block. While nghttp2 enforces this at the protocol
    // level, we track it here as a defense-in-depth measure and for debugging.

    /** @brief True while we're still in the pseudo-header section of the incoming header block. */
    inline bool receivingPseudo() const noexcept { return receivingPseudo_; }
    /** @brief Clear the pseudo-header phase (called when the first regular header arrives). */
    inline void setReceivingPseudo(bool v) noexcept { receivingPseudo_ = v; }

    // ── Setters for sent headers (used by session impl) ─────────────

    inline void addSentHeader(const std::string& name, const std::string& value) {
        // Use append() to record all values (multi-value headers like Set-Cookie)
        sentHeaders_.append(name, value);
    }
    inline void addSentInfoHeaders(const http::Headers& headers) {
        sentInfoHeaders_.push_back(headers);
    }
    inline void addSentTrailer(const std::string& name, const std::string& value) {
        // Use append() to record all values (multi-value trailers)
        sentTrailers_.append(name, value);
    }

    // ── File-descriptor data source (for respondWithFD/respondWithFile) ──

    /** @brief True if this stream has a file descriptor data source. */
    inline bool hasFdSource() const noexcept { return fdSource_ >= 0; }

    /** @brief The file descriptor to read body data from. -1 means none. */
    inline int fdSource() const noexcept { return fdSource_; }

    /** @brief Current byte offset into the fd for the next read. */
    inline int64_t fdCurrentOffset() const noexcept { return fdCurrentOffset_; }

    /** @brief Remaining bytes to read from the fd. -1 means read until EOF. */
    inline int64_t fdRemainingLength() const noexcept { return fdRemainingLength_; }

    /** @brief Whether this stream owns (should close) the fd on cleanup. */
    inline bool fdOwnsFd() const noexcept { return fdOwnsFd_; }

    /** @brief Set the file descriptor data source. */
    inline void setFdSource(int fd, int64_t offset, int64_t length, bool ownsFd) noexcept {
        fdSource_ = fd;
        fdCurrentOffset_ = offset;
        fdRemainingLength_ = length;
        fdOwnsFd_ = ownsFd;
    }

    /** @brief Advance the fd offset after reading bytes. */
    inline void advanceFdSource(int64_t bytesRead) noexcept {
        fdCurrentOffset_ += bytesRead;
        if (fdRemainingLength_ > 0) {
            fdRemainingLength_ -= bytesRead;
        }
    }

    /** @brief Close the fd if this stream owns it. */
    inline void closeFdSource();

private:
    int32_t streamId_;
    std::weak_ptr<Http2SessionImpl> session_;
    events::EventEmitter emitter_;

    bool aborted_ = false;
    bool closed_ = false;
    bool destroyed_ = false;
    bool endAfterHeaders_ = false;
    bool headersSent_ = false;
    bool wantTrailers_ = false;
    bool localClose_ = false;      ///< Local side has sent END_STREAM
    bool remoteClose_ = false;     ///< Remote side has sent END_STREAM
    /// True while still receiving pseudo-headers; flipped false on first regular header.
    bool receivingPseudo_ = true;
    uint32_t rstCode_ = 0;

    http::Headers sentHeaders_;
    std::vector<http::Headers> sentInfoHeaders_;
    http::Headers sentTrailers_;
    http::Headers receivedHeaders_;
    http::Headers receivedTrailers_;

    // Parsed pseudo-headers (separated from regular headers)
    std::optional<Http2RequestMeta> requestMeta_;
    std::optional<Http2ResponseMeta> responseMeta_;

    // Buffered received data
    std::string dataBuffer_;
    bool dataEnded_ = false;

    // File-descriptor data source for respondWithFD/respondWithFile.
    // fd < 0 means no fd source is active.
    int fdSource_ = -1;
    int64_t fdCurrentOffset_ = 0;
    int64_t fdRemainingLength_ = -1;  ///< -1 means read until EOF
    bool fdOwnsFd_ = false;           ///< If true, close fd on stream cleanup
};

// ════════════════════════════════════════════════════════════════════════
// nghttp2 callback bridge (static functions)
// ════════════════════════════════════════════════════════════════════════

namespace callbacks {

inline int onFrameRecvCallback(nghttp2_session* session,
                               const nghttp2_frame* frame,
                               void* userData) {
    auto* impl = static_cast<Http2SessionImpl*>(userData);
    return impl->onFrameRecv(frame);
}

inline int onFrameNotSendCallback(nghttp2_session* session,
                                  const nghttp2_frame* frame,
                                  int errorCode,
                                  void* userData) {
    auto* impl = static_cast<Http2SessionImpl*>(userData);
    return impl->onFrameNotSend(frame, errorCode);
}

inline int onStreamCloseCallback(nghttp2_session* session,
                                 int32_t streamId,
                                 uint32_t errorCode,
                                 void* userData) {
    auto* impl = static_cast<Http2SessionImpl*>(userData);
    return impl->onStreamClose(streamId, errorCode);
}

inline int onBeginHeadersCallback(nghttp2_session* session,
                                  const nghttp2_frame* frame,
                                  void* userData) {
    auto* impl = static_cast<Http2SessionImpl*>(userData);
    return impl->onBeginHeaders(frame);
}

inline int onHeaderCallback(nghttp2_session* session,
                            const nghttp2_frame* frame,
                            const uint8_t* name, size_t namelen,
                            const uint8_t* value, size_t valuelen,
                            uint8_t flags,
                            void* userData) {
    auto* impl = static_cast<Http2SessionImpl*>(userData);
    return impl->onHeader(frame, name, namelen, value, valuelen, flags);
}

inline int onDataChunkRecvCallback(nghttp2_session* session,
                                   uint8_t flags,
                                   int32_t streamId,
                                   const uint8_t* data, size_t len,
                                   void* userData) {
    auto* impl = static_cast<Http2SessionImpl*>(userData);
    return impl->onDataChunkRecv(flags, streamId, data, len);
}

inline ssize_t onSendCallback(nghttp2_session* session,
                               const uint8_t* data, size_t length,
                               int flags,
                               void* userData) {
    auto* impl = static_cast<Http2SessionImpl*>(userData);
    return impl->onSend(data, length, flags);
}

inline ssize_t onDataSourceReadCallback(nghttp2_session* session,
                                         int32_t streamId,
                                         uint8_t* buf, size_t length,
                                         uint32_t* dataFlags,
                                         nghttp2_data_source* source,
                                         void* userData) {
    auto* impl = static_cast<Http2SessionImpl*>(userData);
    return impl->onDataSourceRead(streamId, buf, length, dataFlags);
}

} // namespace callbacks

// ════════════════════════════════════════════════════════════════════════
// Pseudo-header validation free functions
// ════════════════════════════════════════════════════════════════════════

/**
 * @brief Validate request pseudo-headers before submitting to nghttp2.
 *
 * Checks that:
 * - `:method` is present (required for all requests)
 * - `:path` and `:scheme` are present (required for non-CONNECT)
 * - No unknown request pseudo-headers appear
 * - No duplicate pseudo-headers appear
 * - Pseudo-headers appear before regular headers (RFC 9113 §8.3)
 *
 * @param headers The request headers to validate.
 * @throws polycpp::Error with code ERR_HTTP2_INVALID_PSEUDOHEADER on failure.
 */
inline void validateRequestHeaders(const http::Headers& headers) {
    bool hasMethod = false;
    bool hasPath = false;
    bool hasScheme = false;
    bool hasAuthority = false;
    bool hasProtocol = false;
    bool pastPseudo = false;

    for (const auto& [name, value] : headers.raw()) {
        if (!name.empty() && name[0] == ':') {
            // Pseudo-header must appear before all regular headers (RFC 9113 §8.3)
            if (pastPseudo) {
                throw polycpp::Error(
                    "HTTP/2 pseudo-headers must appear before regular headers: " + name)
                    .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
            }
            if (name == ":method") {
                if (hasMethod) {
                    throw polycpp::Error("Duplicate pseudo-header: :method")
                        .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
                }
                hasMethod = true;
            } else if (name == ":path") {
                if (hasPath) {
                    throw polycpp::Error("Duplicate pseudo-header: :path")
                        .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
                }
                hasPath = true;
            } else if (name == ":scheme") {
                if (hasScheme) {
                    throw polycpp::Error("Duplicate pseudo-header: :scheme")
                        .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
                }
                hasScheme = true;
            } else if (name == ":authority") {
                // Optional, allowed once; no duplicate check required by spec
                hasAuthority = true;
            } else if (name == ":protocol") {
                // Extended CONNECT (RFC 8441), optional
                hasProtocol = true;
            } else {
                throw polycpp::Error("Unknown request pseudo-header: " + name)
                    .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
            }
        } else {
            pastPseudo = true;
        }
    }

    // :method is always required
    if (!hasMethod) {
        throw polycpp::Error("Missing required request pseudo-header: :method")
            .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
    }

    // :path and :scheme are required for non-CONNECT methods
    // (CONNECT uses :authority to identify the target host/port instead)
    auto methodOpt = headers.get(":method");
    const std::string method = methodOpt.value_or("");
    if (method != "CONNECT") {
        if (!hasPath) {
            throw polycpp::Error("Missing required request pseudo-header: :path")
                .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
        }
        if (!hasScheme) {
            throw polycpp::Error("Missing required request pseudo-header: :scheme")
                .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
        }
    }

    // Suppress unused-variable warnings for authority/protocol flags
    (void)hasAuthority;
    (void)hasProtocol;
}

/**
 * @brief Validate response pseudo-headers before submitting to nghttp2.
 *
 * Checks that:
 * - `:status` is present and contains a valid integer in the range 100-599
 * - No unknown response pseudo-headers appear
 * - No duplicate `:status` pseudo-header
 * - Pseudo-headers appear before regular headers (RFC 9113 §8.3)
 *
 * @param headers The response headers to validate.
 * @throws polycpp::Error with code ERR_HTTP2_INVALID_PSEUDOHEADER on failure.
 */
inline void validateResponseHeaders(const http::Headers& headers) {
    bool hasStatus = false;
    bool pastPseudo = false;

    for (const auto& [name, value] : headers.raw()) {
        if (!name.empty() && name[0] == ':') {
            if (pastPseudo) {
                throw polycpp::Error(
                    "HTTP/2 pseudo-headers must appear before regular headers: " + name)
                    .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
            }
            if (name == ":status") {
                if (hasStatus) {
                    throw polycpp::Error("Duplicate pseudo-header: :status")
                        .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
                }
                hasStatus = true;
                // Validate status code is numeric and in range 100-599
                try {
                    int code = std::stoi(value);
                    if (code < 100 || code > 599) {
                        throw polycpp::Error(
                            "Invalid :status value: " + value + " (must be 100-599)")
                            .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
                    }
                } catch (const std::invalid_argument&) {
                    throw polycpp::Error(
                        "Invalid :status value: " + value + " (not a number)")
                        .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
                } catch (const std::out_of_range&) {
                    throw polycpp::Error(
                        "Invalid :status value: " + value + " (out of range)")
                        .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
                }
            } else {
                throw polycpp::Error("Unknown response pseudo-header: " + name)
                    .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
            }
        } else {
            pastPseudo = true;
        }
    }

    if (!hasStatus) {
        throw polycpp::Error("Missing required response pseudo-header: :status")
            .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
    }
}

/**
 * @brief Validate that trailer headers contain no pseudo-headers.
 *
 * RFC 9113 §8.1 forbids pseudo-header fields in trailer sections.
 * Any header name beginning with ':' in trailers is a protocol error.
 *
 * @param headers The trailer headers to validate.
 * @throws polycpp::Error with code ERR_HTTP2_INVALID_PSEUDOHEADER if any
 *         pseudo-header is found.
 */
inline void validateTrailers(const http::Headers& headers) {
    for (const auto& [name, value] : headers.raw()) {
        if (!name.empty() && name[0] == ':') {
            throw polycpp::Error("Pseudo-headers are not allowed in trailers: " + name)
                .setCode("ERR_HTTP2_INVALID_PSEUDOHEADER");
        }
    }
}

// ════════════════════════════════════════════════════════════════════════
// Http2SessionImpl implementation
// ════════════════════════════════════════════════════════════════════════

inline Http2SessionImpl::Http2SessionImpl(bool isServer, const SessionOptions& options)
    : isServer_(isServer), options_(options) {
    // Apply default settings
    localSettings_ = getDefaultSettings();
    if (options.settings.has_value()) {
        auto& s = options.settings.value();
        if (s.headerTableSize.has_value()) localSettings_.headerTableSize = s.headerTableSize;
        if (s.enablePush.has_value()) localSettings_.enablePush = s.enablePush;
        if (s.initialWindowSize.has_value()) localSettings_.initialWindowSize = s.initialWindowSize;
        if (s.maxFrameSize.has_value()) localSettings_.maxFrameSize = s.maxFrameSize;
        if (s.maxConcurrentStreams.has_value()) localSettings_.maxConcurrentStreams = s.maxConcurrentStreams;
        if (s.maxHeaderListSize.has_value()) localSettings_.maxHeaderListSize = s.maxHeaderListSize;
        if (s.enableConnectProtocol.has_value()) localSettings_.enableConnectProtocol = s.enableConnectProtocol;
    }
    // Remote settings start as defaults
    remoteSettings_ = getDefaultSettings();
}

inline Http2SessionImpl::~Http2SessionImpl() {
    if (session_) {
        nghttp2_session_del(session_);
        session_ = nullptr;
    }
}

inline void Http2SessionImpl::initialize() {
    nghttp2_session_callbacks* cbs = nullptr;
    nghttp2_session_callbacks_new(&cbs);

    nghttp2_session_callbacks_set_send_callback(cbs, callbacks::onSendCallback);
    nghttp2_session_callbacks_set_on_frame_recv_callback(cbs, callbacks::onFrameRecvCallback);
    nghttp2_session_callbacks_set_on_frame_not_send_callback(cbs, callbacks::onFrameNotSendCallback);
    nghttp2_session_callbacks_set_on_stream_close_callback(cbs, callbacks::onStreamCloseCallback);
    nghttp2_session_callbacks_set_on_begin_headers_callback(cbs, callbacks::onBeginHeadersCallback);
    nghttp2_session_callbacks_set_on_header_callback(cbs, callbacks::onHeaderCallback);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(cbs, callbacks::onDataChunkRecvCallback);

    nghttp2_option* opts = nullptr;
    nghttp2_option_new(&opts);
    // Disable automatic WINDOW_UPDATE
    nghttp2_option_set_no_auto_window_update(opts, 0);

    if (isServer_) {
        nghttp2_session_server_new2(&session_, cbs, this, opts);
    } else {
        nghttp2_session_client_new2(&session_, cbs, this, opts);
    }

    nghttp2_option_del(opts);
    nghttp2_session_callbacks_del(cbs);

    // Submit initial local settings
    std::vector<nghttp2_settings_entry> entries;
    if (localSettings_.headerTableSize.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_HEADER_TABLE_SIZE, localSettings_.headerTableSize.value()});
    }
    // SETTINGS_ENABLE_PUSH is client-to-server only (RFC 9113 §6.5.2).
    // A server MUST NOT send this setting.
    if (!isServer_ && localSettings_.enablePush.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_ENABLE_PUSH, localSettings_.enablePush.value() ? 1u : 0u});
    }
    if (localSettings_.maxConcurrentStreams.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, localSettings_.maxConcurrentStreams.value()});
    }
    if (localSettings_.initialWindowSize.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, localSettings_.initialWindowSize.value()});
    }
    if (localSettings_.maxFrameSize.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_MAX_FRAME_SIZE, localSettings_.maxFrameSize.value()});
    }
    if (localSettings_.maxHeaderListSize.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE, localSettings_.maxHeaderListSize.value()});
    }
    if (localSettings_.enableConnectProtocol.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL, localSettings_.enableConnectProtocol.value() ? 1u : 0u});
    }

    if (!entries.empty()) {
        nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE,
                                entries.data(), entries.size());
    }

    // For client sessions, send the connection preface
    if (!isServer_) {
        // nghttp2 handles the magic preface automatically for client sessions
    }

    // Flush any pending output from settings submission
    flushOutput();
}

inline Settings Http2SessionImpl::localSettings() const {
    return localSettings_;
}

inline Settings Http2SessionImpl::remoteSettings() const {
    if (!session_) return remoteSettings_;

    Settings s;
    s.headerTableSize = nghttp2_session_get_remote_settings(session_, NGHTTP2_SETTINGS_HEADER_TABLE_SIZE);
    s.enablePush = nghttp2_session_get_remote_settings(session_, NGHTTP2_SETTINGS_ENABLE_PUSH) != 0;
    s.maxConcurrentStreams = nghttp2_session_get_remote_settings(session_, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
    s.initialWindowSize = nghttp2_session_get_remote_settings(session_, NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE);
    s.maxFrameSize = nghttp2_session_get_remote_settings(session_, NGHTTP2_SETTINGS_MAX_FRAME_SIZE);
    s.maxHeaderListSize = nghttp2_session_get_remote_settings(session_, NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE);
    s.enableConnectProtocol = nghttp2_session_get_remote_settings(session_, NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL) != 0;
    return s;
}

inline SessionState Http2SessionImpl::state() const {
    SessionState st;
    if (!session_) return st;

    st.effectiveLocalWindowSize = nghttp2_session_get_effective_local_window_size(session_);
    st.effectiveRecvDataLength = nghttp2_session_get_effective_recv_data_length(session_);
    st.nextStreamID = nghttp2_session_get_next_stream_id(session_);
    st.localWindowSize = nghttp2_session_get_local_window_size(session_);
    st.lastProcStreamID = nghttp2_session_get_last_proc_stream_id(session_);
    st.remoteWindowSize = nghttp2_session_get_remote_window_size(session_);
    st.outboundQueueSize = nghttp2_session_get_outbound_queue_size(session_);
    st.deflateDynamicTableSize = nghttp2_session_get_hd_deflate_dynamic_table_size(session_);
    st.inflateDynamicTableSize = nghttp2_session_get_hd_inflate_dynamic_table_size(session_);
    return st;
}

inline void Http2SessionImpl::close(std::function<void()> callback) {
    if (closed_) {
        if (callback) callback();
        return;
    }
    closed_ = true;
    closeCallback_ = std::move(callback);

    // Send GOAWAY if session is still alive
    if (session_ && !destroyed_) {
        goaway(constants::NGHTTP2_NO_ERROR, 0, "");
    }

    emitter_.emit(event::Close);

    if (closeCallback_) {
        closeCallback_();
        closeCallback_ = nullptr;
    }
}

inline void Http2SessionImpl::destroy(const Error* error, uint32_t code) {
    if (destroyed_) return;
    destroyed_ = true;
    closed_ = true;

    if (error) {
        emitter_.emit(event::Error_, *error);
    }

    // Close all streams
    for (auto& [id, stream] : streams_) {
        stream->setClosed(true);
        stream->setDestroyed(true);
        stream->emitter().emit(event::Close);
    }
    streams_.clear();
    pendingData_.clear();
    pendingDataEnd_.clear();

    if (session_) {
        nghttp2_session_del(session_);
        session_ = nullptr;
    }

    emitter_.emit(event::Close);
}

inline void Http2SessionImpl::goaway(uint32_t code, int32_t lastStreamID,
                                     const std::string& opaqueData) {
    if (!session_ || destroyed_) return;

    int32_t lastId = lastStreamID;
    if (lastId == 0) {
        lastId = nghttp2_session_get_last_proc_stream_id(session_);
    }

    const uint8_t* opaque = opaqueData.empty() ? nullptr :
        reinterpret_cast<const uint8_t*>(opaqueData.data());

    nghttp2_submit_goaway(session_, NGHTTP2_FLAG_NONE, lastId, code,
                          opaque, opaqueData.size());
    flushOutput();
}

inline bool Http2SessionImpl::ping(const std::string& payload,
                                   std::function<void(const Error*, double, const std::string&)> callback) {
    if (!session_ || destroyed_ || closed_) return false;

    // Check outstanding pings limit
    uint32_t maxPings = options_.maxOutstandingPings.value_or(10);
    if (pendingPings_.size() >= maxPings) return false;

    uint8_t pingData[8] = {0};
    if (!payload.empty()) {
        size_t copyLen = std::min(payload.size(), size_t(8));
        std::memcpy(pingData, payload.data(), copyLen);
    }

    int rv = nghttp2_submit_ping(session_, NGHTTP2_FLAG_NONE, pingData);
    if (rv != 0) return false;

    PingEntry entry;
    entry.payload = std::string(reinterpret_cast<char*>(pingData), 8);
    entry.sentAt = std::chrono::steady_clock::now();
    entry.callback = std::move(callback);
    pendingPings_.push_back(std::move(entry));

    flushOutput();
    return true;
}

inline void Http2SessionImpl::sendSettings(const Settings& settings,
                                           std::function<void(const Error*)> callback) {
    if (!session_ || destroyed_ || closed_) return;

    std::vector<nghttp2_settings_entry> entries;
    if (settings.headerTableSize.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_HEADER_TABLE_SIZE, settings.headerTableSize.value()});
        localSettings_.headerTableSize = settings.headerTableSize;
    }
    if (settings.enablePush.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_ENABLE_PUSH, settings.enablePush.value() ? 1u : 0u});
        localSettings_.enablePush = settings.enablePush;
    }
    if (settings.maxConcurrentStreams.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, settings.maxConcurrentStreams.value()});
        localSettings_.maxConcurrentStreams = settings.maxConcurrentStreams;
    }
    if (settings.initialWindowSize.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, settings.initialWindowSize.value()});
        localSettings_.initialWindowSize = settings.initialWindowSize;
    }
    if (settings.maxFrameSize.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_MAX_FRAME_SIZE, settings.maxFrameSize.value()});
        localSettings_.maxFrameSize = settings.maxFrameSize;
    }
    if (settings.maxHeaderListSize.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE, settings.maxHeaderListSize.value()});
        localSettings_.maxHeaderListSize = settings.maxHeaderListSize;
    }
    if (settings.enableConnectProtocol.has_value()) {
        entries.push_back({NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL, settings.enableConnectProtocol.value() ? 1u : 0u});
        localSettings_.enableConnectProtocol = settings.enableConnectProtocol;
    }

    if (!entries.empty()) {
        pendingSettingsAck_ = true;
        settingsCallback_ = std::move(callback);
        nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE,
                                entries.data(), entries.size());
        flushOutput();
    } else if (callback) {
        callback(nullptr);
    }
}

inline void Http2SessionImpl::setTimeout(uint32_t msecs, std::function<void()> callback) {
    timeoutMs_ = msecs;
    // In a real implementation, we'd set up a timer here
    if (callback && msecs > 0) {
        emitter_.once(event::Timeout, [cb = std::move(callback)]() {
            cb();
        });
    }
}

inline void Http2SessionImpl::setLocalWindowSize(int32_t windowSize) {
    if (!session_ || destroyed_) return;
    nghttp2_session_set_local_window_size(session_, NGHTTP2_FLAG_NONE, 0, windowSize);
    flushOutput();
}

// ── Stream management ──────────────────────────────────────────────

inline std::shared_ptr<Http2StreamImpl> Http2SessionImpl::getStream(int32_t streamId) {
    auto it = streams_.find(streamId);
    if (it != streams_.end()) return it->second;

    auto stream = std::make_shared<Http2StreamImpl>(streamId, shared_from_this());
    streams_[streamId] = stream;
    return stream;
}

inline void Http2SessionImpl::removeStream(int32_t streamId) {
    streams_.erase(streamId);
    pendingData_.erase(streamId);
    pendingDataEnd_.erase(streamId);
}

inline Http2SessionImpl::NvHolder Http2SessionImpl::headersToNv(const http::Headers& headers) {
    // Build NvHolder so that nva pointers into pairs survive the function return.
    // toHttp2Pairs() expands multi-value headers into separate pairs and lowercases names.
    NvHolder holder;
    holder.pairs = headers.toHttp2Pairs();
    holder.nva.reserve(holder.pairs.size());
    for (const auto& [name, value] : holder.pairs) {
        nghttp2_nv nv;
        nv.name = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(name.data()));
        nv.namelen = name.size();
        nv.value = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(value.data()));
        nv.valuelen = value.size();
        // Respect HPACK no-index flag for sensitive headers (e.g., Authorization)
        nv.flags = headers.isSensitive(name) ? NGHTTP2_NV_FLAG_NO_INDEX
                                             : NGHTTP2_NV_FLAG_NONE;
        holder.nva.push_back(nv);
    }
    return holder;
}

// Private validation method bodies (delegate to free functions above)
inline void Http2SessionImpl::validateRequestHeaders_(const http::Headers& headers) {
    validateRequestHeaders(headers);
}
inline void Http2SessionImpl::validateResponseHeaders_(const http::Headers& headers) {
    validateResponseHeaders(headers);
}
inline void Http2SessionImpl::validateTrailers_(const http::Headers& headers) {
    validateTrailers(headers);
}

inline int32_t Http2SessionImpl::submitRequest(const http::Headers& headers, bool endStream) {
    if (!session_ || destroyed_ || closed_) return -1;

    // Validate pseudo-headers before passing to nghttp2 for clear error messages
    validateRequestHeaders_(headers);

    auto holder = headersToNv(headers);

    nghttp2_data_provider* dpPtr = nullptr;
    nghttp2_data_provider dp;
    if (!endStream) {
        dp.source.ptr = this;
        dp.read_callback = callbacks::onDataSourceReadCallback;
        dpPtr = &dp;
    }

    int32_t streamId = nghttp2_submit_request(session_, nullptr,
                                              holder.nva.data(), holder.nva.size(),
                                              dpPtr, this);
    if (streamId < 0) return -1;

    auto stream = getStream(streamId);
    // Store the sent headers using pre-expanded pairs (preserves multi-value headers)
    for (const auto& [name, value] : holder.pairs) {
        stream->addSentHeader(name, value);
    }
    stream->setHeadersSent(true);

    if (endStream) {
        pendingDataEnd_[streamId] = true;
    }

    flushOutput();
    return streamId;
}

inline void Http2SessionImpl::submitResponse(int32_t streamId,
                                             const http::Headers& headers,
                                             bool endStream) {
    if (!session_ || destroyed_ || closed_) return;

    // Validate pseudo-headers before passing to nghttp2 for clear error messages
    validateResponseHeaders_(headers);

    auto holder = headersToNv(headers);

    nghttp2_data_provider* dpPtr = nullptr;
    nghttp2_data_provider dp;
    if (!endStream) {
        dp.source.ptr = this;
        dp.read_callback = callbacks::onDataSourceReadCallback;
        dpPtr = &dp;
    }

    nghttp2_submit_response(session_, streamId,
                            holder.nva.data(), holder.nva.size(), dpPtr);

    auto stream = getStream(streamId);
    for (const auto& [name, value] : holder.pairs) {
        stream->addSentHeader(name, value);
    }
    stream->setHeadersSent(true);

    if (endStream) {
        pendingDataEnd_[streamId] = true;
    }

    flushOutput();
}

inline void Http2SessionImpl::submitInfo(int32_t streamId, const http::Headers& headers) {
    if (!session_ || destroyed_ || closed_) return;

    // Informational (1xx) responses must also have a valid :status pseudo-header
    validateResponseHeaders_(headers);

    auto holder = headersToNv(headers);
    nghttp2_submit_headers(session_, NGHTTP2_FLAG_NONE, streamId, nullptr,
                           holder.nva.data(), holder.nva.size(), nullptr);

    auto stream = getStream(streamId);
    stream->addSentInfoHeaders(headers);

    flushOutput();
}

inline int32_t Http2SessionImpl::submitPushPromise(int32_t streamId,
                                                   const http::Headers& headers) {
    if (!session_ || destroyed_ || closed_ || !isServer_) return -1;

    auto holder = headersToNv(headers);
    int32_t promisedId = nghttp2_submit_push_promise(session_, NGHTTP2_FLAG_NONE,
                                                     streamId, holder.nva.data(), holder.nva.size(), nullptr);
    if (promisedId < 0) return -1;

    flushOutput();
    return promisedId;
}

inline void Http2SessionImpl::submitRstStream(int32_t streamId, uint32_t code) {
    if (!session_ || destroyed_) return;
    nghttp2_submit_rst_stream(session_, NGHTTP2_FLAG_NONE, streamId, code);
    flushOutput();
}

inline void Http2SessionImpl::submitTrailers(int32_t streamId, const http::Headers& headers) {
    if (!session_ || destroyed_ || closed_) return;

    // Trailers must not contain any pseudo-headers (RFC 9113 §8.1)
    validateTrailers_(headers);

    auto holder = headersToNv(headers);
    nghttp2_submit_trailer(session_, streamId, holder.nva.data(), holder.nva.size());

    auto stream = getStream(streamId);
    for (const auto& [name, value] : holder.pairs) {
        stream->addSentTrailer(name, value);
    }

    flushOutput();
}

inline bool Http2SessionImpl::submitData(int32_t streamId, const std::string& data, bool endStream) {
    if (!session_ || destroyed_ || closed_) return false;

    if (!data.empty()) {
        pendingData_[streamId].push_back(data);
    }
    if (endStream) {
        pendingDataEnd_[streamId] = true;
    }

    // Resume the stream if it was deferred
    nghttp2_session_resume_data(session_, streamId);
    flushOutput();
    return pendingDataSize_(streamId) < kPendingDataHighWaterMark;
}

inline void Http2SessionImpl::submitAltsvc(const std::string& alt,
                                           const std::string& originOrStream) {
    if (!session_ || destroyed_ || closed_) return;

    int32_t streamId = 0;
    std::string originStr = originOrStream;

    // If originOrStream is a numeric string, treat as stream ID
    try {
        size_t pos = 0;
        int val = std::stoi(originOrStream, &pos);
        if (pos == originOrStream.size()) {
            // Entire string was a number — treat as stream ID
            streamId = val;
            originStr.clear();
        }
    } catch (...) {
        // Not a number — treat as origin string
    }

    // When stream_id != 0, origin must be NULL and origin_len 0 per nghttp2 API.
    // When stream_id == 0, origin must be non-empty.
    const uint8_t* originPtr = originStr.empty() ? nullptr
        : reinterpret_cast<const uint8_t*>(originStr.data());
    size_t originLen = originStr.size();

    int rv = nghttp2_submit_altsvc(
        session_, NGHTTP2_FLAG_NONE, streamId,
        originPtr, originLen,
        reinterpret_cast<const uint8_t*>(alt.data()), alt.size());

    if (rv != 0) {
        emitter_.emit(event::Error_,
            Error("Failed to submit ALTSVC frame: " +
                  std::string(nghttp2_strerror(rv))));
    }
    flushOutput();
}

inline void Http2SessionImpl::submitOrigin(const std::vector<std::string>& origins) {
    if (!session_ || destroyed_ || closed_) return;

    // Build nghttp2_origin_entry array. The entries reference the
    // strings in `origins`, which remain valid for the duration of this call.
    // nghttp2_submit_origin() copies the data internally.
    std::vector<nghttp2_origin_entry> entries;
    entries.reserve(origins.size());
    for (const auto& o : origins) {
        nghttp2_origin_entry entry;
        entry.origin = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(o.data()));
        entry.origin_len = o.size();
        entries.push_back(entry);
    }

    int rv = nghttp2_submit_origin(
        session_, NGHTTP2_FLAG_NONE,
        entries.empty() ? nullptr : entries.data(), entries.size());

    if (rv != 0) {
        emitter_.emit(event::Error_,
            Error("Failed to submit ORIGIN frame: " +
                  std::string(nghttp2_strerror(rv))));
    }
    flushOutput();
}

inline void Http2SessionImpl::submitResponseWithFD(
    int32_t streamId, const http::Headers& headers,
    int fd, const StreamFileResponseOptions& options, bool ownsFd) {
    if (!session_ || destroyed_ || closed_) return;

    // Validate response pseudo-headers
    validateResponseHeaders_(headers);

    auto stream = getStream(streamId);

    // Seek to the requested offset (default: current position / 0)
    int64_t offset = options.offset.value_or(0);
    int64_t length = options.length.value_or(-1);  // -1 = read until EOF
    if (offset > 0) {
        ::lseek(fd, static_cast<off_t>(offset), SEEK_SET);
    }

    // Store fd source on the stream impl for lazy reading in onDataSourceRead()
    stream->setFdSource(fd, offset, length, ownsFd);
    stream->setWantTrailers(options.waitForTrailers);

    // Submit response with a data provider (not endStream — data will follow)
    auto holder = headersToNv(headers);

    nghttp2_data_provider dp;
    dp.source.ptr = this;
    dp.read_callback = callbacks::onDataSourceReadCallback;

    nghttp2_submit_response(session_, streamId,
                            holder.nva.data(), holder.nva.size(), &dp);

    for (const auto& [name, value] : holder.pairs) {
        stream->addSentHeader(name, value);
    }
    stream->setHeadersSent(true);

    flushOutput();
}

inline int Http2SessionImpl::feedData(const uint8_t* data, size_t len) {
    if (!session_ || destroyed_) return -1;

    ssize_t rv = nghttp2_session_mem_recv(session_, data, len);
    if (rv < 0) return static_cast<int>(rv);

    // Send any pending frames generated by processing received data
    flushOutput();
    return 0;
}

inline std::string Http2SessionImpl::serializedOutput() {
    if (!session_) return {};

    flushOutput();
    return outputBuffer_;
}

inline std::string Http2SessionImpl::consumeOutput() noexcept {
    std::string result;
    std::swap(result, outputBuffer_);
    return result;
}

inline void Http2SessionImpl::flushOutput() {
    if (!session_) return;

    while (nghttp2_session_want_write(session_)) {
        int rv = nghttp2_session_send(session_);
        if (rv != 0) break;
    }
}

// ── nghttp2 callbacks ──────────────────────────────────────────────

inline ssize_t Http2SessionImpl::onSend(const uint8_t* data, size_t length, int /*flags*/) {
    outputBuffer_.append(reinterpret_cast<const char*>(data), length);
    return static_cast<ssize_t>(length);
}

inline int Http2SessionImpl::onFrameRecv(const nghttp2_frame* frame) {
    switch (frame->hd.type) {
        case NGHTTP2_SETTINGS:
            if (frame->hd.flags & NGHTTP2_FLAG_ACK) {
                // Our settings were acknowledged
                pendingSettingsAck_ = false;
                emitter_.emit(event::LocalSettings, localSettings_);
                if (settingsCallback_) {
                    settingsCallback_(nullptr);
                    settingsCallback_ = nullptr;
                }
            } else {
                // Remote peer sent settings
                remoteSettings_ = remoteSettings();
                emitter_.emit(event::RemoteSettings, remoteSettings_);
            }
            break;

        case NGHTTP2_PING:
            if (frame->hd.flags & NGHTTP2_FLAG_ACK) {
                // PING ACK received
                if (!pendingPings_.empty()) {
                    auto& entry = pendingPings_.front();
                    auto now = std::chrono::steady_clock::now();
                    double durationMs = std::chrono::duration<double, std::milli>(
                        now - entry.sentAt).count();
                    std::string payload(reinterpret_cast<const char*>(frame->ping.opaque_data), 8);
                    if (entry.callback) {
                        entry.callback(nullptr, durationMs, payload);
                    }
                    emitter_.emit(event::Ping, durationMs, payload);
                    pendingPings_.erase(pendingPings_.begin());
                }
            }
            break;

        case NGHTTP2_GOAWAY:
            emitter_.emit(event::Goaway,
                          static_cast<uint32_t>(frame->goaway.error_code),
                          static_cast<int32_t>(frame->goaway.last_stream_id));
            // Peer initiated graceful shutdown. For server sessions, mirror the
            // shutdown promptly so clients waiting on close() are not stalled.
            if (isServer_ && !closed_ && !destroyed_) {
                close(nullptr);
            }
            break;

        case NGHTTP2_HEADERS: {
            // nghttp2 only calls on_frame_recv for HEADERS after the entire
            // header block (including CONTINUATION frames) has been assembled.
            // No need to check NGHTTP2_FLAG_END_HEADERS here.
            auto stream = getStream(frame->hd.stream_id);
            if (frame->headers.cat == NGHTTP2_HCAT_REQUEST) {
                // Server received a new request
                if (isServer_) {
                    emitter_.emit(event::ServerStream,
                                  ServerHttp2Stream(stream),
                                  stream->receivedHeaders());
                }
            } else if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
                // Client received response headers
                if (!isServer_) {
                    stream->emitter().emit(event::Response, stream->receivedHeaders());
                }
            } else if (frame->headers.cat == NGHTTP2_HCAT_HEADERS) {
                // Trailing headers
                if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
                    stream->emitter().emit(event::Trailers, stream->receivedTrailers());
                }
            } else if (frame->headers.cat == NGHTTP2_HCAT_PUSH_RESPONSE) {
                // Push response headers
                if (!isServer_) {
                    stream->emitter().emit(event::Push, stream->receivedHeaders());
                }
            }

            if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
                stream->setEndAfterHeaders(true);
                stream->onEnd();
            }
            break;
        }

        case NGHTTP2_DATA:
            if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
                auto stream = getStream(frame->hd.stream_id);
                stream->onEnd();
            }
            break;

        case NGHTTP2_PUSH_PROMISE: {
            // Client received a push promise
            if (!isServer_) {
                auto parentStream = getStream(frame->push_promise.promised_stream_id);
                emitter_.emit(event::ClientStream,
                              ClientHttp2Stream(parentStream),
                              parentStream->receivedHeaders());
            }
            break;
        }

        default:
            break;
    }

    return 0;
}

inline int Http2SessionImpl::onFrameNotSend(const nghttp2_frame* frame, int errorCode) {
    auto stream = getStream(frame->hd.stream_id);
    stream->emitter().emit(event::FrameError, frame->hd.type, errorCode, frame->hd.stream_id);
    return 0;
}

inline int Http2SessionImpl::onStreamClose(int32_t streamId, uint32_t errorCode) {
    auto it = streams_.find(streamId);
    if (it != streams_.end()) {
        it->second->onStreamClose(errorCode);
    }
    return 0;
}

inline int Http2SessionImpl::onBeginHeaders(const nghttp2_frame* frame) {
    if (frame->hd.type == NGHTTP2_HEADERS || frame->hd.type == NGHTTP2_PUSH_PROMISE) {
        int32_t streamId = frame->hd.stream_id;
        if (frame->hd.type == NGHTTP2_PUSH_PROMISE) {
            streamId = frame->push_promise.promised_stream_id;
        }
        auto stream = getStream(streamId);
        // If this is a trailer header block, don't clear existing headers
        if (frame->headers.cat != NGHTTP2_HCAT_HEADERS) {
            stream->clearReceivedHeaders();
        }
    }
    return 0;
}

inline int Http2SessionImpl::onHeader(const nghttp2_frame* frame,
                                      const uint8_t* name, size_t namelen,
                                      const uint8_t* value, size_t valuelen,
                                      uint8_t /*flags*/) {
    int32_t streamId = frame->hd.stream_id;
    if (frame->hd.type == NGHTTP2_PUSH_PROMISE) {
        streamId = frame->push_promise.promised_stream_id;
    }

    auto stream = getStream(streamId);
    std::string nameStr(reinterpret_cast<const char*>(name), namelen);
    std::string valueStr(reinterpret_cast<const char*>(value), valuelen);

    if (frame->headers.cat == NGHTTP2_HCAT_HEADERS &&
        (frame->hd.flags & NGHTTP2_FLAG_END_STREAM)) {
        // This is a trailer — pseudo-headers are not valid in trailers
        stream->addReceivedTrailer(nameStr, valueStr);
    } else if (!nameStr.empty() && nameStr[0] == ':') {
        // Pseudo-header: must still be in the pseudo-header phase.
        // nghttp2 enforces ordering per RFC 9113 §8.3, so receivingPseudo_
        // being false here is defense-in-depth tracking only (no error thrown).
        // Pseudo-header: parse into request or response meta
        if (nameStr == ":status") {
            // Response pseudo-header
            if (!stream->responseMeta().has_value()) {
                Http2ResponseMeta meta;
                try { meta.status = std::stoi(valueStr); } catch (...) { meta.status = 0; }
                stream->setResponseMeta(meta);
            }
        } else {
            // Request pseudo-headers
            if (!stream->requestMeta().has_value()) {
                Http2RequestMeta meta;
                stream->setRequestMeta(meta);
            }
            stream->updateRequestMeta(nameStr, valueStr);
        }
        // Also store in receivedHeaders for backward compatibility
        stream->addReceivedHeader(nameStr, valueStr);
    } else {
        // First regular (non-pseudo) header: mark end of pseudo-header phase
        stream->setReceivingPseudo(false);
        stream->addReceivedHeader(nameStr, valueStr);
    }

    return 0;
}

inline int Http2SessionImpl::onDataChunkRecv(uint8_t /*flags*/, int32_t streamId,
                                             const uint8_t* data, size_t len) {
    auto stream = getStream(streamId);
    stream->onData(data, len);
    return 0;
}

inline ssize_t Http2SessionImpl::onDataSourceRead(int32_t streamId, uint8_t* buf,
                                                  size_t length, uint32_t* dataFlags) {
    // ── File-descriptor data source path ──────────────────────────────
    // If the stream has an fd source (respondWithFD/respondWithFile),
    // read directly from the fd into the output buffer. This provides
    // true lazy I/O — only the bytes nghttp2 can send are read from disk.
    auto streamIt = streams_.find(streamId);
    if (streamIt != streams_.end() && streamIt->second->hasFdSource()) {
        auto& stream = streamIt->second;
        int fd = stream->fdSource();
        int64_t remaining = stream->fdRemainingLength();

        // Cap read size by remaining length (if not reading until EOF)
        size_t toRead = length;
        if (remaining >= 0) {
            toRead = std::min(length, static_cast<size_t>(remaining));
        }

        if (toRead == 0) {
            // Reached the requested length limit — signal EOF
            *dataFlags |= NGHTTP2_DATA_FLAG_EOF;
            if (stream->wantTrailers()) {
                *dataFlags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
                stream->emitter().emit(event::WantTrailers);
            }
            stream->closeFdSource();
            return 0;
        }

        ssize_t nread = ::read(fd, buf, toRead);
        if (nread < 0) {
            // Read error — emit error and signal EOF
            emitter_.emit(event::Error_,
                Error("Failed to read from file descriptor: " +
                      std::string(std::strerror(errno))));
            *dataFlags |= NGHTTP2_DATA_FLAG_EOF;
            stream->closeFdSource();
            return 0;
        }
        if (nread == 0) {
            // EOF reached on the fd
            *dataFlags |= NGHTTP2_DATA_FLAG_EOF;
            if (stream->wantTrailers()) {
                *dataFlags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
                stream->emitter().emit(event::WantTrailers);
            }
            stream->closeFdSource();
            return 0;
        }

        stream->advanceFdSource(nread);
        return nread;
    }

    // ── pendingData_ path (standard write/end) ────────────────────────
    auto it = pendingData_.find(streamId);
    bool hasEnd = pendingDataEnd_.count(streamId) > 0 && pendingDataEnd_[streamId];

    if (it == pendingData_.end() || it->second.empty()) {
        if (hasEnd) {
            *dataFlags |= NGHTTP2_DATA_FLAG_EOF;

            // Check if stream wants trailers
            auto streamIt = streams_.find(streamId);
            if (streamIt != streams_.end() && streamIt->second->wantTrailers()) {
                *dataFlags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
                streamIt->second->emitter().emit(event::WantTrailers);
            }

            pendingDataEnd_.erase(streamId);
            return 0;
        }
        // No data available yet, defer
        return NGHTTP2_ERR_DEFERRED;
    }

    auto& queue = it->second;
    size_t bufferedBefore = pendingDataSize_(streamId);
    auto& front = queue.front();
    size_t copyLen = std::min(front.size(), length);
    std::memcpy(buf, front.data(), copyLen);

    if (copyLen < front.size()) {
        front = front.substr(copyLen);
    } else {
        queue.pop_front();
    }

    size_t bufferedAfter = pendingDataSize_(streamId);
    if (queue.empty()) {
        pendingData_.erase(streamId);
    }

    if (bufferedBefore >= kPendingDataHighWaterMark &&
        bufferedAfter < kPendingDataHighWaterMark) {
        auto streamIt = streams_.find(streamId);
        if (streamIt != streams_.end()) {
            streamIt->second->emitter().emit(event::Drain);
        }
    }

    // Check if this is the last chunk
    if (bufferedAfter == 0 && hasEnd) {
        *dataFlags |= NGHTTP2_DATA_FLAG_EOF;

        auto streamIt = streams_.find(streamId);
        if (streamIt != streams_.end() && streamIt->second->wantTrailers()) {
            *dataFlags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
            streamIt->second->emitter().emit(event::WantTrailers);
        }

        pendingDataEnd_.erase(streamId);
    }

    return static_cast<ssize_t>(copyLen);
}

inline size_t Http2SessionImpl::pendingDataSize_(int32_t streamId) const {
    auto it = pendingData_.find(streamId);
    if (it == pendingData_.end()) return 0;

    size_t total = 0;
    for (const auto& chunk : it->second) {
        total += chunk.size();
    }
    return total;
}

// ════════════════════════════════════════════════════════════════════════
// Http2StreamImpl implementation
// ════════════════════════════════════════════════════════════════════════

inline Http2StreamImpl::Http2StreamImpl(int32_t streamId,
                                        std::shared_ptr<Http2SessionImpl> session)
    noexcept : streamId_(streamId), session_(session) {}

inline void Http2StreamImpl::closeFdSource() {
    if (fdSource_ >= 0 && fdOwnsFd_) {
        ::close(fdSource_);
    }
    fdSource_ = -1;
    fdCurrentOffset_ = 0;
    fdRemainingLength_ = -1;
    fdOwnsFd_ = false;
}

inline bool Http2StreamImpl::pushAllowed() const {
    auto sess = session_.lock();
    if (!sess) return false;
    auto remote = sess->remoteSettings();
    return remote.enablePush.value_or(true);
}

inline StreamState Http2StreamImpl::state() const {
    StreamState st;
    auto sess = session_.lock();
    if (!sess) {
        st.state = Http2StreamState::Closed;
        st.localClose = true;
        st.remoteClose = true;
        return st;
    }

    // Track local/remote close independently for half-closed states
    st.localClose = localClose_;
    st.remoteClose = remoteClose_;

    if (closed_) {
        st.state = Http2StreamState::Closed;
    } else if (localClose_ && remoteClose_) {
        st.state = Http2StreamState::Closed;
    } else if (localClose_) {
        st.state = Http2StreamState::HalfClosedLocal;
    } else if (remoteClose_) {
        st.state = Http2StreamState::HalfClosedRemote;
    } else if (streamId_ == 0) {
        st.state = Http2StreamState::Idle;
    } else {
        st.state = Http2StreamState::Open;
    }

    return st;
}

inline void Http2StreamImpl::close(uint32_t code, std::function<void()> callback) {
    if (closed_) {
        if (callback) callback();
        return;
    }

    auto sess = session_.lock();
    if (sess && streamId_ > 0) {
        sess->submitRstStream(streamId_, code);
    }

    rstCode_ = code;
    closed_ = true;
    // RST_STREAM closes both directions immediately
    localClose_ = true;
    remoteClose_ = true;
    emitter_.emit(event::Close);

    if (callback) callback();
}

inline void Http2StreamImpl::respond(const http::Headers& headers,
                                     const StreamResponseOptions& options) {
    if (headersSent_) return;

    auto sess = session_.lock();
    if (!sess) return;

    wantTrailers_ = options.waitForTrailers;
    sess->submitResponse(streamId_, headers, options.endStream);
    // If endStream is set, local side is closed with the response headers
    if (options.endStream) {
        localClose_ = true;
    }
}

inline void Http2StreamImpl::respondWithFD(int fd, const http::Headers& headers,
                                           const StreamFileResponseOptions& options) {
    if (headersSent_) return;

    auto sess = session_.lock();
    if (!sess) return;

    // Caller owns the fd — ownsFd = false
    sess->submitResponseWithFD(streamId_, headers, fd, options, /*ownsFd=*/false);
}

inline void Http2StreamImpl::respondWithFile(const std::string& path,
                                             const http::Headers& headers,
                                             const StreamFileResponseOptions& options) {
    if (headersSent_) return;

    auto sess = session_.lock();
    if (!sess) return;

    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        sess->emitter().emit(event::Error_,
            Error("Failed to open file for respondWithFile: " + path +
                  " (" + std::string(std::strerror(errno)) + ")"));
        return;
    }

    // We opened the fd, so we own it — ownsFd = true
    sess->submitResponseWithFD(streamId_, headers, fd, options, /*ownsFd=*/true);
}

inline void Http2StreamImpl::additionalHeaders(const http::Headers& headers) {
    auto sess = session_.lock();
    if (!sess || headersSent_) return;
    sess->submitInfo(streamId_, headers);
}

inline void Http2StreamImpl::pushStream(
    const http::Headers& headers,
    std::function<void(const Error*, ServerHttp2Stream, const http::Headers&)> callback) {
    auto sess = session_.lock();
    if (!sess || !sess->isServer()) {
        if (callback) {
            Error err("Push streams are only supported on server sessions");
            callback(&err, ServerHttp2Stream(), headers);
        }
        return;
    }

    int32_t promisedId = sess->submitPushPromise(streamId_, headers);
    if (promisedId < 0) {
        if (callback) {
            Error err("Failed to create push promise");
            callback(&err, ServerHttp2Stream(), headers);
        }
        return;
    }

    auto pushStream = sess->getStream(promisedId);
    if (callback) {
        callback(nullptr, ServerHttp2Stream(pushStream), headers);
    }
}

inline void Http2StreamImpl::write(const std::string& data, std::function<void()> callback) {
    auto sess = session_.lock();
    if (!sess) return;
    sess->submitData(streamId_, data, false);
    if (callback) callback();
}

inline void Http2StreamImpl::end(const std::string& data, std::function<void()> callback) {
    auto sess = session_.lock();
    if (!sess) return;
    sess->submitData(streamId_, data, true);
    // Mark local side as closed (sent END_STREAM)
    localClose_ = true;
    if (callback) callback();
}

inline void Http2StreamImpl::sendTrailers(const http::Headers& headers) {
    auto sess = session_.lock();
    if (!sess) return;
    sess->submitTrailers(streamId_, headers);
}

inline void Http2StreamImpl::setTimeout(uint32_t msecs, std::function<void()> callback) {
    if (callback && msecs > 0) {
        emitter_.once(event::Timeout, [cb = std::move(callback)]() {
            cb();
        });
    }
}

inline void Http2StreamImpl::addReceivedHeader(const std::string& name, const std::string& value) {
    // Use append() to preserve multi-value headers (e.g., Set-Cookie)
    receivedHeaders_.append(name, value);
}

inline void Http2StreamImpl::addReceivedTrailer(const std::string& name, const std::string& value) {
    // Use append() to preserve multi-value trailers
    receivedTrailers_.append(name, value);
}

inline void Http2StreamImpl::onData(const uint8_t* data, size_t len) {
    std::string chunk(reinterpret_cast<const char*>(data), len);
    dataBuffer_ += chunk;
    emitter_.emit(event::Data, chunk);
}

inline void Http2StreamImpl::onEnd() {
    if (dataEnded_) return;
    dataEnded_ = true;
    // Mark remote side as closed (received END_STREAM)
    remoteClose_ = true;
    emitter_.emit(event::End);
}

inline void Http2StreamImpl::onStreamClose(uint32_t errorCode) {
    rstCode_ = errorCode;
    closed_ = true;
    // Stream close means both directions are done
    localClose_ = true;
    remoteClose_ = true;

    // Clean up fd source if the stream was serving a file
    closeFdSource();

    if (errorCode != 0 && errorCode != constants::NGHTTP2_NO_ERROR) {
        aborted_ = true;
        emitter_.emit(event::Aborted);
    }

    emitter_.emit(event::Close);

    // Remove from session
    auto sess = session_.lock();
    if (sess) {
        sess->removeStream(streamId_);
    }
}

} // namespace impl
} // namespace http2
} // namespace polycpp
