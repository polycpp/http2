#pragma once
#include <polycpp/config.hpp>

/**
 * @file handle_bodies.hpp
 * @brief Inline method bodies for HTTP/2 handle classes.
 *
 * Implements all non-template methods of Http2Stream, ServerHttp2Stream,
 * ClientHttp2Stream, Http2Session, ServerHttp2Session, ClientHttp2Session,
 * Http2Server, Http2SecureServer, Http2ServerRequest, Http2ServerResponse,
 * and the module-level factory functions.
 *
 */

#include <polycpp/http2/http2.hpp>
#include <polycpp/http2/detail/session_impl.hpp>
#include <polycpp/http2/detail/server_impl.hpp>
#include <polycpp/events.hpp>
#include <polycpp/core/error.hpp>
#include <polycpp/io/event_context.hpp>

#include <any>
#include <memory>
#include <string>
#include <vector>

namespace polycpp {
namespace http2 {

// ════════════════════════════════════════════════════════════════════════
// Http2Stream
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL Http2Stream::Http2Stream() noexcept = default;

POLYCPP_IMPL Http2Stream::Http2Stream(std::shared_ptr<impl::Http2StreamImpl> impl)
    noexcept : impl_(std::move(impl)) {
    if (impl_) setEmitter_(impl_->emitter());
}

POLYCPP_IMPL Http2Stream::~Http2Stream() = default;

POLYCPP_IMPL bool Http2Stream::aborted() const noexcept {
    return impl_ ? impl_->aborted() : false;
}

POLYCPP_IMPL int32_t Http2Stream::id() const noexcept {
    return impl_ ? impl_->id() : 0;
}

POLYCPP_IMPL bool Http2Stream::closed() const noexcept {
    return impl_ ? impl_->closed() : true;
}

POLYCPP_IMPL bool Http2Stream::destroyed() const noexcept {
    return impl_ ? impl_->destroyed() : true;
}

POLYCPP_IMPL bool Http2Stream::endAfterHeaders() const noexcept {
    return impl_ ? impl_->endAfterHeaders() : false;
}

POLYCPP_IMPL bool Http2Stream::pending() const noexcept {
    return impl_ ? impl_->pending() : false;
}

POLYCPP_IMPL uint32_t Http2Stream::rstCode() const noexcept {
    return impl_ ? impl_->rstCode() : 0;
}

POLYCPP_IMPL const http::Headers& Http2Stream::sentHeaders() const {
    static const http::Headers empty;
    return impl_ ? impl_->sentHeaders() : empty;
}

POLYCPP_IMPL const std::vector<http::Headers>& Http2Stream::sentInfoHeaders() const noexcept {
    static const std::vector<http::Headers> empty;
    return impl_ ? impl_->sentInfoHeaders() : empty;
}

POLYCPP_IMPL const http::Headers& Http2Stream::sentTrailers() const noexcept {
    static const http::Headers empty;
    return impl_ ? impl_->sentTrailers() : empty;
}

POLYCPP_IMPL std::shared_ptr<impl::Http2SessionImpl> Http2Stream::session() const noexcept {
    return impl_ ? impl_->session() : nullptr;
}

POLYCPP_IMPL const std::optional<Http2RequestMeta>& Http2Stream::requestMeta() const noexcept {
    static const std::optional<Http2RequestMeta> empty;
    return impl_ ? impl_->requestMeta() : empty;
}

POLYCPP_IMPL const std::optional<Http2ResponseMeta>& Http2Stream::responseMeta() const noexcept {
    static const std::optional<Http2ResponseMeta> empty;
    return impl_ ? impl_->responseMeta() : empty;
}

POLYCPP_IMPL StreamState Http2Stream::state() const {
    return impl_ ? impl_->state() : StreamState{};
}

POLYCPP_IMPL void Http2Stream::close(uint32_t code, std::function<void()> callback) {
    if (impl_) impl_->close(code, std::move(callback));
}

POLYCPP_IMPL void Http2Stream::close(Http2ErrorCode code, std::function<void()> callback) {
    close(static_cast<uint32_t>(code), std::move(callback));
}

POLYCPP_IMPL void Http2Stream::priority(const ClientRequestOptions& options) noexcept {
    // Priority is deprecated in HTTP/2 but we accept the call
    (void)options;
}

POLYCPP_IMPL void Http2Stream::setTimeout(uint32_t msecs, std::function<void()> callback) {
    if (impl_) impl_->setTimeout(msecs, std::move(callback));
}

POLYCPP_IMPL void Http2Stream::sendTrailers(const http::Headers& headers) {
    if (impl_) impl_->sendTrailers(headers);
}

POLYCPP_IMPL bool Http2Stream::valid() const noexcept {
    return impl_ != nullptr;
}

POLYCPP_IMPL std::shared_ptr<impl::Http2StreamImpl> Http2Stream::impl() const noexcept {
    return impl_;
}

// ════════════════════════════════════════════════════════════════════════
// ServerHttp2Stream
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL ServerHttp2Stream::ServerHttp2Stream() noexcept = default;

POLYCPP_IMPL ServerHttp2Stream::ServerHttp2Stream(std::shared_ptr<impl::Http2StreamImpl> impl)
    noexcept : Http2Stream(std::move(impl)) {}

POLYCPP_IMPL bool ServerHttp2Stream::headersSent() const noexcept {
    return impl() ? impl()->headersSent() : false;
}

POLYCPP_IMPL bool ServerHttp2Stream::pushAllowed() const {
    return impl() ? impl()->pushAllowed() : false;
}

POLYCPP_IMPL void ServerHttp2Stream::respond(const http::Headers& headers,
                                       const StreamResponseOptions& options) {
    if (impl()) impl()->respond(headers, options);
}

POLYCPP_IMPL void ServerHttp2Stream::respond(int statusCode,
                                       const http::Headers& headers,
                                       const StreamResponseOptions& options){
    // Build combined headers with :status pseudo-header first (RFC 9113 ordering)
    http::Headers combined;
    combined.set(":status", std::to_string(statusCode));
    for (const auto& [name, value] : headers.toHttp2Pairs()) {
        combined.append(name, value);
    }
    respond(combined, options);
}

POLYCPP_IMPL void ServerHttp2Stream::respondWithFD(int fd, const http::Headers& headers,
                                                     const StreamFileResponseOptions& options) {
    if (impl()) impl()->respondWithFD(fd, headers, options);
}

POLYCPP_IMPL void ServerHttp2Stream::respondWithFile(const std::string& path,
                                                       const http::Headers& headers,
                                                       const StreamFileResponseOptions& options) {
    if (impl()) impl()->respondWithFile(path, headers, options);
}

POLYCPP_IMPL void ServerHttp2Stream::additionalHeaders(const http::Headers& headers) {
    if (impl()) impl()->additionalHeaders(headers);
}

POLYCPP_IMPL void ServerHttp2Stream::pushStream(
    const http::Headers& headers,
    std::function<void(const Error*, ServerHttp2Stream, const http::Headers&)> callback) {
    if (impl()) impl()->pushStream(headers, std::move(callback));
}

POLYCPP_IMPL void ServerHttp2Stream::write(const std::string& data, std::function<void()> callback) {
    if (impl()) impl()->write(data, std::move(callback));
}

POLYCPP_IMPL void ServerHttp2Stream::end(const std::string& data, std::function<void()> callback) {
    if (impl()) impl()->end(data, std::move(callback));
}

// ════════════════════════════════════════════════════════════════════════
// ClientHttp2Stream
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL ClientHttp2Stream::ClientHttp2Stream() noexcept = default;

POLYCPP_IMPL ClientHttp2Stream::ClientHttp2Stream(std::shared_ptr<impl::Http2StreamImpl> impl)
    noexcept : Http2Stream(std::move(impl)) {}

POLYCPP_IMPL void ClientHttp2Stream::write(const std::string& data, std::function<void()> callback) {
    if (impl()) impl()->write(data, std::move(callback));
}

POLYCPP_IMPL void ClientHttp2Stream::end(const std::string& data, std::function<void()> callback) {
    if (impl()) impl()->end(data, std::move(callback));
}

// ════════════════════════════════════════════════════════════════════════
// Http2Session
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL Http2Session::Http2Session() = default;

POLYCPP_IMPL Http2Session::Http2Session(std::shared_ptr<impl::Http2SessionImpl> impl)
    noexcept : impl_(std::move(impl)) {
    if (impl_) setEmitter_(impl_->emitter());
}

POLYCPP_IMPL Http2Session::~Http2Session() = default;

POLYCPP_IMPL bool Http2Session::closed() const {
    return impl_ ? impl_->closed() : true;
}

POLYCPP_IMPL bool Http2Session::destroyed() const {
    return impl_ ? impl_->destroyed() : true;
}

POLYCPP_IMPL bool Http2Session::isServer() const {
    return impl_ ? impl_->isServer() : false;
}

POLYCPP_IMPL Settings Http2Session::localSettings() const {
    return impl_ ? impl_->localSettings() : Settings{};
}

POLYCPP_IMPL Settings Http2Session::remoteSettings() const {
    return impl_ ? impl_->remoteSettings() : Settings{};
}

POLYCPP_IMPL bool Http2Session::pendingSettingsAck() const {
    return impl_ ? impl_->pendingSettingsAck() : false;
}

POLYCPP_IMPL bool Http2Session::encrypted() const {
    return impl_ ? impl_->encrypted() : false;
}

POLYCPP_IMPL std::string Http2Session::alpnProtocol() const {
    return impl_ ? impl_->alpnProtocol() : "";
}

POLYCPP_IMPL const std::string& Http2Session::originSet() const noexcept {
    static const std::string empty;
    return impl_ ? impl_->originSet() : empty;
}

POLYCPP_IMPL SessionState Http2Session::state() const {
    return impl_ ? impl_->state() : SessionState{};
}

POLYCPP_IMPL int Http2Session::type() const noexcept {
    return impl_ ? impl_->type() : -1;
}

POLYCPP_IMPL void Http2Session::close(std::function<void()> callback) {
    if (impl_) impl_->close(std::move(callback));
}

POLYCPP_IMPL void Http2Session::destroy(const Error* error, uint32_t code) {
    if (impl_) impl_->destroy(error, code);
}

POLYCPP_IMPL void Http2Session::destroy(const Error* error, Http2ErrorCode code) {
    destroy(error, static_cast<uint32_t>(code));
}

POLYCPP_IMPL void Http2Session::goaway(uint32_t code, int32_t lastStreamID,
                                 const std::string& opaqueData) {
    if (impl_) impl_->goaway(code, lastStreamID, opaqueData);
}

POLYCPP_IMPL void Http2Session::goaway(Http2ErrorCode code, int32_t lastStreamID,
                                 const std::string& opaqueData) {
    goaway(static_cast<uint32_t>(code), lastStreamID, opaqueData);
}

POLYCPP_IMPL bool Http2Session::ping(const std::string& payload,
                               std::function<void(const Error*, double, const std::string&)> callback) {
    return impl_ ? impl_->ping(payload, std::move(callback)) : false;
}

POLYCPP_IMPL void Http2Session::settings(const Settings& settings,
                                   std::function<void(const Error*)> callback) {
    if (impl_) impl_->sendSettings(settings, std::move(callback));
}

POLYCPP_IMPL void Http2Session::setTimeout(uint32_t msecs, std::function<void()> callback) {
    if (impl_) impl_->setTimeout(msecs, std::move(callback));
}

POLYCPP_IMPL void Http2Session::setLocalWindowSize(int32_t windowSize) {
    if (impl_) impl_->setLocalWindowSize(windowSize);
}

POLYCPP_IMPL bool Http2Session::valid() const noexcept {
    return impl_ != nullptr;
}

POLYCPP_IMPL std::shared_ptr<impl::Http2SessionImpl> Http2Session::impl() const noexcept {
    return impl_;
}

// ════════════════════════════════════════════════════════════════════════
// ServerHttp2Session
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL ServerHttp2Session::ServerHttp2Session() noexcept = default;

POLYCPP_IMPL ServerHttp2Session::ServerHttp2Session(std::shared_ptr<impl::Http2SessionImpl> impl)
    : Http2Session(std::move(impl)) {}

POLYCPP_IMPL void ServerHttp2Session::altsvc(const std::string& alt,
                                       const std::string& originOrStream) {
    auto session = impl();
    if (!session) return;
    session->submitAltsvc(alt, originOrStream);
}

POLYCPP_IMPL void ServerHttp2Session::origin(const std::vector<std::string>& origins) {
    auto session = impl();
    if (!session) return;
    session->submitOrigin(origins);
}

// ════════════════════════════════════════════════════════════════════════
// ClientHttp2Session
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL ClientHttp2Session::ClientHttp2Session() noexcept = default;

POLYCPP_IMPL ClientHttp2Session::ClientHttp2Session(std::shared_ptr<impl::Http2SessionImpl> impl)
    noexcept : Http2Session(std::move(impl)) {}

POLYCPP_IMPL ClientHttp2Stream ClientHttp2Session::request(const http::Headers& headers,
                                                     const ClientRequestOptions& options) {
    if (!impl_) return ClientHttp2Stream();

    // Set wantTrailers on the stream that will be created
    int32_t streamId = impl_->submitRequest(headers, options.endStream);
    if (streamId < 0) return ClientHttp2Stream();

    auto stream = impl_->getStream(streamId);
    stream->setWantTrailers(options.waitForTrailers);
    return ClientHttp2Stream(stream);
}

POLYCPP_IMPL ClientHttp2Stream ClientHttp2Session::request(
    const Http2RequestMeta& meta,
    const http::Headers& headers,
    const ClientRequestOptions& options) {
    // Build combined headers with pseudo-headers first (RFC 9113 ordering)
    http::Headers combined;
    combined.set(":method", meta.method);
    if (!meta.path.empty()) combined.set(":path", meta.path);
    if (!meta.scheme.empty()) combined.set(":scheme", meta.scheme);
    if (!meta.authority.empty()) combined.set(":authority", meta.authority);
    if (meta.protocol.has_value()) combined.set(":protocol", meta.protocol.value());
    for (const auto& [name, value] : headers.toHttp2Pairs()) {
        combined.append(name, value);
    }
    return request(combined, options);
}


// ════════════════════════════════════════════════════════════════════════
// Http2Server
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL Http2Server::Http2Server() noexcept = default;

POLYCPP_IMPL Http2Server::Http2Server(std::shared_ptr<impl::Http2ServerImpl> impl)
    noexcept : impl_(std::move(impl)) {
    if (impl_) setEmitter_(impl_->emitter());
}

POLYCPP_IMPL Http2Server::~Http2Server() = default;

POLYCPP_IMPL void Http2Server::listen(uint16_t port, const std::string& host,
                                std::function<void()> callback) {
    if (impl_) impl_->listen(port, host, std::move(callback));
}

POLYCPP_IMPL void Http2Server::close(std::function<void()> callback){
    if (impl_) impl_->close(std::move(callback));
}

POLYCPP_IMPL void Http2Server::setTimeout(uint32_t msecs, std::function<void()> callback){
    if (impl_) impl_->setTimeout(msecs, std::move(callback));
}

POLYCPP_IMPL bool Http2Server::listening() const noexcept {
    return impl_ ? impl_->listening() : false;
}

POLYCPP_IMPL std::unordered_map<std::string, std::string> Http2Server::address() const{
    return impl_ ? impl_->address() : std::unordered_map<std::string, std::string>{};
}

POLYCPP_IMPL bool Http2Server::valid() const noexcept {
    return impl_ != nullptr;
}

POLYCPP_IMPL std::shared_ptr<impl::Http2ServerImpl> Http2Server::impl() const {
    return impl_;
}

// ════════════════════════════════════════════════════════════════════════
// Http2SecureServer
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL Http2SecureServer::Http2SecureServer() noexcept = default;

POLYCPP_IMPL Http2SecureServer::Http2SecureServer(std::shared_ptr<impl::Http2ServerImpl> impl)
    noexcept : impl_(std::move(impl)) {
    if (impl_) setEmitter_(impl_->emitter());
}

POLYCPP_IMPL Http2SecureServer::~Http2SecureServer() = default;

POLYCPP_IMPL void Http2SecureServer::listen(uint16_t port, const std::string& host,
                                      std::function<void()> callback) {
    if (impl_) impl_->listen(port, host, std::move(callback));
}

POLYCPP_IMPL void Http2SecureServer::close(std::function<void()> callback) {
    if (impl_) impl_->close(std::move(callback));
}

POLYCPP_IMPL void Http2SecureServer::setTimeout(uint32_t msecs, std::function<void()> callback) {
    if (impl_) impl_->setTimeout(msecs, std::move(callback));
}

POLYCPP_IMPL bool Http2SecureServer::listening() const noexcept {
    return impl_ ? impl_->listening() : false;
}

POLYCPP_IMPL std::unordered_map<std::string, std::string> Http2SecureServer::address() const {
    return impl_ ? impl_->address() : std::unordered_map<std::string, std::string>{};
}

POLYCPP_IMPL bool Http2SecureServer::valid() const {
    return impl_ != nullptr;
}

POLYCPP_IMPL std::shared_ptr<impl::Http2ServerImpl> Http2SecureServer::impl() const noexcept {
    return impl_;
}

// ════════════════════════════════════════════════════════════════════════
// Http2ServerRequest (Compatibility Layer)
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL Http2ServerRequest::Http2ServerRequest() {
    ownedEmitter_ = std::make_unique<events::EventEmitter>();
    emitter_ = ownedEmitter_.get();
    setEmitter_(*emitter_);
}

POLYCPP_IMPL Http2ServerRequest::Http2ServerRequest(ServerHttp2Stream stream, const http::Headers& headers)
    : stream_(std::move(stream)), headers_(headers) {
    ownedEmitter_ = std::make_unique<events::EventEmitter>();
    emitter_ = ownedEmitter_.get();
    setEmitter_(*emitter_);

    // Proxy stream events to the request's own emitter so that callers can
    // use req.on(event::Data, ...) instead of req.stream().on(event::Data, ...).
    // Capture the raw emitter pointer — it is owned by ownedEmitter_ (unique_ptr)
    // which stays alive for the full lifetime of this request object.
    auto* em = emitter_;
    stream_.on(event::Data, [em](const std::string& chunk) {
        em->emit(event::Data, chunk);
    });
    stream_.on(event::End, [em]() {
        em->emit(event::End);
    });
    stream_.on(event::Close, [em]() {
        em->emit(event::Close);
    });
    stream_.on(event::Aborted, [em]() {
        em->emit(event::Aborted);
    });
}

POLYCPP_IMPL std::string Http2ServerRequest::authority() const {
    return headers_.get(":authority").value_or("");
}

POLYCPP_IMPL bool Http2ServerRequest::complete() const noexcept {
    return stream_.closed() || stream_.endAfterHeaders();
}

POLYCPP_IMPL const http::Headers& Http2ServerRequest::headers() const noexcept {
    return headers_;
}

POLYCPP_IMPL std::string Http2ServerRequest::httpVersion() const {
    return "2.0";
}

POLYCPP_IMPL int Http2ServerRequest::httpVersionMajor() const noexcept {
    return 2;
}

POLYCPP_IMPL int Http2ServerRequest::httpVersionMinor() const {
    return 0;
}

POLYCPP_IMPL std::string Http2ServerRequest::method() const {
    return headers_.get(":method").value_or("GET");
}

POLYCPP_IMPL std::vector<std::string> Http2ServerRequest::rawHeaders() const {
    std::vector<std::string> raw;
    for (const auto& [name, value] : headers_.raw()) {
        raw.push_back(name);
        raw.push_back(value);
    }
    return raw;
}

POLYCPP_IMPL std::vector<std::string> Http2ServerRequest::rawTrailers() const noexcept {
    return {};  // Trailers are received later
}

POLYCPP_IMPL std::string Http2ServerRequest::scheme() const {
    return headers_.get(":scheme").value_or("http");
}

POLYCPP_IMPL ServerHttp2Stream Http2ServerRequest::stream() const {
    return stream_;
}

POLYCPP_IMPL std::string Http2ServerRequest::url() const {
    return headers_.get(":path").value_or("/");
}

POLYCPP_IMPL void Http2ServerRequest::setTimeout(uint32_t msecs, std::function<void()> callback) {
    stream_.setTimeout(msecs, std::move(callback));
}

POLYCPP_IMPL void Http2ServerRequest::destroy(const Error* /*error*/) {
    stream_.close();
}

// ════════════════════════════════════════════════════════════════════════
// Http2ServerResponse (Compatibility Layer)
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL Http2ServerResponse::Http2ServerResponse() {
    ownedEmitter_ = std::make_unique<events::EventEmitter>();
    emitter_ = ownedEmitter_.get();
    setEmitter_(*emitter_);
}

POLYCPP_IMPL Http2ServerResponse::Http2ServerResponse(ServerHttp2Stream stream)
    : stream_(std::move(stream)) {
    ownedEmitter_ = std::make_unique<events::EventEmitter>();
    emitter_ = ownedEmitter_.get();
    setEmitter_(*emitter_);

    // Proxy the stream close event to the response's own emitter so callers
    // can use res.on(event::Close, ...) instead of res.stream().on(event::Close, ...).
    auto* em = emitter_;
    stream_.on(event::Close, [em]() {
        em->emit(event::Close);
    });
}

POLYCPP_IMPL bool Http2ServerResponse::headersSent() const noexcept {
    return headersSent_;
}

POLYCPP_IMPL int Http2ServerResponse::statusCode() const noexcept {
    return statusCode_;
}

POLYCPP_IMPL void Http2ServerResponse::setStatusCode(int code) noexcept {
    statusCode_ = code;
}

POLYCPP_IMPL std::string Http2ServerResponse::statusMessage() const {
    // HTTP/2 does not use status messages
    return "";
}

POLYCPP_IMPL bool Http2ServerResponse::writable() const noexcept {
    return !finished_ && stream_.valid() && !stream_.closed();
}

POLYCPP_IMPL bool Http2ServerResponse::finished() const {
    return finished_;
}

POLYCPP_IMPL ServerHttp2Stream Http2ServerResponse::stream() const {
    return stream_;
}

POLYCPP_IMPL void Http2ServerResponse::setHeader(const std::string& name, const std::string& value) {
    // Headers::set() lowercases name and replaces any existing value
    headers_.set(name, value);
}

POLYCPP_IMPL std::string Http2ServerResponse::getHeader(const std::string& name) const {
    return headers_.get(name).value_or("");
}

POLYCPP_IMPL void Http2ServerResponse::removeHeader(const std::string& name) {
    headers_.delete_(name);
}

POLYCPP_IMPL http::Headers Http2ServerResponse::getHeaders() const {
    return headers_;
}

POLYCPP_IMPL bool Http2ServerResponse::hasHeader(const std::string& name) const {
    return headers_.has(name);
}

POLYCPP_IMPL void Http2ServerResponse::writeHead(int statusCode, const http::Headers& headers) {
    if (headersSent_) return;
    statusCode_ = statusCode;
    // Merge incoming headers into stored headers (Headers::set lowercases names)
    for (const auto& [name, value] : headers.toHttp2Pairs()) {
        headers_.set(name, value);
    }

    // Build response headers with :status FIRST (RFC 9113: pseudo-headers must precede regular headers)
    http::Headers responseHeaders;
    responseHeaders.set(":status", std::to_string(statusCode_));
    for (const auto& [name, value] : headers_.toHttp2Pairs()) {
        responseHeaders.append(name, value);
    }
    stream_.respond(responseHeaders);
    headersSent_ = true;
}

POLYCPP_IMPL bool Http2ServerResponse::write(const std::string& data, std::function<void()> callback) {
    if (finished_) return false;

    // Auto-send headers if not sent
    if (!headersSent_) {
        writeHead(statusCode_);
    }

    stream_.write(data, std::move(callback));
    return true;
}

POLYCPP_IMPL void Http2ServerResponse::end(const std::string& data, std::function<void()> callback) {
    if (finished_) return;

    // Auto-send headers if not sent
    if (!headersSent_) {
        writeHead(statusCode_);
    }

    finished_ = true;
    stream_.end(data, std::move(callback));

    if (emitter_) emitter_->emit(event::Finish);
}

POLYCPP_IMPL void Http2ServerResponse::addTrailers(const http::Headers& headers) {
    stream_.sendTrailers(headers);
}

POLYCPP_IMPL void Http2ServerResponse::setTimeout(uint32_t msecs, std::function<void()> callback) {
    stream_.setTimeout(msecs, std::move(callback));
}

POLYCPP_IMPL void Http2ServerResponse::createPushResponse(
    const http::Headers& headers,
    std::function<void(const Error*, Http2ServerResponse)> callback) {
    stream_.pushStream(headers, [callback](const Error* err, ServerHttp2Stream pushStream, const http::Headers&) {
        if (callback) {
            callback(err, Http2ServerResponse(pushStream));
        }
    });
}

// ════════════════════════════════════════════════════════════════════════
// Module-level factory functions
// ════════════════════════════════════════════════════════════════════════

POLYCPP_IMPL Http2Server createServer(EventContext& ctx,
                                const ServerOptions& options,
                                std::function<void(ServerHttp2Stream, http::Headers)> onStream) {
    auto impl = std::make_shared<impl::Http2ServerImpl>(ctx, options, false);
    if (onStream) {
        impl->setStreamHandler(std::move(onStream));
    }
    return Http2Server(impl);
}

POLYCPP_IMPL Http2SecureServer createSecureServer(
    EventContext& ctx,
    const SecureServerOptions& options,
    std::function<void(ServerHttp2Stream, http::Headers)> onStream) {
    auto impl = std::make_shared<impl::Http2ServerImpl>(ctx, options, true);
    impl->configureTls(options);
    if (onStream) {
        impl->setStreamHandler(std::move(onStream));
    }
    return Http2SecureServer(impl);
}

// ── Client connect ─────────────────────────────────────────────────

namespace detail {

/**
 * @brief Parses an authority URL into host, port, and scheme.
 * @param authority The authority string (e.g., "http://localhost:8080").
 * @param host Output host.
 * @param port Output port.
 * @param scheme Output scheme.
 */
POLYCPP_IMPL void parseAuthority(const std::string& authority,
                           std::string& host, uint16_t& port, std::string& scheme) {
    scheme = "https";
    host = "localhost";
    port = 443;

    std::string url = authority;

    // Extract scheme
    auto schemeEnd = url.find("://");
    if (schemeEnd != std::string::npos) {
        scheme = url.substr(0, schemeEnd);
        url = url.substr(schemeEnd + 3);
    }

    // Strip path component
    auto pathStart = url.find('/');
    if (pathStart != std::string::npos) {
        url = url.substr(0, pathStart);
    }

    // Extract host:port — handle bracketed IPv6 addresses.
    // For "[::1]:8443", the port separator is the colon AFTER the closing
    // bracket, not the colons inside the IPv6 address.
    std::string::size_type portStart = std::string::npos;
    if (!url.empty() && url[0] == '[') {
        // Bracketed IPv6: find closing bracket, then look for port colon
        auto bracketEnd = url.find(']');
        if (bracketEnd != std::string::npos) {
            // Port colon (if any) must be immediately after the closing bracket
            if (bracketEnd + 1 < url.size() && url[bracketEnd + 1] == ':') {
                portStart = bracketEnd + 1;
            }
            // else: no port specified, portStart stays npos
        }
        // else: malformed bracket — treat entire string as host
    } else {
        portStart = url.find(':');
    }

    if (portStart != std::string::npos) {
        host = url.substr(0, portStart);
        port = static_cast<uint16_t>(std::stoi(url.substr(portStart + 1)));
    } else {
        host = url;
        port = (scheme == "http") ? 80 : 443;
    }
}

} // namespace detail

POLYCPP_IMPL ClientHttp2Session connect(EventContext& ctx,
                                  const std::string& authority,
                                  const ClientSessionOptions& options,
                                  std::function<void(ClientHttp2Session&)> callback) {
    std::string host, scheme;
    uint16_t port;
    detail::parseAuthority(authority, host, port, scheme);

    auto sessionImpl = std::make_shared<impl::Http2SessionImpl>(false, options);
    sessionImpl->setOriginSet(authority);

    bool isSecure = (scheme == "https");
    sessionImpl->setEncrypted(isSecure);

    sessionImpl->initialize();

    ClientHttp2Session session(sessionImpl);

    // Connect via TCP
    auto socket = std::make_shared<io::TcpSocket>(ctx);
    socket->asyncConnect(host, port, [sessionImpl, socket, session, callback, isSecure](std::error_code ec) mutable {
        if (ec) {
            sessionImpl->emitter().emit(event::Error_, Error("Connection failed: " + ec.message()));
            return;
        }

        sessionImpl->emitter().emit(event::Connect, session, SessionState{});

        if (callback) {
            callback(session);
        }

        // Start read loop
        auto readBuf = std::make_shared<std::vector<uint8_t>>(16384);
        std::function<void()> readLoop;
        readLoop = [sessionImpl, socket, readBuf, &readLoop]() {
            socket->asyncReadSome(readBuf->data(), readBuf->size(),
                [sessionImpl, socket, readBuf, &readLoop](std::error_code ec, size_t bytesRead) {
                    if (ec || bytesRead == 0) {
                        sessionImpl->close(nullptr);
                        return;
                    }
                    sessionImpl->feedData(readBuf->data(), bytesRead);

                    // Write output
                    if (sessionImpl->hasPendingOutput()) {
                        auto output = std::make_shared<std::string>(sessionImpl->consumeOutput());
                        socket->asyncWrite(output->data(), output->size(),
                            [output](std::error_code, size_t) {});
                    }

                    if (!sessionImpl->closed() && !sessionImpl->destroyed()) {
                        readLoop();
                    }
                });
        };

        // Write initial output (connection preface + settings)
        if (sessionImpl->hasPendingOutput()) {
            auto output = std::make_shared<std::string>(sessionImpl->consumeOutput());
            socket->asyncWrite(output->data(), output->size(),
                [sessionImpl, socket, readLoop, output](std::error_code ec, size_t) {
                    if (!ec) {
                        readLoop();
                    }
                });
        }
    });

    return session;
}

POLYCPP_IMPL ClientHttp2Session connect(EventContext& ctx,
                                  const std::string& authority,
                                  const SecureClientSessionOptions& options,
                                  std::function<void(ClientHttp2Session&)> callback) {
    // Convert to base ClientSessionOptions and connect
    ClientSessionOptions baseOpts;
    static_cast<SessionOptions&>(baseOpts) = options;
    baseOpts.maxReservedRemoteStreams = options.maxReservedRemoteStreams;
    baseOpts.protocol = options.protocol.value_or("https:");
    return connect(ctx, authority, baseOpts, std::move(callback));
}

} // namespace http2
} // namespace polycpp
