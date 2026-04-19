#pragma once

/**
 * @file nghttp2_allocator.hpp
 * @brief Custom nghttp2 memory allocator.
 *
 * Provides a thin wrapper around the standard allocator for nghttp2,
 * enabling future memory tracking and limiting.
 *
 */

#include <cstdlib>
#include <cstddef>
#include <polycpp/platform/types.hpp>

#include <nghttp2/nghttp2.h>

namespace polycpp {
namespace http2 {
namespace impl {

/**
 * @brief Custom memory allocator callbacks for nghttp2.
 *
 * Wraps standard malloc/free/calloc/realloc for nghttp2's nghttp2_mem
 * interface. Allows future memory tracking and session memory limits.
 *
 */
struct NgHttp2Allocator {
    /**
     * @brief Returns a static nghttp2_mem struct using standard allocators.
     * @return Pointer to a static nghttp2_mem with standard allocation functions.
     */
    static inline nghttp2_mem* get() noexcept {
        static nghttp2_mem mem = {
            nullptr,  // mem_user_data
            mallocCb,
            freeCb,
            callocCb,
            reallocCb
        };
        return &mem;
    }

private:
    static inline void* mallocCb(size_t size, void* /*user_data*/) noexcept {
        return std::malloc(size);
    }

    static inline void freeCb(void* ptr, void* /*user_data*/) noexcept {
        std::free(ptr);
    }

    static inline void* callocCb(size_t nmemb, size_t size, void* /*user_data*/) noexcept {
        return std::calloc(nmemb, size);
    }

    static inline void* reallocCb(void* ptr, size_t size, void* /*user_data*/) {
        return std::realloc(ptr, size);
    }
};

} // namespace impl
} // namespace http2
} // namespace polycpp
