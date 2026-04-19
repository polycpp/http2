# cmake/HTTP2.cmake — nghttp2 for HTTP/2 support
# Included by the root CMakeLists.txt after Unicode.cmake.
# User path → FetchContent (no system detect — pinned version preferred).
# Sets: POLYCPP_HAS_HTTP2, POLYCPP_NGHTTP2_LIB, POLYCPP_NGHTTP2_INCLUDE_DIR

# ── nghttp2 (for HTTP/2 support) ──────────────────────────────────────
option(POLYCPP_WITH_HTTP2 "Enable HTTP/2 support (requires nghttp2)" ON)

if(POLYCPP_WITH_HTTP2)
    set(POLYCPP_NGHTTP2_SOURCE_DIR "" CACHE PATH "Local nghttp2 source directory (optional)")

    # Disable everything except the core C library
    set(ENABLE_LIB_ONLY ON CACHE BOOL "" FORCE)
    set(ENABLE_STATIC_LIB ON CACHE BOOL "" FORCE)
    set(ENABLE_SHARED_LIB OFF CACHE BOOL "" FORCE)
    set(ENABLE_DOC OFF CACHE BOOL "" FORCE)
    set(ENABLE_APP OFF CACHE BOOL "" FORCE)
    set(ENABLE_HPACK_TOOLS OFF CACHE BOOL "" FORCE)
    set(ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ENABLE_FAILMALLOC OFF CACHE BOOL "" FORCE)
    set(ENABLE_THREADS OFF CACHE BOOL "" FORCE)
    set(WITH_JEMALLOC OFF CACHE BOOL "" FORCE)
    set(WITH_LIBXML2 OFF CACHE BOOL "" FORCE)
    set(WITH_MRUBY OFF CACHE BOOL "" FORCE)
    set(WITH_NEVERBLEED OFF CACHE BOOL "" FORCE)
    set(WITH_LIBBPF OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(_POLYCPP_HTTP2_BUILD_SHARED_LIBS_SAVED "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    # Fix: nghttp2's check_type_size("ssize_t") fails under clang because
    # CMAKE_EXTRA_INCLUDE_FILES doesn't include <sys/types.h>.
    # On POSIX systems ssize_t is always available; pre-set the cache entry
    # so nghttp2 skips the broken check and doesn't #define ssize_t int.
    if(UNIX)
        set(HAVE_SIZEOF_SSIZE_T TRUE CACHE INTERNAL "")
        set(SIZEOF_SSIZE_T 8 CACHE INTERNAL "")
    endif()

    if(POLYCPP_NGHTTP2_SOURCE_DIR AND EXISTS "${POLYCPP_NGHTTP2_SOURCE_DIR}/CMakeLists.txt")
        message(STATUS "Using local nghttp2: ${POLYCPP_NGHTTP2_SOURCE_DIR}")
        FetchContent_Declare(nghttp2 SOURCE_DIR "${POLYCPP_NGHTTP2_SOURCE_DIR}")
    else()
        message(STATUS "Fetching nghttp2 from https://github.com/nghttp2/nghttp2")
        FetchContent_Declare(
            nghttp2
            GIT_REPOSITORY https://github.com/nghttp2/nghttp2.git
            GIT_TAG        v1.68.1
            GIT_SHALLOW    TRUE
        )
    endif()
    FetchContent_MakeAvailable(nghttp2)

    set(BUILD_SHARED_LIBS "${_POLYCPP_HTTP2_BUILD_SHARED_LIBS_SAVED}" CACHE BOOL "" FORCE)

    set(POLYCPP_NGHTTP2_LIB nghttp2_static)
    set(POLYCPP_NGHTTP2_INCLUDE_DIR
        "${nghttp2_SOURCE_DIR}/lib/includes"
        "${nghttp2_BINARY_DIR}/lib/includes")
    set(POLYCPP_HAS_HTTP2 TRUE)
endif()
