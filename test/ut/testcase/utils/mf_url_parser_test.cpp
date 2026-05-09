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

// 测试边界值 - 端口为0
TEST_F(MFUrlParserTest, InitializeWithInvalidPortZero)
{
    UrlParser parser;
    EXPECT_TRUE(parser.Initialize("192.168.1.1:0"));
    EXPECT_TRUE(parser.IsInitialized());
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
