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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include <gtest/gtest.h>
#include "network_endpoint_util.h"

using namespace ock::smem;

namespace {
constexpr uint16_t K_PORT_MIN = 1;
constexpr uint16_t K_PORT_MAX = 65535;
constexpr uint16_t K_PORT_ZERO = 0;
constexpr uint16_t K_PORT_COMMON = 8080;
constexpr uint16_t K_PORT_ETCD = 2379;
constexpr uint16_t K_PORT_TCP_TEST = 12335;
constexpr uint16_t K_PORT_ROUND_TRIP = 12345;
constexpr uint16_t K_PORT_ROUND_TRIP_IPV6 = 9999;
constexpr uint16_t K_PORT_TWO_PARAM = 9000;
constexpr uint16_t K_PORT_LISTENER_IPV4 = 19876;
constexpr uint16_t K_PORT_LISTENER_IPV6 = 19877;
constexpr uint16_t K_PORT_REFUSED = 1;
constexpr uint16_t K_PORT_ENV_RANGE_START = 49200;
constexpr uint16_t K_PORT_ENV_RANGE_END = 49250;
constexpr uint16_t K_PORT_DEFAULT_MIN = 9000;
constexpr int K_LISTEN_BACKLOG = 5;
constexpr int K_SOCK_OPT_ENABLE = 1;
constexpr useconds_t K_CHILD_STARTUP_WAIT_US = 200000;
constexpr int K_EXIT_SUCCESS = 0;
constexpr int K_EXIT_ERR_SOCKET = 1;
constexpr int K_EXIT_ERR_BIND = 2;
constexpr int K_EXIT_ERR_LISTEN = 3;
} // namespace

class NetworkEndpointUtilTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}

    void SetUp() override
    {
        ip_.clear();
        port_ = 0;
        type_ = BackendType::UNKNOWN;
    }

    void TearDown() override {}

protected:
    std::string ip_;
    uint16_t port_ = 0;
    BackendType type_ = BackendType::UNKNOWN;
};

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_tcp_ipv4_normal)
{
    EXPECT_TRUE(NetworkEndpointUtil::ExtractIpAndPort("tcp://127.0.0.1:12335", ip_, port_, type_));
    EXPECT_EQ(ip_, "127.0.0.1");
    EXPECT_EQ(port_, K_PORT_TCP_TEST);
    EXPECT_EQ(type_, BackendType::TCP);
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_etcd_ipv4_normal)
{
    EXPECT_TRUE(NetworkEndpointUtil::ExtractIpAndPort("etcd://192.168.1.100:2379", ip_, port_, type_));
    EXPECT_EQ(ip_, "192.168.1.100");
    EXPECT_EQ(port_, K_PORT_ETCD);
    EXPECT_EQ(type_, BackendType::ETCD);
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_tcp_ipv6_bracketed)
{
    EXPECT_TRUE(NetworkEndpointUtil::ExtractIpAndPort("tcp://[::1]:8080", ip_, port_, type_));
    EXPECT_EQ(ip_, "::1");
    EXPECT_EQ(port_, K_PORT_COMMON);
    EXPECT_EQ(type_, BackendType::TCP);
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_etcd_ipv6_bracketed)
{
    EXPECT_TRUE(NetworkEndpointUtil::ExtractIpAndPort("etcd://[2001:db8::1]:8080", ip_, port_, type_));
    EXPECT_EQ(ip_, "2001:db8::1");
    EXPECT_EQ(port_, K_PORT_COMMON);
    EXPECT_EQ(type_, BackendType::ETCD);
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_port_min)
{
    EXPECT_TRUE(NetworkEndpointUtil::ExtractIpAndPort("tcp://127.0.0.1:1", ip_, port_, type_));
    EXPECT_EQ(port_, K_PORT_MIN);
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_port_max)
{
    EXPECT_TRUE(NetworkEndpointUtil::ExtractIpAndPort("tcp://127.0.0.1:65535", ip_, port_, type_));
    EXPECT_EQ(port_, K_PORT_MAX);
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_empty_endpoint)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_unsupported_protocol)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("http://127.0.0.1:8080", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_missing_port)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://127.0.0.1", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_missing_ip)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://:8080", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_port_zero)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://127.0.0.1:0", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_port_overflow)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://127.0.0.1:65536", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_port_large_overflow)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://127.0.0.1:999999", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_non_digit_port)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://127.0.0.1:abc", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_port_mixed_chars)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://127.0.0.1:80ab", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_invalid_ip)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://999.999.999.999:8080", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_ipv6_missing_close_bracket)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://[::1:8080", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_ipv6_missing_colon_after_bracket)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://[::1]8080", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_empty_after_scheme)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_trailing_colon_no_port)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("tcp://127.0.0.1:", ip_, port_, type_));
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_two_param_success)
{
    EXPECT_TRUE(NetworkEndpointUtil::ExtractIpAndPort("tcp://10.0.0.1:9000", ip_, port_));
    EXPECT_EQ(ip_, "10.0.0.1");
    EXPECT_EQ(port_, K_PORT_TWO_PARAM);
}

