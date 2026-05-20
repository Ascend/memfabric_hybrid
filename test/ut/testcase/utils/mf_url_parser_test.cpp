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
#include <gtest/gtest.h>
#include <arpa/inet.h>

#include "mf_ipv4_validator.h"

using namespace ock::mf;
using ock::mf::NetValidator;

namespace {
constexpr uint16_t TEST_PORT_MAX = 65535;
constexpr uint16_t TEST_PORT_MIN = 1024;
}

class MFUrlParserTest : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    void SetUp() override {}

    void TearDown() override {}
};

// 测试IPv4地址解析 - 带协议前缀
TEST_F(MFUrlParserTest, InitializeWithTcpProtocol)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("tcp://192.168.1.1:8080"));
    EXPECT_EQ(parser.GetIp(), "192.168.1.1");
    EXPECT_EQ(parser.GetPort(), 8080);
    EXPECT_FALSE(parser.IsIpv6());
    EXPECT_EQ(parser.GetAddressFamily(), AF_INET);
    EXPECT_EQ(parser.GetProtocol(), "tcp://");
    EXPECT_TRUE(parser.IsInitialized());
}

TEST_F(MFUrlParserTest, InitializeWithHttpProtocol)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("http://10.0.0.1:9090"));
    EXPECT_EQ(parser.GetIp(), "10.0.0.1");
    EXPECT_EQ(parser.GetPort(), 9090);
    EXPECT_FALSE(parser.IsIpv6());
    EXPECT_EQ(parser.GetAddressFamily(), AF_INET);
    EXPECT_EQ(parser.GetProtocol(), "http://");
}

TEST_F(MFUrlParserTest, InitializeWithHttpsProtocol)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("https://172.16.0.1:443"));
    EXPECT_EQ(parser.GetIp(), "172.16.0.1");
    EXPECT_EQ(parser.GetPort(), 443);
    EXPECT_FALSE(parser.IsIpv6());
    EXPECT_EQ(parser.GetAddressFamily(), AF_INET);
    EXPECT_EQ(parser.GetProtocol(), "https://");
}

// 测试IPv4地址解析 - 不带协议前缀
TEST_F(MFUrlParserTest, InitializeWithoutProtocol)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("192.168.1.100:6000"));
    EXPECT_EQ(parser.GetIp(), "192.168.1.100");
    EXPECT_EQ(parser.GetPort(), 6000);
    EXPECT_FALSE(parser.IsIpv6());
    EXPECT_EQ(parser.GetAddressFamily(), AF_INET);
    EXPECT_EQ(parser.GetProtocol(), "");
}

// 测试IPv6地址解析 - 带方括号
TEST_F(MFUrlParserTest, InitializeIpv6WithBrackets)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("tcp://[::1]:8080"));
    EXPECT_EQ(parser.GetIp(), "::1");
    EXPECT_EQ(parser.GetPort(), 8080);
    EXPECT_TRUE(parser.IsIpv6());
    EXPECT_EQ(parser.GetAddressFamily(), AF_INET6);
    EXPECT_EQ(parser.GetProtocol(), "tcp://");
}

TEST_F(MFUrlParserTest, InitializeIpv6FullAddress)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("[2001:db8::1]:9090"));
    EXPECT_EQ(parser.GetIp(), "2001:db8::1");
    EXPECT_EQ(parser.GetPort(), 9090);
    EXPECT_TRUE(parser.IsIpv6());
    EXPECT_EQ(parser.GetAddressFamily(), AF_INET6);
}

// 测试边界端口值
TEST_F(MFUrlParserTest, InitializeWithMinPort)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("192.168.1.1:1"));
    EXPECT_EQ(parser.GetPort(), 1);
}

TEST_F(MFUrlParserTest, InitializeWithMaxPort)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("192.168.1.1:65535"));
    EXPECT_EQ(parser.GetPort(), 65535);
}

// 测试无效输入 - 空URL
TEST_F(MFUrlParserTest, InitializeWithEmptyUrl)
{
    UrlParser parser;
    EXPECT_FALSE(parser.Initialize(""));
    EXPECT_FALSE(parser.IsInitialized());
    EXPECT_EQ(parser.GetIp(), "");
    EXPECT_EQ(parser.GetPort(), 0);
}

