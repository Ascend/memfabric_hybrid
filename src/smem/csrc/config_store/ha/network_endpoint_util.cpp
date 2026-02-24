/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "network_endpoint_util.h"

#include "smem_logger.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace ock {
namespace smem {

class SocketGuard {
public:
    explicit SocketGuard(int fd) noexcept : fd_(fd) {}

    ~SocketGuard() noexcept
    {
        if (fd_ >= 0) {
            (void)close(fd_);
        }
    }

    SocketGuard(const SocketGuard &) = delete;
    SocketGuard &operator=(const SocketGuard &) = delete;

    SocketGuard(SocketGuard &&other) noexcept : fd_(other.fd_)
    {
        other.fd_ = -1;
    }

    SocketGuard &operator=(SocketGuard &&other) noexcept
    {
        if (this != &other) {
            if (fd_ >= 0) {
                (void)close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    [[nodiscard]] int Get() const noexcept
    {
        return fd_;
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        return fd_ >= 0;
    }

private:
    int fd_;
};

namespace {

[[nodiscard]] bool SetNonBlocking(int fd) noexcept
{
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

[[nodiscard]] bool PollConnectComplete(int fd, int timeoutMs, const std::string &ip, uint16_t port) noexcept
{
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLOUT;

    const int pollResult = poll(&pfd, 1, timeoutMs);
    if (pollResult <= 0) {
        SM_LOG_ERROR("poll timeout/error, ip=" << ip << ", port=" << port << ", timeoutMs=" << timeoutMs
                                               << ", pollResult=" << pollResult << ", errno=" << errno
                                               << ", errstr=" << std::strerror(errno));
        return false;
    }

    if ((pfd.revents & POLLOUT) == 0) {
        SM_LOG_ERROR("poll not writable, ip=" << ip << ", port=" << port << ", revents=" << pfd.revents);
        return false;
    }

    int soError = 0;
    socklen_t len = sizeof(soError);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soError, &len) != 0) {
        SM_LOG_ERROR("getsockopt(SO_ERROR) failed, ip=" << ip << ", port=" << port << ", errno=" << errno
                                                        << ", errstr=" << std::strerror(errno));
        return false;
    }

    if (soError != 0) {
        SM_LOG_ERROR("async connect failed, ip=" << ip << ", port=" << port << ", so_error=" << soError
                                                 << ", so_error_str=" << std::strerror(soError));
        return false;
    }

    return true;
}

} // namespace

NetworkEndpointUtil::AddressFamily NetworkEndpointUtil::DetectAddressFamily(const std::string &ip) noexcept
{
    if (ip.empty()) {
        return AddressFamily::STORE_UNKNOWN;
    }

    in_addr addr4{};
    if (inet_pton(AF_INET, ip.c_str(), &addr4) == 1) {
        return AddressFamily::STORE_IPV4;
    }

    in6_addr addr6{};
    if (inet_pton(AF_INET6, ip.c_str(), &addr6) == 1) {
        return AddressFamily::STORE_IPV6;
    }

    return AddressFamily::STORE_UNKNOWN;
}

bool NetworkEndpointUtil::CheckConnectivity(const std::string &ip, uint16_t port) noexcept
{
    const AddressFamily family = DetectAddressFamily(ip);
    if (family == AddressFamily::STORE_UNKNOWN) {
        SM_LOG_ERROR("invalid ip, ip=" << ip << ", port=" << port);
        return false;
    }

    const int af = (family == AddressFamily::STORE_IPV4) ? AF_INET : AF_INET6;
    SocketGuard sockfd(socket(af, SOCK_STREAM, 0));
    if (!sockfd.IsValid()) {
        SM_LOG_ERROR("socket create failed, af=" << af << ", errno=" << errno << ", errstr=" << std::strerror(errno));
        return false;
    }

    if (!SetNonBlocking(sockfd.Get())) {
        SM_LOG_ERROR("set non-blocking failed, fd=" << sockfd.Get() << ", errno=" << errno
                                                    << ", errstr=" << std::strerror(errno));
        return false;
    }

    int connectResult = -1;
    if (family == AddressFamily::STORE_IPV4) {
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
            SM_LOG_ERROR("inet_pton ipv4 failed, ip=" << ip << ", errno=" << errno
                                                      << ", errstr=" << std::strerror(errno));
            return false;
        }

        connectResult = connect(sockfd.Get(), reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr));
    } else {
        sockaddr_in6 serverAddr{};
        serverAddr.sin6_family = AF_INET6;
        serverAddr.sin6_port = htons(port);

        if (inet_pton(AF_INET6, ip.c_str(), &serverAddr.sin6_addr) <= 0) {
            SM_LOG_ERROR("inet_pton ipv6 failed, ip=" << ip << ", errno=" << errno
                                                      << ", errstr=" << std::strerror(errno));
            return false;
        }

        connectResult = connect(sockfd.Get(), reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr));
    }