TEST_F(NetworkEndpointUtilTest, ExtractIpAndPort_two_param_failure)
{
    EXPECT_FALSE(NetworkEndpointUtil::ExtractIpAndPort("bad://10.0.0.1:9000", ip_, port_));
}

TEST_F(NetworkEndpointUtilTest, BuildEndpoint_ipv4)
{
    EXPECT_EQ(NetworkEndpointUtil::BuildEndpoint("tcp", "127.0.0.1", K_PORT_COMMON), "tcp://127.0.0.1:8080");
}

TEST_F(NetworkEndpointUtilTest, BuildEndpoint_ipv6)
{
    EXPECT_EQ(NetworkEndpointUtil::BuildEndpoint("tcp", "::1", K_PORT_COMMON), "tcp://[::1]:8080");
}

TEST_F(NetworkEndpointUtilTest, BuildEndpoint_ipv6_full)
{
    EXPECT_EQ(NetworkEndpointUtil::BuildEndpoint("etcd", "2001:db8::1", K_PORT_ETCD), "etcd://[2001:db8::1]:2379");
}

TEST_F(NetworkEndpointUtilTest, BuildEndpoint_etcd_ipv4)
{
    EXPECT_EQ(NetworkEndpointUtil::BuildEndpoint("etcd", "10.0.0.1", K_PORT_ETCD), "etcd://10.0.0.1:2379");
}

TEST_F(NetworkEndpointUtilTest, BuildEndpoint_invalid_ip)
{
    EXPECT_TRUE(NetworkEndpointUtil::BuildEndpoint("tcp", "not_an_ip", K_PORT_COMMON).empty());
}

TEST_F(NetworkEndpointUtilTest, BuildEndpoint_empty_ip)
{
    EXPECT_TRUE(NetworkEndpointUtil::BuildEndpoint("tcp", "", K_PORT_COMMON).empty());
}

TEST_F(NetworkEndpointUtilTest, RoundTrip_ipv4)
{
    const std::string endpoint = NetworkEndpointUtil::BuildEndpoint("tcp", "192.168.1.1", K_PORT_ROUND_TRIP);
    ASSERT_FALSE(endpoint.empty());

    ASSERT_TRUE(NetworkEndpointUtil::ExtractIpAndPort(endpoint, ip_, port_, type_));
    EXPECT_EQ(ip_, "192.168.1.1");
    EXPECT_EQ(port_, K_PORT_ROUND_TRIP);
    EXPECT_EQ(type_, BackendType::TCP);
}

TEST_F(NetworkEndpointUtilTest, RoundTrip_ipv6)
{
    const std::string endpoint = NetworkEndpointUtil::BuildEndpoint("etcd", "::1", K_PORT_ROUND_TRIP_IPV6);
    ASSERT_FALSE(endpoint.empty());

    ASSERT_TRUE(NetworkEndpointUtil::ExtractIpAndPort(endpoint, ip_, port_, type_));
    EXPECT_EQ(ip_, "::1");
    EXPECT_EQ(port_, K_PORT_ROUND_TRIP_IPV6);
    EXPECT_EQ(type_, BackendType::ETCD);
}

TEST_F(NetworkEndpointUtilTest, ConvertToTcpUrl_no_scheme)
{
    std::string url = "127.0.0.1:8080";
    NetworkEndpointUtil::ConvertToTcpUrl(url);
    EXPECT_EQ(url, "tcp://127.0.0.1:8080");
}

