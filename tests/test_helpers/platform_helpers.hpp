#pragma once

/**
 * @file platform_helpers.hpp
 * @brief Shared platform-specific test utilities for Windows/POSIX compatibility.
 */

#include <string>
#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>

#include <polycpp/core/json.hpp>
#include <polycpp/path.hpp>
#include <polycpp/platform/os.hpp>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <cstdlib>
#else
#  include <cstdlib>
#endif

namespace polycpp {
namespace test {

/// Get a platform-appropriate test temp directory under the OS tmpdir.
///
/// Delegates to the library's own `platform::getTmpdir()` (which honours
/// $TMPDIR, returns `/data/local/tmp` on Android, `GetTempPath()` on Windows,
/// `/tmp` on other POSIX) and joins via `polycpp::path::join()` so the
/// separator is platform-appropriate.
///
/// @return e.g. `/tmp/polycpp_test` on Linux, `C:\Users\<u>\AppData\...\polycpp_test`
///         on Windows, `/data/local/tmp/polycpp_test` on Android. No trailing
///         separator — use `path::join()` to append child paths.
inline std::string testTempDir() {
    return polycpp::path::join(polycpp::platform::getTmpdir(), "polycpp_test");
}

/// Build a mkdtemp/mkstemp-compatible template under the OS tmpdir.
///
/// @param prefix  e.g. "polycpp_fs_test". The returned string is
///                `<tmpdir>/<prefix>_XXXXXX`, suitable for `fs::mkdtempSync`
///                or `::mkdtemp`.
inline std::string testTempTemplate(const std::string& prefix) {
    return polycpp::path::join(polycpp::platform::getTmpdir(),
                                prefix + "_XXXXXX");
}

/// Build a path under the OS tmpdir. Convenience over `path::join`.
///
/// @param name  Leaf name or sub-path (can contain separators; `path::join`
///              normalises them).
inline std::string testTempPath(const std::string& name) {
    return polycpp::path::join(polycpp::platform::getTmpdir(), name);
}

inline std::string jsStringLiteral(const std::string& value) {
    return polycpp::JSON::stringify(polycpp::JsonValue(value));
}

/// Find a standard UNIX utility by probing a list of typical install paths.
/// Useful for child_process tests that need an absolute path to a binary
/// (execFile doesn't do PATH lookup).
///
/// Returns the first path in `candidates` that exists as a regular file,
/// or the input `name` unchanged if none match (callers can still try
/// PATH lookup via the shell).
///
/// Typical usage:
///   findSystemExecutable("wc", {"/usr/bin/wc", "/bin/wc", "/system/bin/wc"});
inline std::string findSystemExecutable(const std::string& name,
        std::initializer_list<const char*> candidates) {
    for (const char* c : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(c, ec) && !ec) return c;
    }
    return name;
}

/// Assert that `stmt` throws `polycpp::SystemError` with `expected_code`
/// (e.g. "ENOENT", "EEXIST"). Stricter than EXPECT_THROW(..., polycpp::Error)
/// — required to catch cases where a test passes for the wrong reason
/// (e.g. an Android SELinux EACCES masking an EEXIST that should be tested).
#define EXPECT_SYSTEM_ERROR(stmt, expected_code)                              \
    do {                                                                       \
        try {                                                                  \
            (stmt);                                                            \
            ADD_FAILURE() << "expected SystemError [" << (expected_code)       \
                          << "], no exception thrown";                         \
        } catch (const polycpp::SystemError& _e) {                             \
            EXPECT_EQ(_e.code(), (expected_code))                              \
                << "syscall=" << _e.syscall                                    \
                << " errnum=" << _e.errnum;                                    \
        } catch (const std::exception& _e) {                                   \
            ADD_FAILURE() << "expected SystemError, got std::exception: "      \
                          << _e.what();                                        \
        } catch (...) {                                                        \
            ADD_FAILURE() << "expected SystemError, got unknown exception";    \
        }                                                                      \
    } while (0)

/// Assert that `stmt` throws `polycpp::Error` with `expected_code`
/// (e.g. "ERR_INVALID_ARG_VALUE", "ERR_INVALID_ARG_TYPE"). Use this for
/// library-level validation errors; use EXPECT_SYSTEM_ERROR for syscall
/// failures where the code is a POSIX errno name.
#define EXPECT_ERROR_CODE(stmt, expected_code)                                \
    do {                                                                       \
        try {                                                                  \
            (stmt);                                                            \
            ADD_FAILURE() << "expected Error [" << (expected_code)             \
                          << "], no exception thrown";                         \
        } catch (const polycpp::Error& _e) {                                   \
            EXPECT_EQ(_e.code(), (expected_code)) << "message: " << _e.what(); \
        } catch (const std::exception& _e) {                                   \
            ADD_FAILURE() << "expected polycpp::Error, got std::exception: "   \
                          << _e.what();                                        \
        } catch (...) {                                                        \
            ADD_FAILURE() << "expected polycpp::Error, got unknown exception"; \
        }                                                                      \
    } while (0)

/// Normalize path separators for cross-platform comparison.
inline std::string normPath(const std::string& p) {
    std::string result = p;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

/// Skip test if symlink creation requires elevated privileges.
inline void skipIfNoSymlinkPrivilege() {
#if defined(_WIN32)
    std::error_code ec;
    auto tmp = std::filesystem::temp_directory_path(ec) / "polycpp_symlink_test";
    std::filesystem::create_symlink(".", tmp, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        GTEST_SKIP() << "Symlinks require elevated privileges on Windows";
    }
    std::filesystem::remove(tmp, ec);
#endif
}

/// Set an environment variable (cross-platform).
inline void setEnv(const std::string& name, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

/// Unset an environment variable (cross-platform).
inline void unsetEnv(const std::string& name) {
#if defined(_WIN32)
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
}

/// Platform-specific shell and command constants.
#if defined(_WIN32)
inline constexpr const char* TEST_SHELL = "cmd.exe";
inline constexpr const char* DEV_NULL = "NUL";
inline constexpr const char* ECHO_CMD = "echo";
inline constexpr const char* TRUE_CMD = "cmd.exe /c exit 0";
inline constexpr const char* FALSE_CMD = "cmd.exe /c exit 1";
#else
inline constexpr const char* TEST_SHELL = "/bin/sh";
inline constexpr const char* DEV_NULL = "/dev/null";
inline constexpr const char* ECHO_CMD = "echo";
inline constexpr const char* TRUE_CMD = "true";
inline constexpr const char* FALSE_CMD = "false";
#endif

/// Skip POSIX-only tests on Windows.
#if defined(_WIN32)
#define SKIP_ON_WINDOWS() GTEST_SKIP() << "Test not applicable on Windows"
#define SKIP_IF_WINDOWS(reason) GTEST_SKIP() << reason
#else
#define SKIP_ON_WINDOWS() ((void)0)
#define SKIP_IF_WINDOWS(reason) ((void)0)
#endif

/// Skip Windows-only tests on POSIX.
#if defined(_WIN32)
#define SKIP_ON_POSIX() ((void)0)
#else
#define SKIP_ON_POSIX() GTEST_SKIP() << "Test only applicable on Windows"
#endif

} // namespace test
} // namespace polycpp
