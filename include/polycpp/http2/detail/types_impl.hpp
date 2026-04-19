#pragma once
#include <polycpp/config.hpp>

/**
 * @file types_impl.hpp
 * @brief Implementation of HTTP/2 settings utility functions.
 *
 * Implements getDefaultSettings(), getPackedSettings(), getUnpackedSettings(),
 * and validateSettings(). Settings serialization uses the HTTP/2 wire format
 * (6 bytes per setting: 2-byte ID + 4-byte value, network byte order) as
 * specified in RFC 9113 Section 6.5.1.
 *
 */

#include <polycpp/http2/http2.hpp>
#include <polycpp/buffer.hpp>
#include <polycpp/core/error.hpp>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace polycpp {
namespace http2 {

// Each settings entry is 6 bytes on the wire (RFC 9113 Section 6.5.1)
static constexpr size_t SETTINGS_ENTRY_SIZE = 6;

// ── getDefaultSettings() ───────────────────────────────────────────────

POLYCPP_IMPL Settings getDefaultSettings() {
    Settings s;
    s.headerTableSize = constants::DEFAULT_SETTINGS_HEADER_TABLE_SIZE;
    s.enablePush = constants::DEFAULT_SETTINGS_ENABLE_PUSH;
    s.initialWindowSize = constants::DEFAULT_SETTINGS_INITIAL_WINDOW_SIZE;
    s.maxFrameSize = constants::DEFAULT_SETTINGS_MAX_FRAME_SIZE;
    // maxConcurrentStreams and maxHeaderListSize are left as nullopt
    // (unlimited by default, matching Node.js)
    s.enableConnectProtocol = false;
    return s;
}

// ── Internal helper: Settings to wire entries ──────────────────────────

struct SettingsEntry {
    uint16_t id;
    uint32_t value;
};

POLYCPP_IMPL std::vector<SettingsEntry> settingsToEntries(const Settings& settings) {
    std::vector<SettingsEntry> entries;
    entries.reserve(7);

    if (settings.headerTableSize.has_value()) {
        entries.push_back({static_cast<uint16_t>(constants::NGHTTP2_SETTINGS_HEADER_TABLE_SIZE),
                           settings.headerTableSize.value()});
    }
    if (settings.enablePush.has_value()) {
        entries.push_back({static_cast<uint16_t>(constants::NGHTTP2_SETTINGS_ENABLE_PUSH),
                           settings.enablePush.value() ? 1u : 0u});
    }
    if (settings.maxConcurrentStreams.has_value()) {
        entries.push_back({static_cast<uint16_t>(constants::NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS),
                           settings.maxConcurrentStreams.value()});
    }
    if (settings.initialWindowSize.has_value()) {
        entries.push_back({static_cast<uint16_t>(constants::NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE),
                           settings.initialWindowSize.value()});
    }
    if (settings.maxFrameSize.has_value()) {
        entries.push_back({static_cast<uint16_t>(constants::NGHTTP2_SETTINGS_MAX_FRAME_SIZE),
                           settings.maxFrameSize.value()});
    }
    if (settings.maxHeaderListSize.has_value()) {
        entries.push_back({static_cast<uint16_t>(constants::NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE),
                           settings.maxHeaderListSize.value()});
    }
    if (settings.enableConnectProtocol.has_value()) {
        entries.push_back({static_cast<uint16_t>(constants::NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL),
                           settings.enableConnectProtocol.value() ? 1u : 0u});
    }
    return entries;
}

// ── getPackedSettings() ────────────────────────────────────────────────

POLYCPP_IMPL buffer::Buffer getPackedSettings(const Settings& settings) {
    auto entries = settingsToEntries(settings);
    if (entries.empty()) {
        return buffer::Buffer::alloc(0);
    }

    // Pack settings in network byte order: 2-byte ID + 4-byte value per entry
    const size_t bufLen = entries.size() * SETTINGS_ENTRY_SIZE;
    std::vector<uint8_t> buf(bufLen);

    for (size_t i = 0; i < entries.size(); ++i) {
        uint8_t* ptr = buf.data() + i * SETTINGS_ENTRY_SIZE;
        // Network byte order (big-endian)
        ptr[0] = static_cast<uint8_t>(entries[i].id >> 8);
        ptr[1] = static_cast<uint8_t>(entries[i].id & 0xFF);
        ptr[2] = static_cast<uint8_t>((entries[i].value >> 24) & 0xFF);
        ptr[3] = static_cast<uint8_t>((entries[i].value >> 16) & 0xFF);
        ptr[4] = static_cast<uint8_t>((entries[i].value >> 8) & 0xFF);
        ptr[5] = static_cast<uint8_t>(entries[i].value & 0xFF);
    }

    return buffer::Buffer::from(buf.data(), bufLen);
}

// ── getUnpackedSettings() ──────────────────────────────────────────────

POLYCPP_IMPL Settings getUnpackedSettings(const buffer::Buffer& buf) {
    const size_t len = buf.length();
    if (len % SETTINGS_ENTRY_SIZE != 0) {
        throw Error("Invalid HTTP/2 settings buffer size: must be multiple of " +
                     std::to_string(SETTINGS_ENTRY_SIZE));
    }

    Settings settings;
    const uint8_t* data = buf.data();
    const size_t numEntries = len / SETTINGS_ENTRY_SIZE;

    for (size_t i = 0; i < numEntries; ++i) {
        const uint8_t* entry = data + i * SETTINGS_ENTRY_SIZE;
        // Network byte order: 2-byte ID + 4-byte value
        uint16_t id = static_cast<uint16_t>((entry[0] << 8) | entry[1]);
        uint32_t value = static_cast<uint32_t>(
            (static_cast<uint32_t>(entry[2]) << 24) |
            (static_cast<uint32_t>(entry[3]) << 16) |
            (static_cast<uint32_t>(entry[4]) << 8) |
            static_cast<uint32_t>(entry[5]));

        switch (id) {
            case constants::NGHTTP2_SETTINGS_HEADER_TABLE_SIZE:
                settings.headerTableSize = value;
                break;
            case constants::NGHTTP2_SETTINGS_ENABLE_PUSH:
                settings.enablePush = (value != 0);
                break;
            case constants::NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS:
                settings.maxConcurrentStreams = value;
                break;
            case constants::NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE:
                settings.initialWindowSize = value;
                break;
            case constants::NGHTTP2_SETTINGS_MAX_FRAME_SIZE:
                settings.maxFrameSize = value;
                break;
            case constants::NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE:
                settings.maxHeaderListSize = value;
                break;
            case constants::NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL:
                settings.enableConnectProtocol = (value != 0);
                break;
            default:
                // Unknown setting IDs are ignored (per RFC 9113 Section 6.5.2)
                break;
        }
    }

    return settings;
}

// ── validateSettings() ─────────────────────────────────────────────────

POLYCPP_IMPL void validateSettings(const Settings& settings) {
    if (settings.initialWindowSize.has_value()) {
        if (settings.initialWindowSize.value() > static_cast<uint32_t>(
                constants::MAX_INITIAL_WINDOW_SIZE)) {
            throw Error("initialWindowSize exceeds maximum (2^31 - 1)");
        }
    }
    if (settings.maxFrameSize.has_value()) {
        uint32_t v = settings.maxFrameSize.value();
        if (v < constants::MIN_MAX_FRAME_SIZE || v > constants::MAX_MAX_FRAME_SIZE) {
            throw Error("maxFrameSize must be between 16384 and 16777215");
        }
    }
}

} // namespace http2
} // namespace polycpp
