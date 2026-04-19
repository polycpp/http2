#pragma once

/**
 * @file aggregator.hpp
 * @brief Aggregator header for the http2 module.
 *
 * Includes all declaration and detail headers in the correct dependency order.
 *
 */

// Phase 1: Main declarations (types, constants, function declarations, handle classes)
#include <polycpp/http2/http2.hpp>

// Phase 2: Utility helpers (always visible — functions are inline)
#include <polycpp/http2/detail/util.hpp>

// Phase 3: nghttp2 allocator (always visible — header-only utility)
#include <polycpp/http2/detail/nghttp2_allocator.hpp>

// Phase 4: Impl class declarations (always visible — needed by template bodies)
#include <polycpp/http2/detail/session_impl.hpp>
#include <polycpp/http2/detail/server_impl_decl.hpp>

// Phase 5: Non-template implementations
#ifdef POLYCPP_HEADER_ONLY
// http::Headers implementation needed since http::Headers = http::Headers
#include <polycpp/http/detail/headers_impl.hpp>
#include <polycpp/http2/detail/types_impl.hpp>
#include <polycpp/http2/detail/server_impl.hpp>
#include <polycpp/http2/detail/handle_bodies.hpp>
#endif
