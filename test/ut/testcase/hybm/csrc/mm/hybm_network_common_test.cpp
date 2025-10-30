/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <ifaddrs.h>

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "hybm.h"
#include "hybm_types.h"
#include "hybm_networks_common.h"

using namespace ock::mf;

class HybmNetworkCommonTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
    }
    static void TearDownTestSuite()
    {
    }
    void SetUp() override
    {
    }
    void TearDown() override
    {
        mockcpp::GlobalMockObject::reset();
    }
};

namespace {
struct ifaddrs mock_lo;
struct ifaddrs mock_eth0;
struct sockaddr_in lo_addr;
struct sockaddr_in lo_netmask;
struct sockaddr_in eth0_addr;
struct sockaddr_in eth0_netmask;
char lo_name[] = "lo";
char eth0_name[] = "eth0";

int getifaddrs_ok_stub(struct ifaddrs **ifap)
{
    // 1. 准备模拟的 ifaddrs 链表数据
    // 模拟两个接口：lo 和 eth0
    // 初始化 lo 回环接口信息
    mock_lo.ifa_next = &mock_eth0; // 链表下一个指向 eth0
    mock_lo.ifa_name = lo_name;
    mock_lo.ifa_flags = IFF_UP | IFF_LOOPBACK; // 状态和回环标志

    // 设置 lo 的地址 (127.0.0.1)
    lo_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &lo_addr.sin_addr);
    mock_lo.ifa_addr = reinterpret_cast<struct sockaddr *>(&lo_addr);

    // 设置 lo 的子网掩码 (255.0.0.0)
    lo_netmask.sin_family = AF_INET;
    inet_pton(AF_INET, "255.0.0.0", &lo_netmask.sin_addr);
    mock_lo.ifa_netmask = reinterpret_cast<struct sockaddr *>(&lo_netmask);
    mock_lo.ifa_ifu.ifu_broadaddr = nullptr; // 回环接口无广播地址
    mock_lo.ifa_data = nullptr;

    // 初始化 eth0 以太网接口信息
    mock_eth0.ifa_next = nullptr; // 链表结束
    mock_eth0.ifa_name = eth0_name;
    mock_eth0.ifa_flags = IFF_UP | IFF_BROADCAST; // 状态和广播标志

    // 设置 eth0 的地址 (192.168.1.100)
    eth0_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "192.168.1.100", &eth0_addr.sin_addr);   // ip in res
    mock_eth0.ifa_addr = reinterpret_cast<struct sockaddr *>(&eth0_addr);

    // 设置 eth0 的子网掩码 (255.255.255.0)
    eth0_netmask.sin_family = AF_INET;
    inet_pton(AF_INET, "255.255.255.0", &eth0_netmask.sin_addr);
    mock_eth0.ifa_netmask = reinterpret_cast<struct sockaddr *>(&eth0_netmask);

    // 设置 eth0 的广播地址 (192.168.1.255)
    struct sockaddr_in eth0_broadaddr;
    eth0_broadaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "192.168.1.255", &eth0_broadaddr.sin_addr);
    mock_eth0.ifa_ifu.ifu_broadaddr = reinterpret_cast<struct sockaddr *>(&eth0_broadaddr);
    mock_eth0.ifa_data = nullptr;

    *ifap = &mock_lo;
    return 0;
}

int getifaddrs_fail_stub(struct ifaddrs **ifap)
{
    errno = -1;
    return -1;
}

int getifaddrs_null_stub(struct ifaddrs **ifap)
{
    *ifap = nullptr;
    return 0;
}

int getifaddrs_ifa_addr_null_stub(struct ifaddrs **ifap)
{
    mock_lo.ifa_addr = nullptr;
    *ifap = &mock_lo;
    return 0;
}

void freeifaddrs_stub(struct ifaddrs *ifa)
{
    mock_lo.ifa_next = nullptr;
}
}

TEST_F(HybmNetworkCommonTest, NetworkGetIpAddresses_OK)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(getifaddrs_ok_stub));
    MOCKER(freeifaddrs).expects(once()).will(invoke(freeifaddrs_stub));

    std::vector<uint32_t> res = NetworkGetIpAddresses();
    ASSERT_EQ(res.size(), 1);
}

TEST_F(HybmNetworkCommonTest, NetworkGetIpAddresses_Fail)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(getifaddrs_fail_stub));

    std::vector<uint32_t> res = NetworkGetIpAddresses();
    ASSERT_EQ(res.size(), 0);
}

TEST_F(HybmNetworkCommonTest, NetworkGetIpAddresses_ifa_null)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(getifaddrs_null_stub));
    MOCKER(freeifaddrs).expects(once()).will(invoke(freeifaddrs_stub));

    std::vector<uint32_t> res = NetworkGetIpAddresses();
    ASSERT_EQ(res.size(), 0);
}

TEST_F(HybmNetworkCommonTest, NetworkGetIpAddresses_ifa_addr_null)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(getifaddrs_ifa_addr_null_stub));
    MOCKER(freeifaddrs).expects(once()).will(invoke(freeifaddrs_stub));

    std::vector<uint32_t> res = NetworkGetIpAddresses();
    ASSERT_EQ(res.size(), 0);
}