TEST_F(NetworkEndpointUtilTest, ConvertToTcpUrl_already_tcp)
{
    std::string url = "tcp://127.0.0.1:8080";
    NetworkEndpointUtil::ConvertToTcpUrl(url);
    EXPECT_EQ(url, "tcp://127.0.0.1:8080");
}

TEST_F(NetworkEndpointUtilTest, ConvertToTcpUrl_etcd_to_tcp)
{
    std::string url = "etcd://127.0.0.1:8080";
    NetworkEndpointUtil::ConvertToTcpUrl(url);
    EXPECT_EQ(url, "tcp://127.0.0.1:8080");
}

TEST_F(NetworkEndpointUtilTest, ConvertToTcpUrl_http_to_tcp)
{
    std::string url = "http://10.0.0.1:9090";
    NetworkEndpointUtil::ConvertToTcpUrl(url);
    EXPECT_EQ(url, "tcp://10.0.0.1:9090");
}

TEST_F(NetworkEndpointUtilTest, ConvertToTcpUrl_ipv6_no_scheme)
{
    std::string url = "[::1]:8080";
    NetworkEndpointUtil::ConvertToTcpUrl(url);
    EXPECT_EQ(url, "tcp://[::1]:8080");
}

TEST_F(NetworkEndpointUtilTest, ConvertToTcpUrl_ipv6_etcd_to_tcp)
{
    std::string url = "etcd://[::1]:8080";
    NetworkEndpointUtil::ConvertToTcpUrl(url);
    EXPECT_EQ(url, "tcp://[::1]:8080");
}

TEST_F(NetworkEndpointUtilTest, CheckConnectivity_invalid_ip)
{
    EXPECT_FALSE(NetworkEndpointUtil::CheckConnectivity("", K_PORT_COMMON));
    EXPECT_FALSE(NetworkEndpointUtil::CheckConnectivity("not_an_ip", K_PORT_COMMON));
}

TEST_F(NetworkEndpointUtilTest, CheckConnectivity_connect_to_listening_port_ipv4)
{
    pid_t pid = fork();
    ASSERT_NE(pid, -1);

    if (pid == 0) {
        // Child: create a TCP listener, accept one connection, then exit.
        int serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd < 0) {
            _exit(K_EXIT_ERR_SOCKET);
        }

        (void)setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &K_SOCK_OPT_ENABLE, sizeof(K_SOCK_OPT_ENABLE));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(K_PORT_LISTENER_IPV4);

        if (bind(serverFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
            close(serverFd);
            _exit(K_EXIT_ERR_BIND);
        }

        if (listen(serverFd, K_LISTEN_BACKLOG) < 0) {
            close(serverFd);
            _exit(K_EXIT_ERR_LISTEN);
        }

        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
        if (clientFd >= 0) {
            close(clientFd);
        }

        close(serverFd);
        _exit(K_EXIT_SUCCESS);
    }

    // Parent: give child time to start listening, then check connectivity
    usleep(K_CHILD_STARTUP_WAIT_US);
    EXPECT_TRUE(NetworkEndpointUtil::CheckConnectivity("127.0.0.1", K_PORT_LISTENER_IPV4));

    int status = 0;
    EXPECT_NE(waitpid(pid, &status, 0), -1);
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), K_EXIT_SUCCESS);
}

TEST_F(NetworkEndpointUtilTest, CheckConnectivity_connect_to_listening_port_ipv6)
{
    pid_t pid = fork();
    ASSERT_NE(pid, -1);

    if (pid == 0) {
        int serverFd = socket(AF_INET6, SOCK_STREAM, 0);
        if (serverFd < 0) {
            _exit(K_EXIT_ERR_SOCKET);
        }

        (void)setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &K_SOCK_OPT_ENABLE, sizeof(K_SOCK_OPT_ENABLE));
        (void)setsockopt(serverFd, IPPROTO_IPV6, IPV6_V6ONLY, &K_SOCK_OPT_ENABLE, sizeof(K_SOCK_OPT_ENABLE));

        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_addr = in6addr_loopback;
        addr.sin6_port = htons(K_PORT_LISTENER_IPV6);

        if (bind(serverFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
            close(serverFd);
            _exit(K_EXIT_ERR_BIND);
        }

        if (listen(serverFd, K_LISTEN_BACKLOG) < 0) {
            close(serverFd);
            _exit(K_EXIT_ERR_LISTEN);
        }

        sockaddr_in6 clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
        if (clientFd >= 0) {
            close(clientFd);
        }

        close(serverFd);
        _exit(K_EXIT_SUCCESS);
    }

    usleep(K_CHILD_STARTUP_WAIT_US);
    EXPECT_TRUE(NetworkEndpointUtil::CheckConnectivity("::1", K_PORT_LISTENER_IPV6));

    int status = 0;
    EXPECT_NE(waitpid(pid, &status, 0), -1);
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), K_EXIT_SUCCESS);
}