// 测试无效输入 - 缺少端口
TEST_F(MFUrlParserTest, InitializeWithoutPort)
{
    UrlParser parser;
    EXPECT_FALSE(parser.Initialize("192.168.1.1"));
    EXPECT_FALSE(parser.IsInitialized());
}

// 测试无效输入 - 端口超出范围
TEST_F(MFUrlParserTest, InitializeWithInvalidPortZero)
{
    UrlParser parser;
    EXPECT_FALSE(parser.Initialize("192.168.1.1:0"));
    EXPECT_FALSE(parser.IsInitialized());
}

TEST_F(MFUrlParserTest, InitializeWithInvalidPortNegative)
{
    UrlParser parser;
    EXPECT_FALSE(parser.Initialize("192.168.1.1:-1"));
    EXPECT_FALSE(parser.IsInitialized());
}

TEST_F(MFUrlParserTest, InitializeWithInvalidPortTooLarge)
{
    UrlParser parser;
    EXPECT_FALSE(parser.Initialize("192.168.1.1:65536"));
    EXPECT_FALSE(parser.IsInitialized());
}

// 测试无效输入 - 端口不是数字
TEST_F(MFUrlParserTest, InitializeWithNonNumericPort)
{
    UrlParser parser;
    EXPECT_FALSE(parser.Initialize("192.168.1.1:abc"));
    EXPECT_FALSE(parser.IsInitialized());
}

// 测试无效输入 - 无效的IPv4地址
TEST_F(MFUrlParserTest, InitializeWithInvalidIpv4)
{
    UrlParser parser;
    EXPECT_FALSE(parser.Initialize("999.999.999.999:8080"));
    EXPECT_FALSE(parser.IsInitialized());
}

TEST_F(MFUrlParserTest, InitializeWithInvalidIpv4Format)
{
    UrlParser parser;
    EXPECT_FALSE(parser.Initialize("192.168.1:8080"));
    EXPECT_FALSE(parser.IsInitialized());
}

// 测试无效输入 - 无效的IPv6地址
TEST_F(MFUrlParserTest, InitializeWithInvalidIpv6)
{
    UrlParser parser;
    EXPECT_FALSE(parser.Initialize("[invalid:ipv6:address]:8080"));
    EXPECT_FALSE(parser.IsInitialized());
}

// 测试重复初始化
TEST_F(MFUrlParserTest, InitializeMultipleTimes)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("tcp://192.168.1.1:8080"));
    // 第二次初始化应该返回true，但不会改变已解析的值
    EXPECT_TRUE(parser.Initialize("tcp://10.0.0.1:9090"));
    EXPECT_EQ(parser.GetIp(), "192.168.1.1");
    EXPECT_EQ(parser.GetPort(), 8080);
}

// 测试GetSockAddr和GetAddrLen
TEST_F(MFUrlParserTest, GetSockAddrAndLen)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("tcp://192.168.1.1:8080"));

    const struct sockaddr *addr = parser.GetSockAddr();
    EXPECT_NE(addr, nullptr);
    EXPECT_EQ(parser.GetAddrLen(), sizeof(struct sockaddr_in));

    auto *addr4 = reinterpret_cast<const struct sockaddr_in *>(addr);
    EXPECT_EQ(addr4->sin_family, AF_INET);
    EXPECT_EQ(ntohs(addr4->sin_port), 8080);
}

TEST_F(MFUrlParserTest, GetSockAddrIpv6)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("tcp://[::1]:8080"));

    const struct sockaddr *addr = parser.GetSockAddr();
    EXPECT_NE(addr, nullptr);
    EXPECT_EQ(parser.GetAddrLen(), sizeof(struct sockaddr_in6));

    auto *addr6 = reinterpret_cast<const struct sockaddr_in6 *>(addr);
    EXPECT_EQ(addr6->sin6_family, AF_INET6);
    EXPECT_EQ(ntohs(addr6->sin6_port), 8080);
}

