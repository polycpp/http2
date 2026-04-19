#pragma once

/**
 * @file server_impl_decl.hpp
 * @brief Http2ServerImpl class definition (declarations only).
 *
 * Http2ServerImpl manages the TCP/TLS acceptor and creates sessions
 * for each incoming connection. Bridges between the transport layer
 * and the Http2SessionImpl.
 *
 * @note Do not include directly. Included via <polycpp/http2/detail/aggregator.hpp>.
 */

#include <polycpp/http2/http2.hpp>
#include <polycpp/http2/detail/session_impl.hpp>
#include <polycpp/io/event_context.hpp>
#include <polycpp/io/address.hpp>
#include <polycpp/io/tcp_acceptor.hpp>
#include <polycpp/io/tcp_socket.hpp>
#include <polycpp/io/tls_context.hpp>
#include <polycpp/io/tls_stream.hpp>
#include <polycpp/events.hpp>
#include <polycpp/core/error.hpp>

#include <memory>
#include <string>
#include <vector>

namespace polycpp {
namespace http2 {
namespace impl {

/**
 * @brief Internal implementation for HTTP/2 servers.
 *
 * Manages TCP/TLS listening, connection acceptance, and session creation.
 *
 */
class Http2ServerImpl : public std::enable_shared_from_this<Http2ServerImpl> {
public:
    /**
     * @brief Constructs a server implementation.
     * @param ctx The event context.
     * @param options Server options.
     * @param secure Whether to use TLS.
     */
    inline Http2ServerImpl(EventContext& ctx, const ServerOptions& options, bool secure);

    inline ~Http2ServerImpl() = default;

    // Non-copyable
    Http2ServerImpl(const Http2ServerImpl&) = delete;
    Http2ServerImpl& operator=(const Http2ServerImpl&) = delete;

    // ── Properties ─────────────────────────────────────────────────

    inline bool listening() const noexcept { return listening_; }
    inline bool secure() const noexcept { return secure_; }

    inline std::unordered_map<std::string, std::string> address() const {
        std::unordered_map<std::string, std::string> addr;
        if (listening_) {
            addr["address"] = host_;
            addr["family"] = io::parseAddressFamily(host_) == io::AddressFamily::kIPv6
                ? "IPv6" : "IPv4";
            addr["port"] = std::to_string(port_);
        }
        return addr;
    }

    // ── Event Emitter ──────────────────────────────────────────────

    inline events::EventEmitter& emitter() noexcept { return emitter_; }
    inline const events::EventEmitter& emitter() const noexcept { return emitter_; }

    // ── Methods ────────────────────────────────────────────────────

    inline void listen(uint16_t port, const std::string& host,
                       std::function<void()> callback);
    inline void close(std::function<void()> callback);
    inline void setTimeout(uint32_t msecs, std::function<void()> callback);

    // ── TLS configuration ──────────────────────────────────────────

    inline void configureTls(const SecureServerOptions& opts);

    // ── Stream event handler ───────────────────────────────────────

    inline void setStreamHandler(std::function<void(ServerHttp2Stream, http::Headers)> handler) {
        streamHandler_ = std::move(handler);
    }

    // ── Request event handler (compat layer) ───────────────────────

    inline void setRequestHandler(std::function<void(Http2ServerRequest&, Http2ServerResponse&)> handler) {
        requestHandler_ = std::move(handler);
    }

private:
    EventContext& ctx_;
    ServerOptions options_;
    bool secure_;
    bool listening_ = false;
    std::string host_;
    uint16_t port_ = 0;
    uint32_t timeoutMs_ = 0;

    events::EventEmitter emitter_;
    std::unique_ptr<io::TcpAcceptor> acceptor_;
    std::unique_ptr<io::TlsContext> tlsContext_;

    // Active sessions
    std::vector<std::shared_ptr<Http2SessionImpl>> sessions_;

    // User-provided handlers
    std::function<void(ServerHttp2Stream, http::Headers)> streamHandler_;
    std::function<void(Http2ServerRequest&, Http2ServerResponse&)> requestHandler_;

    // ── Internal helpers ───────────────────────────────────────────

    inline void startAccepting();
    inline void handleConnection(io::TcpSocket socket);
    inline void handleTlsConnection(io::TcpSocket socket);
    inline void setupSession(std::shared_ptr<Http2SessionImpl> session,
                             std::shared_ptr<io::TcpSocket> socket);
    inline void readLoop(std::shared_ptr<Http2SessionImpl> session,
                         std::shared_ptr<io::TcpSocket> socket);
    inline void writeOutput(std::shared_ptr<Http2SessionImpl> session,
                            std::shared_ptr<io::TcpSocket> socket);
};

} // namespace impl
} // namespace http2
} // namespace polycpp