TEST_F(NetworkEndpointUtilTest, CheckConnectivity_refused_port)
{
    // Port 1 on loopback is almost certainly not listening and will be refused
    EXPECT_FALSE(NetworkEndpointUtil::CheckConnectivity("127.0.0.1", K_PORT_REFUSED));
}

class FindAvailablePortTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}

    void SetUp() override
    {
        (void)unsetenv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_START");
        (void)unsetenv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_END");
    }

    void TearDown() override
    {
        (void)unsetenv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_START");
        (void)unsetenv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_END");
    }
};

TEST_F(FindAvailablePortTest, FindsPort_ipv4)
{
    uint16_t port = 0;
    EXPECT_TRUE(NetworkEndpointUtil::FindAvailablePort(port, false));
    EXPECT_GT(port, K_PORT_ZERO);
}

TEST_F(FindAvailablePortTest, FindsPort_ipv6)
{
    uint16_t port = 0;
    EXPECT_TRUE(NetworkEndpointUtil::FindAvailablePort(port, true));
    EXPECT_GT(port, K_PORT_ZERO);
}

TEST_F(FindAvailablePortTest, RespectEnvRange)
{
    (void)setenv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_START", "49200", 1);
    (void)setenv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_END", "49250", 1);

    uint16_t port = 0;
    EXPECT_TRUE(NetworkEndpointUtil::FindAvailablePort(port, false));
    EXPECT_GE(port, K_PORT_ENV_RANGE_START);
    EXPECT_LE(port, K_PORT_ENV_RANGE_END);
}

TEST_F(FindAvailablePortTest, ReversedEnvRange_fallback_to_default)
{
    (void)setenv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_START", "60000", 1);
    (void)setenv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_END", "50000", 1);

    uint16_t port = 0;
    EXPECT_TRUE(NetworkEndpointUtil::FindAvailablePort(port, false));
    EXPECT_GE(port, K_PORT_DEFAULT_MIN);
}

TEST_F(FindAvailablePortTest, InvalidEnvValue_ignored)
{
    (void)setenv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_START", "notanumber", 1);

    uint16_t port = 0;
    EXPECT_TRUE(NetworkEndpointUtil::FindAvailablePort(port, false));
    EXPECT_GE(port, K_PORT_DEFAULT_MIN);
}

TEST_F(FindAvailablePortTest, EnvValueZero_ignored)
{
    (void)setenv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_START", "0", 1);

    uint16_t port = 0;
    EXPECT_TRUE(NetworkEndpointUtil::FindAvailablePort(port, false));
}

TEST_F(FindAvailablePortTest, EnvValueOverflow_ignored)
{
    (void)setenv("MEMFABRIC_HYBRID_CONFIG_STORE_PORT_START", "99999", 1);

    uint16_t port = 0;
    EXPECT_TRUE(NetworkEndpointUtil::FindAvailablePort(port, false));
}

TEST_F(NetworkEndpointUtilTest, GetLocalIpWithTarget_ipv4_loopback)
{
    std::string local;
    EXPECT_TRUE(NetworkEndpointUtil::GetLocalIpWithTarget("127.0.0.1", local));
    EXPECT_EQ(local, "127.0.0.1");
}

TEST_F(NetworkEndpointUtilTest, GetLocalIpWithTarget_ipv6_loopback)
{
    std::string local;
    EXPECT_TRUE(NetworkEndpointUtil::GetLocalIpWithTarget("::1", local));
    EXPECT_EQ(local, "::1");
}

TEST_F(NetworkEndpointUtilTest, GetLocalIpWithTarget_invalid_ip)
{
    std::string local;
    EXPECT_FALSE(NetworkEndpointUtil::GetLocalIpWithTarget("", local));
    EXPECT_FALSE(NetworkEndpointUtil::GetLocalIpWithTarget("not_an_ip", local));
}