#pragma once

#include <gtest/gtest.h>
#include <polycpp/io/event_context.hpp>
#include <polycpp/io/tcp_acceptor.hpp>

#include <array>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "Iphlpapi.lib")
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace test::ipv6 {

struct ScopedIpv6Interface {
    std::string zoneToken;
    std::string name;
    std::string address;
};

inline bool ipv6LoopbackAvailable() {
    polycpp::EventContext ctx;
    polycpp::io::TcpAcceptor probe(ctx);
    std::error_code ec;
    probe.open(AF_INET6, ec);
    if (ec) {
        return false;
    }

    probe.bind("::1", 0, ec);
    std::error_code closeEc;
    probe.close(closeEc);
    return !ec;
}

inline bool localhostResolvesToBothFamilies() {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (::getaddrinfo("localhost", nullptr, &hints, &result) != 0) {
        return false;
    }

    bool hasV4 = false;
    bool hasV6 = false;
    for (addrinfo* current = result; current; current = current->ai_next) {
        hasV4 = hasV4 || current->ai_family == AF_INET;
        hasV6 = hasV6 || current->ai_family == AF_INET6;
    }

    ::freeaddrinfo(result);
    return hasV4 && hasV6;
}

inline std::optional<ScopedIpv6Interface> findScopedIpv6Interface() {
#if defined(_WIN32)
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                  GAA_FLAG_SKIP_DNS_SERVER;
    ULONG bufLen = 16 * 1024;
    std::vector<unsigned char> buf(bufLen);

    auto* adapters =
        reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    ULONG rc = GetAdaptersAddresses(AF_INET6, flags, nullptr, adapters, &bufLen);
    if (rc == ERROR_BUFFER_OVERFLOW) {
        buf.resize(bufLen);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
        rc = GetAdaptersAddresses(AF_INET6, flags, nullptr, adapters, &bufLen);
    }
    if (rc != NO_ERROR) {
        return std::nullopt;
    }

    std::array<char, INET6_ADDRSTRLEN> addrBuf{};
    for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
        for (auto* ua = adapter->FirstUnicastAddress; ua; ua = ua->Next) {
            if (!ua->Address.lpSockaddr ||
                ua->Address.lpSockaddr->sa_family != AF_INET6) {
                continue;
            }

            auto* sin6 = reinterpret_cast<sockaddr_in6*>(ua->Address.lpSockaddr);
            if (!IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
                continue;
            }
            if (!InetNtopA(AF_INET6, &sin6->sin6_addr,
                           addrBuf.data(),
                           static_cast<DWORD>(addrBuf.size()))) {
                continue;
            }

            return ScopedIpv6Interface{
                std::to_string(adapter->Ipv6IfIndex),
                adapter->AdapterName ? adapter->AdapterName : "",
                addrBuf.data()
            };
        }
    }
    return std::nullopt;
#else
    ifaddrs* ifaddr = nullptr;
    if (::getifaddrs(&ifaddr) != 0) {
        return std::nullopt;
    }

    std::array<char, INET6_ADDRSTRLEN> addrBuf{};
    for (ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET6) {
            continue;
        }

        auto* sin6 = reinterpret_cast<sockaddr_in6*>(ifa->ifa_addr);
        if (!IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
            continue;
        }
        if (!::inet_ntop(AF_INET6, &sin6->sin6_addr,
                         addrBuf.data(), addrBuf.size())) {
            continue;
        }

        ScopedIpv6Interface result{
            ifa->ifa_name ? ifa->ifa_name : "",
            ifa->ifa_name ? ifa->ifa_name : "",
            addrBuf.data()
        };
        ::freeifaddrs(ifaddr);
        return result;
    }

    ::freeifaddrs(ifaddr);
    return std::nullopt;
#endif
}

} // namespace test::ipv6