// 测试未初始化时获取属性
TEST_F(MFUrlParserTest, GetPropertiesBeforeInitialize)
{
    UrlParser parser;
    EXPECT_EQ(parser.GetIp(), "");
    EXPECT_EQ(parser.GetPort(), 0);
    EXPECT_EQ(parser.IsIpv6(), false);
    EXPECT_EQ(parser.GetAddressFamily(), 0);
    EXPECT_EQ(parser.GetSockAddr(), nullptr);
    EXPECT_EQ(parser.GetAddrLen(), 0);
    EXPECT_EQ(parser.GetProtocol(), "");
    EXPECT_FALSE(parser.IsInitialized());
}

// 测试GetPeerAddress - IPv4
TEST_F(MFUrlParserTest, GetPeerAddressIpv4)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("tcp://192.168.1.1:8080"));

    auto [addr, len] = parser.GetPeerAddress("192.168.1.2", 9090);
    EXPECT_NE(addr, nullptr);
    EXPECT_EQ(len, sizeof(struct sockaddr_in));

    auto *addr4 = reinterpret_cast<const struct sockaddr_in *>(addr);
    EXPECT_EQ(addr4->sin_family, AF_INET);
    EXPECT_EQ(ntohs(addr4->sin_port), 9090);

    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr4->sin_addr, ipStr, sizeof(ipStr));
    EXPECT_EQ(std::string(ipStr), "192.168.1.2");
}

// 测试GetPeerAddress - IPv6
TEST_F(MFUrlParserTest, GetPeerAddressIpv6)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("tcp://[::1]:8080"));

    auto [addr, len] = parser.GetPeerAddress("::2", 9090);
    EXPECT_NE(addr, nullptr);
    EXPECT_EQ(len, sizeof(struct sockaddr_in6));

    auto *addr6 = reinterpret_cast<const struct sockaddr_in6 *>(addr);
    EXPECT_EQ(addr6->sin6_family, AF_INET6);
    EXPECT_EQ(ntohs(addr6->sin6_port), 9090);

    char ipStr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr6->sin6_addr, ipStr, sizeof(ipStr));
    EXPECT_EQ(std::string(ipStr), "::2");
}

// 测试GetPeerAddress - 未初始化
TEST_F(MFUrlParserTest, GetPeerAddressNotInitialized)
{
    UrlParser parser;
    auto [addr, len] = parser.GetPeerAddress("192.168.1.2", 9090);
    EXPECT_EQ(addr, nullptr);
    EXPECT_EQ(len, 0);
}

// 测试GetPeerAddress - 无效的对端IP
TEST_F(MFUrlParserTest, GetPeerAddressInvalidIp)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("tcp://192.168.1.1:8080"));

    auto [addr, len] = parser.GetPeerAddress("invalid_ip", 9090);
    EXPECT_EQ(addr, nullptr);
    EXPECT_EQ(len, 0);
}

// 测试特殊IPv4地址
TEST_F(MFUrlParserTest, InitializeWithLocalhost)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("127.0.0.1:8080"));
    EXPECT_EQ(parser.GetIp(), "127.0.0.1");
    EXPECT_EQ(parser.GetPort(), 8080);
}

TEST_F(MFUrlParserTest, InitializeWithBroadcast)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("255.255.255.255:8080"));
    EXPECT_EQ(parser.GetIp(), "255.255.255.255");
    EXPECT_EQ(parser.GetPort(), 8080);
}

// 测试IPv6全零地址
TEST_F(MFUrlParserTest, InitializeIpv6ZeroAddress)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("[::]:8080"));
    EXPECT_EQ(parser.GetIp(), "::");
    EXPECT_EQ(parser.GetPort(), 8080);
    EXPECT_TRUE(parser.IsIpv6());
}

// 测试常见服务端口
TEST_F(MFUrlParserTest, InitializeWithCommonPorts)
{
    UrlParser parser1;
    EXPECT_TRUE(parser1.Initialize("tcp://192.168.1.1:80"));
    EXPECT_EQ(parser1.GetPort(), 80);

    UrlParser parser2;
    EXPECT_TRUE(parser2.Initialize("tcp://192.168.1.1:22"));
    EXPECT_EQ(parser2.GetPort(), 22);

    UrlParser parser3;
    EXPECT_TRUE(parser3.Initialize("tcp://192.168.1.1:3306"));
    EXPECT_EQ(parser3.GetPort(), 3306);
}

// ============================================================
// Tests for IsValidIpV4Strict - deterministic IPv4 validation
// ============================================================

