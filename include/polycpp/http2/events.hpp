/**
 * @file events.hpp
 * @brief Typed event constants for the HTTP/2 module.
 *
 * These constants enable type-safe event registration via
 * `emitter.on(event::Data, callback)` instead of
 * `emitter.on(stream::event::Data, [](const Buffer&) { ... })`.
 *
 * @see polycpp::events::TypedEvent
 */
#pragma once

#include <polycpp/events/typed_event.hpp>
#include <polycpp/core/error.hpp>
#include <polycpp/http/headers.hpp>
#include <cstdint>
#include <string>

namespace polycpp {
namespace http2 {

// Forward declarations for event argument types
class ServerHttp2Stream;
class ClientHttp2Stream;
class ServerHttp2Session;
class ClientHttp2Session;
struct Settings;
struct SessionState;

/// @defgroup http2_events HTTP/2 Typed Event Constants
/// @brief Compile-time event descriptors for type-safe listener registration.
/// @{

/// @brief Typed event constants for the HTTP/2 module.
namespace event {

// -- Stream events (emitted on Http2Stream / ServerHttp2Stream / ClientHttp2Stream) --

/// @brief Data chunk received. Callback: `void(const std::string& chunk)`.
constexpr events::TypedEvent<"data", const std::string&>         Data;

/// @brief END_STREAM received. Callback: `void()`.
constexpr events::TypedEvent<"end">                           End;

/// @brief Stream destroyed. Callback: `void()`.
constexpr events::TypedEvent<"close">                           Close;

/// @brief Stream aborted (RST_STREAM with error). Callback: `void()`.
constexpr events::TypedEvent<"aborted">                           Aborted;

/// @brief Trailers expected in response. Callback: `void()`.
constexpr events::TypedEvent<"wantTrailers">                           WantTrailers;

/// @brief Trailing headers received. Callback: `void(const http::Headers&)`.
constexpr events::TypedEvent<"trailers", const http::Headers&>       Trailers;

/// @brief Frame send error. Callback: `void(uint8_t type, int code, int32_t streamId)`.
constexpr events::TypedEvent<"frameError", uint8_t, int, int32_t>      FrameError;

// -- Client stream events (emitted on ClientHttp2Stream only) --

/// @brief Response headers received. Callback: `void(const http::Headers&)`.
constexpr events::TypedEvent<"response", const http::Headers&>       Response;

/// @brief Push promise received. Callback: `void(const http::Headers&)`.
constexpr events::TypedEvent<"push", const http::Headers&>       Push;

// -- Session events (emitted on Http2Session / Server/ClientHttp2Session) --

/// @brief Session error. Callback: `void(const Error&)`.
/// @note Named Error_ to avoid collision with polycpp::Error class.
constexpr events::TypedEvent<"error", const Error&>               Error_;

/// @brief Local SETTINGS acknowledged. Callback: `void(const Settings&)`.
constexpr events::TypedEvent<"localSettings", const Settings&>            LocalSettings;

/// @brief Remote SETTINGS received. Callback: `void(const Settings&)`.
constexpr events::TypedEvent<"remoteSettings", const Settings&>            RemoteSettings;

/// @brief PING response received. Callback: `void(double durationMs, const std::string& payload)`.
constexpr events::TypedEvent<"ping", double, const std::string&> Ping;

/// @brief GOAWAY frame received. Callback: `void(uint32_t errorCode, int32_t lastStreamId)`.
constexpr events::TypedEvent<"goaway", uint32_t, int32_t>          Goaway;

// -- Server session "stream" event --
// Note: any_cast requires EXACT type match. ServerHttp2Stream and ClientHttp2Stream
// are different types, so we need separate constants even though both use "stream".

/// @brief New request stream on server. Callback: `void(ServerHttp2Stream, const http::Headers&)`.
constexpr events::TypedEvent<"stream", ServerHttp2Stream, const http::Headers&> ServerStream;

/// @brief Push promise stream on client. Callback: `void(ClientHttp2Stream, const http::Headers&)`.
constexpr events::TypedEvent<"stream", ClientHttp2Stream, const http::Headers&> ClientStream;

// -- Client session events --

/// @brief Client session connected. Callback: `void(ClientHttp2Session, const SessionState&)`.
constexpr events::TypedEvent<"connect", ClientHttp2Session, const SessionState&> Connect;

// -- Server events (emitted on Http2Server / Http2SecureServer) --

/// @brief Server listening on port. Callback: `void()`.
constexpr events::TypedEvent<"listening">                           Listening;

/// @brief New HTTP/2 session accepted. Callback: `void(ServerHttp2Session)`.
constexpr events::TypedEvent<"session", ServerHttp2Session>         Session;

/// @brief TLS handshake error. Callback: `void(const Error&)`.
constexpr events::TypedEvent<"tlsClientError", const Error&>               TlsClientError;

// -- Timeout event (session / server / stream setTimeout) --

/// @brief Timeout fired. Callback: `void()`.
constexpr events::TypedEvent<"timeout">                           Timeout;

// -- Compat layer events (Http2ServerRequest / Http2ServerResponse) --

/// @brief Response fully sent. Callback: `void()`.
constexpr events::TypedEvent<"finish">                           Finish;

/// @brief Write buffer drained (inherited from stream). Callback: `void()`.
constexpr events::TypedEvent<"drain">                           Drain;

} // namespace event

/// @}

} // namespace http2
} // namespace polycpp