    if (connectResult == 0) {
        return true;
    }

    if (errno != EINPROGRESS) {
        SM_LOG_ERROR("connect failed immediately, ip=" << ip << ", port=" << port << ", errno=" << errno
                                                       << ", errstr=" << std::strerror(errno));
        return false;
    }

    return PollConnectComplete(sockfd.Get(), kConnectTimeoutMs, ip, port);
}

static bool ParsePortEnv(const char *name, uint16_t &outPort) noexcept
{
    const char *val = std::getenv(name);
    if (val == nullptr || *val == '\0') {
        return false;
    }

    char *end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(val, &end, 10);
    // Must be all digits, and in 1~65535.
    if (errno != 0 || end == val || *end != '\0' || parsed == 0UL || parsed > 65535UL) {
        SM_LOG_ERROR("invalid env value, name=" << name << ", value=" << val << ", errno=" << errno
                                                << ", errstr=" << std::strerror(errno));
        return false;
    }

    outPort = static_cast<uint16_t>(parsed);
    return true;
}

bool NetworkEndpointUtil::FindAvailablePort(uint16_t &port, bool isIpv6) noexcept
{
    const int af = isIpv6 ? AF_INET6 : AF_INET;

    uint16_t startPort = kDefaultStartPort;
    uint16_t maxPort = kDefaultMaxPort;

    uint16_t envStart = 0;
    if (ParsePortEnv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_START", envStart)) {
        startPort = envStart;
    }

    uint16_t envEnd = 0;
    if (ParsePortEnv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_END", envEnd)) {
        maxPort = envEnd;
    }

    // If env range is reversed, fallback to default range.
    if (startPort > maxPort) {
        SM_LOG_ERROR("invalid env range, fallback to default, startPort=" << startPort << ", endPort=" << maxPort
                                                                          << ", defaultStart=" << kDefaultStartPort
                                                                          << ", defaultEnd=" << kDefaultMaxPort);
        startPort = kDefaultStartPort;
        maxPort = kDefaultMaxPort;
    }

    for (uint32_t testPort = startPort; testPort <= maxPort; ++testPort) {
        SocketGuard sockfd(socket(af, SOCK_STREAM, 0));
        if (!sockfd.IsValid()) {
            continue;
        }

        constexpr int kReuseAddrEnabled = 1;
        if (setsockopt(sockfd.Get(), SOL_SOCKET, SO_REUSEADDR, &kReuseAddrEnabled, sizeof(kReuseAddrEnabled)) < 0) {
            continue;
        }

        if (isIpv6) {
            constexpr int kV6OnlyEnabled = 1;
            (void)setsockopt(sockfd.Get(), IPPROTO_IPV6, IPV6_V6ONLY, &kV6OnlyEnabled, sizeof(kV6OnlyEnabled));
        }

        bool bindSuccess = false;
        if (isIpv6) {
            sockaddr_in6 addr{};
            addr.sin6_family = AF_INET6;
            addr.sin6_addr = in6addr_any;
            addr.sin6_port = htons(static_cast<uint16_t>(testPort));

            bindSuccess = (bind(sockfd.Get(), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);
        } else {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(static_cast<uint16_t>(testPort));

            bindSuccess = (bind(sockfd.Get(), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);
        }

        if (bindSuccess) {
            port = static_cast<uint16_t>(testPort);
            return true;
        }
    }

    SM_LOG_ERROR("no available port found, isIpv6=" << isIpv6 << ", startPort=" << startPort
                                                    << ", endPort=" << maxPort);
    return false;
}

bool NetworkEndpointUtil::ExtractIpAndPort(const std::string &endpoint, std::string &ip, uint16_t &port,
                                           BackendType &type) noexcept
{
    if (endpoint.empty()) {
        SM_LOG_ERROR("endpoint is empty.");
        return false;
    }

    type = BackendType::UNKNOWN;

    constexpr const char *kTcpPrefix = "tcp://";
    constexpr const char *kHttpPrefix = "etcd://";
    constexpr size_t kTcpPrefixLen = 6;
    constexpr size_t kHttpPrefixLen = 7;
    constexpr size_t kBracketPortOffset = 2; // "]:" length
    constexpr uint32_t kMaxPort = 65535U;
    constexpr uint32_t kBase10 = 10U;

    std::string processed;
    if (endpoint.compare(0, kTcpPrefixLen, kTcpPrefix) == 0) {
        processed = endpoint.substr(kTcpPrefixLen);
        type = BackendType::TCP;
    } else if (endpoint.compare(0, kHttpPrefixLen, kHttpPrefix) == 0) {
        processed = endpoint.substr(kHttpPrefixLen);
        type = BackendType::ETCD;
    } else {
        SM_LOG_ERROR("protocol not supported, endpoint=" << endpoint);
        return false;
    }

    if (processed.empty()) {
        SM_LOG_ERROR("empty host:port part, endpoint=" << endpoint);
        return false;
    }

    std::string ipStr;
    std::string portStr;

    if (processed[0] == '[') {
        const size_t closeBracket = processed.find(']');
        if (closeBracket == std::string::npos || closeBracket + 1 >= processed.size() ||
            processed[closeBracket + 1] != ':') {
            SM_LOG_ERROR("invalid bracketed ipv6 endpoint, processed=" << processed);
            return false;
        }

        ipStr = processed.substr(1, closeBracket - 1);
        portStr = processed.substr(closeBracket + kBracketPortOffset);
    } else {
        const size_t colonPos = processed.rfind(':');
        if (colonPos == std::string::npos || colonPos == 0 || colonPos == processed.size() - 1) {
            SM_LOG_ERROR("invalid host:port format, processed=" << processed);
            return false;
        }

        ipStr = processed.substr(0, colonPos);
        portStr = processed.substr(colonPos + 1);
    }

    const AddressFamily family = DetectAddressFamily(ipStr);
    if (family == AddressFamily::STORE_UNKNOWN) {
        SM_LOG_ERROR("invalid ip, ip=" << ipStr << ", endpoint=" << endpoint);
        return false;
    }

    if (portStr.empty()) {
        SM_LOG_ERROR("empty port, endpoint=" << endpoint);
        return false;
    }

    uint32_t portValue = 0;
    for (const char c : portStr) {
        if (c < '0' || c > '9') {
            SM_LOG_ERROR("non-digit port char, portStr=" << portStr << ", endpoint=" << endpoint);
            return false;
        }
        portValue = (portValue * kBase10) + static_cast<uint32_t>(c - '0');
        if (portValue > kMaxPort) {
            SM_LOG_ERROR("port out of range, portStr=" << portStr << ", endpoint=" << endpoint);
            return false;
        }
    }

    if (portValue == 0U) {
        SM_LOG_ERROR("port cannot be zero, endpoint=" << endpoint);
        return false;
    }

    port = static_cast<uint16_t>(portValue);
    ip = ipStr;
    return true;
}

std::string NetworkEndpointUtil::BuildEndpoint(const std::string &protocol, const std::string &ip,
                                               uint16_t port) noexcept
{
    const AddressFamily family = DetectAddressFamily(ip);
    if (family == AddressFamily::STORE_UNKNOWN) {
        SM_LOG_ERROR("invalid ip, protocol=" << protocol << ", ip=" << ip << ", port=" << port);
        return "";
    }

    try {
        if (family == AddressFamily::STORE_IPV4) {
            return protocol + "://" + ip + ":" + std::to_string(port);
        }
        return protocol + "://[" + ip + "]:" + std::to_string(port);
    } catch (const std::exception &e) {
        SM_LOG_ERROR("exception, protocol=" << protocol << ", ip=" << ip << ", port=" << port << ", what=" << e.what());
        return "";
    } catch (...) {
        SM_LOG_ERROR("unknown exception, protocol=" << protocol << ", ip=" << ip << ", port=" << port);
        return "";
    }
}

bool NetworkEndpointUtil::GetLocalIpWithTarget(const std::string &target, std::string &local) noexcept
{
    const AddressFamily family = DetectAddressFamily(target);
    if (family == AddressFamily::STORE_UNKNOWN) {
        SM_LOG_ERROR("invalid target ip, target=" << target);
        return false;
    }

    const int af = (family == AddressFamily::STORE_IPV4) ? AF_INET : AF_INET6;
    SocketGuard sockfd(socket(af, SOCK_DGRAM, 0));
    if (!sockfd.IsValid()) {
        SM_LOG_ERROR("socket create failed, af=" << af << ", errno=" << errno << ", errstr=" << std::strerror(errno));
        return false;
    }

    int connectResult = -1;
    if (family == AddressFamily::STORE_IPV4) {
        sockaddr_in targetAddr{};
        targetAddr.sin_family = AF_INET;
        targetAddr.sin_port = htons(kProbePort);

        if (inet_pton(AF_INET, target.c_str(), &targetAddr.sin_addr) <= 0) {
            SM_LOG_ERROR("inet_pton ipv4 failed, target=" << target << ", errno=" << errno
                                                          << ", errstr=" << std::strerror(errno));
            return false;
        }

        connectResult = connect(sockfd.Get(), reinterpret_cast<sockaddr *>(&targetAddr), sizeof(targetAddr));
    } else {
        sockaddr_in6 targetAddr{};
        targetAddr.sin6_family = AF_INET6;
        targetAddr.sin6_port = htons(kProbePort);

        if (inet_pton(AF_INET6, target.c_str(), &targetAddr.sin6_addr) <= 0) {
            SM_LOG_ERROR("inet_pton ipv6 failed, target=" << target << ", errno=" << errno
                                                          << ", errstr=" << std::strerror(errno));
            return false;
        }

        connectResult = connect(sockfd.Get(), reinterpret_cast<sockaddr *>(&targetAddr), sizeof(targetAddr));
    }

    if (connectResult < 0) {
        SM_LOG_ERROR("connect failed, target=" << target << ", errno=" << errno << ", errstr=" << std::strerror(errno));
        return false;
    }

    if (family == AddressFamily::STORE_IPV4) {
        sockaddr_in localAddr{};
        socklen_t addrLen = sizeof(localAddr);

        if (getsockname(sockfd.Get(), reinterpret_cast<sockaddr *>(&localAddr), &addrLen) < 0) {
            SM_LOG_ERROR("getsockname ipv4 failed, errno=" << errno << ", errstr=" << std::strerror(errno));
            return false;
        }

        char ipBuffer[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &localAddr.sin_addr, ipBuffer, sizeof(ipBuffer)) == nullptr) {
            SM_LOG_ERROR("inet_ntop ipv4 failed, errno=" << errno << ", errstr=" << std::strerror(errno));
            return false;
        }

        local = ipBuffer;
    } else {
        sockaddr_in6 localAddr{};
        socklen_t addrLen = sizeof(localAddr);

        if (getsockname(sockfd.Get(), reinterpret_cast<sockaddr *>(&localAddr), &addrLen) < 0) {
            SM_LOG_ERROR("getsockname ipv6 failed, errno=" << errno << ", errstr=" << std::strerror(errno));
            return false;
        }

        char ipBuffer[INET6_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET6, &localAddr.sin6_addr, ipBuffer, sizeof(ipBuffer)) == nullptr) {
            SM_LOG_ERROR("inet_ntop ipv6 failed, errno=" << errno << ", errstr=" << std::strerror(errno));
            return false;
        }

        local = ipBuffer;
    }

    return true;
}

void NetworkEndpointUtil::ConvertToTcpUrl(std::string &url) noexcept
{
    constexpr std::string_view kUrlSeparator = "://";

    const size_t pos = url.find(kUrlSeparator);
    if (pos == std::string::npos) {
        url = "tcp://" + url;
        return;
    }

    const std::string_view scheme(url.data(), pos);
    if (scheme == "tcp") {
        return;
    }

    url.replace(0, pos, "tcp");
}

} // namespace smem
} // namespace ock