TEST_F(MFUrlParserTest, IsValidIpV4Strict_Valid)
{
    EXPECT_TRUE(NetValidator::IsValidIpV4Strict("127.0.0.1"));
    EXPECT_TRUE(NetValidator::IsValidIpV4Strict("192.168.1.1"));
    EXPECT_TRUE(NetValidator::IsValidIpV4Strict("255.255.255.255"));
    EXPECT_TRUE(NetValidator::IsValidIpV4Strict("0.0.0.0"));
    EXPECT_TRUE(NetValidator::IsValidIpV4Strict("10.0.0.1"));
    EXPECT_TRUE(NetValidator::IsValidIpV4Strict("172.16.0.1"));
}

TEST_F(MFUrlParserTest, IsValidIpV4Strict_Invalid)
{
    // Out of range
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("999.1.1.1"));
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("256.0.0.0"));
    // Wrong segment count
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("1.2.3"));
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("1.2.3.4.5"));
    // Empty segment
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("1..3.4"));
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("1.2.3."));
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict(".1.2.3"));
    // Non-digit characters
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("abc"));
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("1.2.3.a"));
    // Empty string
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict(""));
    // Too long
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("1111.222.333.444"));
    // Leading zeros (should be rejected for strict validation)
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("01.0.0.1"));
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("192.168.01.1"));
}

// ============================================================
// Tests for IsZeroIpV4
// ============================================================

TEST_F(MFUrlParserTest, IsZeroIpV4_Valid)
{
    EXPECT_TRUE(NetValidator::IsZeroIpV4("0.0.0.0"));
    EXPECT_TRUE(NetValidator::IsZeroIpV4("00.00.00.00"));
    EXPECT_TRUE(NetValidator::IsZeroIpV4("000.000.000.000"));
}

TEST_F(MFUrlParserTest, IsZeroIpV4_Invalid)
{
    EXPECT_FALSE(NetValidator::IsZeroIpV4("127.0.0.1"));
    EXPECT_FALSE(NetValidator::IsZeroIpV4("0.0.0.1"));
    EXPECT_FALSE(NetValidator::IsZeroIpV4(""));
    EXPECT_FALSE(NetValidator::IsZeroIpV4("0.0.0"));
}

// ============================================================
// Tests for IsValidIpV4OrZero
// ============================================================

TEST_F(MFUrlParserTest, IsValidIpV4OrZero_Valid)
{
    EXPECT_TRUE(NetValidator::IsValidIpV4OrZero("127.0.0.1"));
    EXPECT_TRUE(NetValidator::IsValidIpV4OrZero("192.168.1.1"));
    EXPECT_TRUE(NetValidator::IsValidIpV4OrZero("0.0.0.0"));
    EXPECT_TRUE(NetValidator::IsValidIpV4OrZero("00.00.00.00"));
}

TEST_F(MFUrlParserTest, IsValidIpV4OrZero_Invalid)
{
    EXPECT_FALSE(NetValidator::IsValidIpV4OrZero("999.1.1.1"));
    EXPECT_FALSE(NetValidator::IsValidIpV4OrZero("1.2.3"));
    EXPECT_FALSE(NetValidator::IsValidIpV4OrZero(""));
    EXPECT_FALSE(NetValidator::IsValidIpV4OrZero("abc"));
}

// ============================================================
// Tests for IsValidUbcEid
// ============================================================

