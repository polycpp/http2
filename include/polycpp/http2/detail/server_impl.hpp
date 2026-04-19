#pragma once
#include <polycpp/config.hpp>

/**
 * @file server_impl.hpp
 * @brief HTTP/2 server implementation method bodies.
 *
 * Contains POLYCPP_IMPL method bodies for Http2ServerImpl.
 * The class definition is in server_impl_decl.hpp.
 *
 * @note Do not include directly. Included via <polycpp/http2/detail/aggregator.hpp>.
 */

#include <polycpp/http2/detail/server_impl_decl.hpp>
#include <polycpp/http2/events.hpp>
#include <polycpp/io/address.hpp>

namespace polycpp {
namespace http2 {
namespace impl {

// ════════════════════════════════════════════════════════════════════════
// Http2ServerImpl implementation
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL Http2ServerImpl::Http2ServerImpl(EventContext& ctx,
                                        const ServerOptions& options,
                                        bool secure)
    : ctx_(ctx), options_(options), secure_(secure) {}

POLYCPP_IMPL void Http2ServerImpl::configureTls(const SecureServerOptions& opts) {
    tlsContext_ = std::make_unique<io::TlsContext>(io::TlsContext::Method::kTLSServer);
    tlsContext_->setDefaultOptions();

    if (opts.cert.has_value()) {
        tlsContext_->useCertificateChainPem(opts.cert.value());
    }
    if (opts.key.has_value()) {
        tlsContext_->usePrivateKeyPem(opts.key.value());
    }
    if (opts.ca.has_value()) {
        tlsContext_->addCertificateAuthorityPem(opts.ca.value());
    }

    // Set ALPN to h2 in wire format: length-prefixed protocol names
    // "h2" = 0x02, 'h', '2'
    auto& sslCtx = tlsContext_->sslContext();
    std::vector<uint8_t> alpnWire = {0x02, 'h', '2'};
    sslCtx.setAlpnProtocols(alpnWire);
}

POLYCPP_IMPL void Http2ServerImpl::listen(uint16_t port, const std::string& host,
                                    std::function<void()> callback) {
    if (listening_) return;

    host_ = io::resolveListenHost(host);
    port_ = port;

    acceptor_ = std::make_unique<io::TcpAcceptor>(ctx_);
    auto addrFamily = io::parseAddressFamily(host_);
    if (addrFamily == io::AddressFamily::kInvalid) {
        emitter_.emit(event::Error_, Error("Failed to bind: invalid address"));
        return;
    }
    acceptor_->open(addrFamily == io::AddressFamily::kIPv6 ? AF_INET6 : AF_INET);
    acceptor_->setReuseAddress(true);

    std::error_code ec;
    acceptor_->bind(host_, port, ec);
    if (ec) {
        emitter_.emit(event::Error_, Error("Failed to bind: " + ec.message()));
        return;
    }

    acceptor_->listen(511, ec);
    if (ec) {
        emitter_.emit(event::Error_, Error("Failed to listen: " + ec.message()));
        return;
    }

    listening_ = true;
    port_ = acceptor_->localPort();  // Get the actual port (useful for port 0)

    emitter_.emit(event::Listening);
    if (callback) callback();

    startAccepting();
}

POLYCPP_IMPL void Http2ServerImpl::close(std::function<void()> callback) {
    if (!listening_) {
        if (callback) callback();
        return;
    }

    listening_ = false;
    if (acceptor_) {
        acceptor_->close();
    }

    // Close all sessions
    for (auto& session : sessions_) {
        session->close(nullptr);
    }
    sessions_.clear();

    emitter_.emit(event::Close);
    if (callback) callback();
}

POLYCPP_IMPL void Http2ServerImpl::setTimeout(uint32_t msecs, std::function<void()> callback) {
    timeoutMs_ = msecs;
    if (callback && msecs > 0) {
        emitter_.once(event::Timeout, [cb = std::move(callback)]() {
            cb();
        });
    }
}

POLYCPP_IMPL void Http2ServerImpl::startAccepting() {
    if (!listening_ || !acceptor_) return;

    auto self = shared_from_this();
    acceptor_->asyncAccept([self](std::error_code ec, io::TcpSocket socket) {
        if (ec) {
            if (self->listening_) {
                // Transient error, keep accepting
                self->startAccepting();
            }
            return;
        }

        if (self->secure_) {
            self->handleTlsConnection(std::move(socket));
        } else {
            self->handleConnection(std::move(socket));
        }

        // Accept next connection
        self->startAccepting();
    });
}

POLYCPP_IMPL void Http2ServerImpl::handleConnection(io::TcpSocket socket) {
    auto session = std::make_shared<Http2SessionImpl>(true, options_);
    session->initialize();

    auto socketPtr = std::make_shared<io::TcpSocket>(std::move(socket));
    sessions_.push_back(session);

    // Set up the stream handler on the session
    if (streamHandler_) {
        auto handler = streamHandler_;
        ServerHttp2Session sessionHandle(session);
        sessionHandle.on(event::ServerStream, [handler](ServerHttp2Stream stream, const http::Headers& headers) {
            handler(stream, headers);
        });
    }

    // Set up compat request handler
    if (requestHandler_) {
        auto reqHandler = requestHandler_;
        ServerHttp2Session sessionHandle(session);
        sessionHandle.on(event::ServerStream, [reqHandler](ServerHttp2Stream stream, const http::Headers& headers) {
            Http2ServerRequest req(stream, headers);
            Http2ServerResponse res(stream);
            reqHandler(req, res);
        });
    }

    emitter_.emit(event::Session, ServerHttp2Session(session));

    setupSession(session, socketPtr);
}

POLYCPP_IMPL void Http2ServerImpl::handleTlsConnection(io::TcpSocket socket) {
    if (!tlsContext_) return;

    auto socketPtr = std::make_shared<io::TcpSocket>(std::move(socket));
    auto tlsStreamPtr = std::make_shared<io::TlsStream>(std::move(*socketPtr), *tlsContext_);

    auto self = shared_from_this();
    tlsStreamPtr->asyncHandshake(true, [self, tlsStreamPtr](std::error_code ec) {
        if (ec) {
            self->emitter_.emit(event::TlsClientError, Error("TLS handshake failed: " + ec.message()));
            return;
        }

        auto session = std::make_shared<Http2SessionImpl>(true, self->options_);
        session->setEncrypted(true);
        session->setAlpnProtocol("h2");
        session->initialize();

        self->sessions_.push_back(session);

        // Set up stream handler
        if (self->streamHandler_) {
            auto handler = self->streamHandler_;
            ServerHttp2Session sessionHandle(session);
            sessionHandle.on(event::ServerStream, [handler](ServerHttp2Stream stream, const http::Headers& headers) {
                handler(stream, headers);
            });
        }

        self->emitter_.emit(event::Session, ServerHttp2Session(session));

        // Read loop using TLS stream
        // (Simplified — in production we'd integrate with the TLS stream properly)
    });
}

POLYCPP_IMPL void Http2ServerImpl::setupSession(std::shared_ptr<Http2SessionImpl> session,
                                          std::shared_ptr<io::TcpSocket> socket) {
    // Send any initial output (settings frame, etc.)
    writeOutput(session, socket);

    // Start reading
    readLoop(session, socket);
}

POLYCPP_IMPL void Http2ServerImpl::readLoop(std::shared_ptr<Http2SessionImpl> session,
                                      std::shared_ptr<io::TcpSocket> socket) {
    auto buf = std::make_shared<std::vector<uint8_t>>(16384);
    auto self = shared_from_this();

    socket->asyncReadSome(buf->data(), buf->size(),
        [self, session, socket, buf](std::error_code ec, size_t bytesRead) {
            if (ec || bytesRead == 0) {
                session->close(nullptr);
                std::error_code ignoreEc;
                socket->close(ignoreEc);
                return;
            }

            // Feed data into nghttp2
            session->feedData(buf->data(), bytesRead);

            // Send any response frames
            self->writeOutput(session, socket);

            // Continue reading
            if (!session->closed() && !session->destroyed()) {
                self->readLoop(session, socket);
            } else {
                std::error_code ignoreEc;
                socket->close(ignoreEc);
            }
        });
}

POLYCPP_IMPL void Http2ServerImpl::writeOutput(std::shared_ptr<Http2SessionImpl> session,
                                         std::shared_ptr<io::TcpSocket> socket) {
    if (!session->hasPendingOutput()) return;

    auto output = std::make_shared<std::string>(session->consumeOutput());
    if (output->empty()) return;

    auto self = shared_from_this();
    socket->asyncWrite(output->data(), output->size(),
        [self, session, socket, output](std::error_code ec, size_t /*bytesWritten*/) {
            if (ec) {
                session->destroy(nullptr, constants::NGHTTP2_INTERNAL_ERROR);
                std::error_code ignoreEc;
                socket->close(ignoreEc);
                return;
            }

            // Check if there's more output to send (from callbacks)
            if (session->hasPendingOutput()) {
                self->writeOutput(session, socket);
            } else if (session->closed() || session->destroyed()) {
                std::error_code ignoreEc;
                socket->close(ignoreEc);
            }
        });
}

} // namespace impl
} // namespace http2
} // namespace polycpp
