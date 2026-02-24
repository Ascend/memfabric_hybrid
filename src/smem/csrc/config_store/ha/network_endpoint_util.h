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

#ifndef SMEM_SMEM_STORE_NET_H
#define SMEM_SMEM_STORE_NET_H

#include <cstdint>
#include <string>

namespace ock {
namespace smem {

/**
 * @brief Backend storage type.
 */
enum class BackendType { TCP, ETCD, UNKNOWN };

/**
 * @brief Network utility helper for endpoint parsing and connectivity detection.
 */
class NetworkEndpointUtil {
public:
    /**
     * @brief Check whether a TCP connection to the specified IP and port can be established.
     *
     * Supports both IPv4 and IPv6 literal addresses.
     *
     * @param ip      [in] Target IP address (IPv4 or IPv6 literal).
     * @param port    [in] Target port number.
     *
     * @return true if connection can be successfully established within timeout; otherwise false.
     */
    [[nodiscard]] static bool CheckConnectivity(const std::string &ip, uint16_t port) noexcept;

    /**
     * @brief Find an available TCP port within configured port range.
     *
     * The search starts from kStartPort and continues until kMaxPort.
     *
     * @param port     [out] Available port number if found.
     * @param isIpv6   [in]  true to search using IPv6 socket; false for IPv4.
     *
     * @return true if an available port is found; otherwise false.
     */
    [[nodiscard]] static bool FindAvailablePort(uint16_t &port, bool isIpv6) noexcept;

    /**
     * @brief Extract IP and port from a formatted endpoint string.
     *
     * Supported formats:
     * - tcp://127.0.0.1:12335
     * - etcd://127.0.0.1:12335
     * - tcp://[::1]:8080
     * - etcd://[2001:db8::1]:8080
     *
     * @param endpoint   [in]  Endpoint string.
     * @param ip         [out] Extracted IP address.
     * @param port       [out] Extracted port number.
     * @param type       [out] Extracted backend type.
     *
     * @return true if parsing succeeds; otherwise false.
     */
    [[nodiscard]] static bool ExtractIpAndPort(const std::string &endpoint, std::string &ip, uint16_t &port,
                                               BackendType &type) noexcept;

    /**
     * @brief Extract IP and port from endpoint string.
     *
     * Backend type is ignored.
     *
     * @param endpoint   [in]  Endpoint string.
     * @param ip         [out] Extracted IP address.
     * @param port       [out] Extracted port number.
     *
     * @return true if parsing succeeds; otherwise false.
     */
    [[nodiscard]] static bool ExtractIpAndPort(const std::string &endpoint, std::string &ip, uint16_t &port) noexcept
    {
        BackendType type = BackendType::UNKNOWN;
        return ExtractIpAndPort(endpoint, ip, port, type);
    }

    /**
     * @brief Build endpoint string from protocol, IP, and port.
     *
     * IPv4 format:  protocol://ip:port
     * IPv6 format:  protocol://[ipv6]:port
     *
     * @param protocol  [in] Protocol scheme (e.g., "tcp", "etcd").
     * @param ip        [in] IP address (IPv4 or IPv6 literal).
     * @param port      [in] Port number.
     *
     * @return Constructed endpoint string. Returns empty string on failure.
     */
    [[nodiscard]] static std::string BuildEndpoint(const std::string &protocol, const std::string &ip,
                                                   uint16_t port) noexcept;

    /**
     * @brief Obtain the local IP address used to communicate with a target IP.
     *
     * This method determines the outbound interface used to reach the target.
     *
     * @param target   [in]  Target IP address (IPv4 or IPv6 literal).
     * @param local    [out] Local IP address selected by the system routing table.
     *
     * @return true if local IP is successfully obtained; otherwise false.
     */
    [[nodiscard]] static bool GetLocalIpWithTarget(const std::string &target, std::string &local) noexcept;

    /**
     * @brief Convert a URL to tcp scheme.
     *
     * If scheme is missing, "tcp://" will be prepended.
     * If scheme exists and is not "tcp", it will be replaced.
     *
     * @param url   [in,out] URL string to be converted.
     */
    static void ConvertToTcpUrl(std::string &url) noexcept;

private:
    NetworkEndpointUtil() = default;

    /**
     * @brief Address family type.
     */
    enum class AddressFamily { STORE_IPV4, STORE_IPV6, STORE_UNKNOWN };

    /**
     * @brief Detect address family from IP literal string.
     *
     * @param ip   [in] IP literal string.
     *
     * @return Detected address family.
     */
    [[nodiscard]] static AddressFamily DetectAddressFamily(const std::string &ip) noexcept;

    static constexpr uint16_t kDefaultStartPort = 9000;
    static constexpr uint16_t kDefaultMaxPort = 65535;
    static constexpr int kConnectTimeoutMs = 1000;
    static constexpr uint16_t kProbePort = 80;
};

} // namespace smem
} // namespace ock

#endif