TEST_F(MFUrlParserTest, IsValidUbcEid_Valid)
{
    EXPECT_TRUE(NetValidator::IsValidUbcEid("0000:0000:0000:0000:0000:0000:0000:0001"));
    EXPECT_TRUE(NetValidator::IsValidUbcEid("ABCD:EF01:2345:6789:ABCD:EF01:2345:6789"));
    EXPECT_TRUE(NetValidator::IsValidUbcEid("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
    EXPECT_TRUE(NetValidator::IsValidUbcEid("aBcD:0000:0000:0000:0000:0000:0000:0001"));
}

TEST_F(MFUrlParserTest, IsValidUbcEid_Invalid)
{
    // Wrong segment count
    EXPECT_FALSE(NetValidator::IsValidUbcEid("0000:0000:0000:0000:0000:0000:0001"));
    EXPECT_FALSE(NetValidator::IsValidUbcEid("0000:0000:0000:0000:0000:0000:0000:0000:0000"));
    // Wrong segment length
    EXPECT_FALSE(NetValidator::IsValidUbcEid("000:0000:0000:0000:0000:0000:0000:0001"));
    EXPECT_FALSE(NetValidator::IsValidUbcEid("00000:0000:0000:0000:0000:0000:0000:0001"));
    // Invalid hex chars
    EXPECT_FALSE(NetValidator::IsValidUbcEid("GGGG:0000:0000:0000:0000:0000:0000:0001"));
    EXPECT_FALSE(NetValidator::IsValidUbcEid(""));
}

// ============================================================
// Tests for IsValidTag
// ============================================================

TEST_F(MFUrlParserTest, IsValidTag_Valid)
{
    EXPECT_TRUE(NetValidator::IsValidTag("tag"));
    EXPECT_TRUE(NetValidator::IsValidTag("rank_0"));
    EXPECT_TRUE(NetValidator::IsValidTag("a"));
    EXPECT_TRUE(NetValidator::IsValidTag(std::string(NetValidator::MAX_TAG_LEN, 'a')));
    EXPECT_TRUE(NetValidator::IsValidTag("ABC_def_123"));
}

TEST_F(MFUrlParserTest, IsValidTag_Invalid)
{
    // Empty
    EXPECT_FALSE(NetValidator::IsValidTag(""));
    // Too long (31 chars)
    EXPECT_FALSE(NetValidator::IsValidTag(std::string(NetValidator::MAX_TAG_LEN + 1, 'a')));
    // Invalid characters
    EXPECT_FALSE(NetValidator::IsValidTag("tag@0"));
    EXPECT_FALSE(NetValidator::IsValidTag("tag 0"));
    EXPECT_FALSE(NetValidator::IsValidTag("tag-0"));
    EXPECT_FALSE(NetValidator::IsValidTag("tag.0"));
}

// ============================================================
// Tests for ParseTagOpInfo
// ============================================================

TEST_F(MFUrlParserTest, ParseTagOpInfo_Valid)
{
    std::string tag1;
    std::string opType;
    std::string tag2;
    EXPECT_TRUE(NetValidator::ParseTagOpInfo("tag1:DEVICE_SDMA:tag2", tag1, opType, tag2));
    EXPECT_EQ(tag1, "tag1");
    EXPECT_EQ(opType, "DEVICE_SDMA");
    EXPECT_EQ(tag2, "tag2");

    EXPECT_TRUE(NetValidator::ParseTagOpInfo("a:DEVICE_RDMA:b", tag1, opType, tag2));
    EXPECT_EQ(tag1, "a");
    EXPECT_EQ(opType, "DEVICE_RDMA");
    EXPECT_EQ(tag2, "b");
}

TEST_F(MFUrlParserTest, ParseTagOpInfo_Invalid)
{
    std::string tag1;
    std::string opType;
    std::string tag2;
    // Wrong field count
    EXPECT_FALSE(NetValidator::ParseTagOpInfo("tag1:HOST_RDMA:tag2:extra", tag1, opType, tag2));
    EXPECT_FALSE(NetValidator::ParseTagOpInfo("tag1:HOST_RDMA", tag1, opType, tag2));
    EXPECT_FALSE(NetValidator::ParseTagOpInfo("", tag1, opType, tag2));
    // Invalid tag
    EXPECT_FALSE(NetValidator::ParseTagOpInfo("tag@1:DEVICE_SDMA:tag2", tag1, opType, tag2));
    // Invalid opType (too short)
    EXPECT_FALSE(NetValidator::ParseTagOpInfo("tag1:SHORT:tag2", tag1, opType, tag2));
    // Invalid opType (lowercase)
    EXPECT_FALSE(NetValidator::ParseTagOpInfo("tag1:device_rdma:tag2", tag1, opType, tag2));
}

// ============================================================
// Tests for IsValidPort
// ============================================================

TEST_F(MFUrlParserTest, IsValidPort_Valid)
{
    EXPECT_TRUE(NetValidator::IsValidPort("1", 1, TEST_PORT_MAX));
    EXPECT_TRUE(NetValidator::IsValidPort("1024", TEST_PORT_MIN, TEST_PORT_MAX));
    EXPECT_TRUE(NetValidator::IsValidPort("65535", 1, TEST_PORT_MAX));
    EXPECT_TRUE(NetValidator::IsValidPort("2048", 1, TEST_PORT_MAX));
}

TEST_F(MFUrlParserTest, IsValidPort_Invalid)
{
    // Empty
    EXPECT_FALSE(NetValidator::IsValidPort("", 1, TEST_PORT_MAX));
    // Non-digit
    EXPECT_FALSE(NetValidator::IsValidPort("abc", 1, TEST_PORT_MAX));
    EXPECT_FALSE(NetValidator::IsValidPort("12a4", 1, TEST_PORT_MAX));
    // Out of range
    EXPECT_FALSE(NetValidator::IsValidPort("0", 1, TEST_PORT_MAX));
    EXPECT_FALSE(NetValidator::IsValidPort("65536", 1, TEST_PORT_MAX));
    // Under min
    EXPECT_FALSE(NetValidator::IsValidPort("80", TEST_PORT_MIN, TEST_PORT_MAX));
    EXPECT_FALSE(NetValidator::IsValidPort("1023", TEST_PORT_MIN, TEST_PORT_MAX));
}

// ============================================================
// Tests for ParseNicUrl
// ============================================================

TEST_F(MFUrlParserTest, ParseNicUrl_Basic)
{
    std::string protocol;
    std::string ip;
    std::string mask;
    std::string port;
    EXPECT_TRUE(NetValidator::ParseNicUrl("tcp://127.0.0.1:2048", protocol, ip, mask, port));
    EXPECT_EQ(protocol, "tcp://");
    EXPECT_EQ(ip, "127.0.0.1");
    EXPECT_TRUE(mask.empty());
    EXPECT_EQ(port, "2048");
}

TEST_F(MFUrlParserTest, ParseNicUrl_WithMask)
{
    std::string protocol;
    std::string ip;
    std::string mask;
    std::string port;
    EXPECT_TRUE(NetValidator::ParseNicUrl("tcp://127.0.0.1/8:2048", protocol, ip, mask, port));
    EXPECT_EQ(protocol, "tcp://");
    EXPECT_EQ(ip, "127.0.0.1");
    EXPECT_EQ(mask, "8");
    EXPECT_EQ(port, "2048");
}

TEST_F(MFUrlParserTest, ParseNicUrl_Invalid)
{
    std::string protocol;
    std::string ip;
    std::string mask;
    std::string port;
    // Missing protocol
    EXPECT_FALSE(NetValidator::ParseNicUrl("127.0.0.1:2048", protocol, ip, mask, port));
    // Missing port
    EXPECT_FALSE(NetValidator::ParseNicUrl("tcp://127.0.0.1", protocol, ip, mask, port));
    // Empty
    EXPECT_FALSE(NetValidator::ParseNicUrl("", protocol, ip, mask, port));
    // No protocol prefix
    EXPECT_FALSE(NetValidator::ParseNicUrl("://127.0.0.1:2048", protocol, ip, mask, port));
}

// ============================================================
// Additional validation: no tests depend on std::regex
// ============================================================

TEST_F(MFUrlParserTest, NoRegexDependency)
{
    // Verify all helpers work without std::regex
    EXPECT_TRUE(NetValidator::IsValidIpV4Strict("10.0.0.1"));
    EXPECT_FALSE(NetValidator::IsValidIpV4Strict("invalid"));
    EXPECT_TRUE(NetValidator::IsValidUbcEid("0000:0000:0000:0000:0000:0000:0000:0001"));
    EXPECT_FALSE(NetValidator::IsValidUbcEid("bad:eid"));
    EXPECT_TRUE(NetValidator::IsValidTag("valid_tag_123"));
    EXPECT_FALSE(NetValidator::IsValidTag(""));
    EXPECT_TRUE(NetValidator::IsValidPort("8080", 1, TEST_PORT_MAX));
    EXPECT_FALSE(NetValidator::IsValidPort("0", 1, TEST_PORT_MAX));

    std::string p;
    std::string ip;
    std::string m;
    std::string port;
    EXPECT_TRUE(NetValidator::ParseNicUrl("tcp://1.2.3.4:5678", p, ip, m, port));
    EXPECT_EQ(port, "5678");
}
