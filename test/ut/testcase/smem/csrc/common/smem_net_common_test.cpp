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
#include "smem_common_includes.h"
#include "smem_types.h"

using namespace ock::mf;
using namespace ock::smem;

namespace {
    void smem_init_ifaddrs();
    void smem_init_ifaddrs_ipv6();
}

class SmemNetCommonTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
    }
    static void TearDownTestSuite()
    {
    }
    void SetUp() override
    {
        smem_init_ifaddrs();
    }
    void TearDown() override
    {
        mockcpp::GlobalMockObject::reset();
    }
};

namespace {
struct ifaddrs mock_lo;
struct sockaddr_in lo_addr;
struct sockaddr_in lo_netmask;
char lo_name[] = "lo";

void smem_init_ifaddrs()
{
    // 1. 准备模拟的 ifaddrs 链表数据
    mock_lo.ifa_next = nullptr;
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
}

int smem_getifaddrs_ok_stub(struct ifaddrs **ifap)
{
    *ifap = &mock_lo;
    return 0;
}

int smem_getifaddrs_fail_stub(struct ifaddrs **ifap)
{
    errno = -1;
    return -1;
}

int smem_getifaddrs_null_stub(struct ifaddrs **ifap)
{
    *ifap = nullptr;
    return 0;
}

int smem_getifaddrs_ifa_addr_null_stub(struct ifaddrs **ifap)
{
    mock_lo.ifa_addr = nullptr;
    *ifap = &mock_lo;
    return 0;
}

void smem_freeifaddrs_stub(struct ifaddrs *ifa)
{
}
}

TEST_F(SmemNetCommonTest, GetLocalIpWithTarget_OK)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(smem_getifaddrs_ok_stub));
    MOCKER(freeifaddrs).expects(once()).will(invoke(smem_freeifaddrs_stub));

    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("127.0.0.2", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(localOutIpStr, "127.0.0.1");
}

TEST_F(SmemNetCommonTest, GetLocalIpWithTarget_Fail)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(smem_getifaddrs_fail_stub));

    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("127.0.0.2", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, SM_ERROR);
}

TEST_F(SmemNetCommonTest, GetLocalIpWithTarget_ifa_null)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(smem_getifaddrs_null_stub));
    MOCKER(freeifaddrs).expects(once()).will(invoke(smem_freeifaddrs_stub));

    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("127.0.0.2", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, SM_ERROR);
}

TEST_F(SmemNetCommonTest, GetLocalIpWithTarget_ifa_addr_null)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(smem_getifaddrs_ifa_addr_null_stub));
    MOCKER(freeifaddrs).expects(once()).will(invoke(smem_freeifaddrs_stub));

    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("127.0.0.2", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, SM_ERROR);
}

TEST_F(SmemNetCommonTest, GetLocalIpWithTarget_mask_not_match)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(smem_getifaddrs_ok_stub));
    MOCKER(freeifaddrs).expects(once()).will(invoke(smem_freeifaddrs_stub));

    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("126.0.0.2", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, SM_ERROR);
}

TEST_F(SmemNetCommonTest, GetLocalIpWithTarget_Invalid_IP)
{
    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("127.0.0.", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, SM_INVALID_PARAM);
}

class SmemNetCommonIpv6Test : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
    }
    static void TearDownTestSuite()
    {
    }
    void SetUp() override
    {
        smem_init_ifaddrs_ipv6();
    }
    void TearDown() override
    {
        mockcpp::GlobalMockObject::reset();
    }
};

namespace {
struct ifaddrs mock_lo_v6;
struct sockaddr_in6 lo_addr_v6;
struct sockaddr_in6 lo_netmask_v6;
char lo_name_v6[] = "lov6";

void smem_init_ifaddrs_ipv6()
{
    // 1. 准备模拟的 ifaddrs 链表数据
    mock_lo_v6.ifa_next = nullptr;
    mock_lo_v6.ifa_name = lo_name_v6;
    mock_lo_v6.ifa_flags = IFF_UP | IFF_LOOPBACK; // 状态和回环标志

    // 设置 lo 的地址 (::1)
    lo_addr_v6.sin6_family = AF_INET6;
    inet_pton(AF_INET6, "::1", &lo_addr_v6.sin6_addr);
    mock_lo_v6.ifa_addr = reinterpret_cast<struct sockaddr *>(&lo_addr_v6);

    // 设置 lo 的子网掩码 (ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff)
    lo_netmask_v6.sin6_family = AF_INET6;
    inet_pton(AF_INET6, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", &lo_netmask_v6.sin6_addr);
    mock_lo_v6.ifa_netmask = reinterpret_cast<struct sockaddr *>(&lo_netmask_v6);
    mock_lo_v6.ifa_ifu.ifu_broadaddr = nullptr; // 回环接口无广播地址
    mock_lo_v6.ifa_data = nullptr;
}

int smem_getifaddrs_ok_ipv6_stub(struct ifaddrs **ifap)
{
    *ifap = &mock_lo_v6;
    return 0;
}

int smem_getifaddrs_fail_ipv6_stub(struct ifaddrs **ifap)
{
    errno = -1;
    return -1;
}

int smem_getifaddrs_null_ipv6_stub(struct ifaddrs **ifap)
{
    *ifap = nullptr;
    return 0;
}

int smem_getifaddrs_ifa_addr_null_ipv6_stub(struct ifaddrs **ifap)
{
    mock_lo_v6.ifa_addr = nullptr;
    *ifap = &mock_lo_v6;
    return 0;
}

void smem_freeifaddrs_ipv6_stub(struct ifaddrs *ifa)
{
}
}

TEST_F(SmemNetCommonIpv6Test, GetLocalIpWithTarget_OK)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(smem_getifaddrs_ok_ipv6_stub));
    MOCKER(freeifaddrs).expects(once()).will(invoke(smem_freeifaddrs_ipv6_stub));

    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("::1", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(localOutIpStr, "::1");
}

TEST_F(SmemNetCommonIpv6Test, GetLocalIpWithTarget_Fail)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(smem_getifaddrs_fail_ipv6_stub));

    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("::2", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, SM_ERROR);
}

TEST_F(SmemNetCommonIpv6Test, GetLocalIpWithTarget_ifa_null)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(smem_getifaddrs_null_ipv6_stub));
    MOCKER(freeifaddrs).expects(once()).will(invoke(smem_freeifaddrs_ipv6_stub));

    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("::2", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, SM_ERROR);
}

TEST_F(SmemNetCommonIpv6Test, GetLocalIpWithTarget_ifa_addr_null)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(smem_getifaddrs_ifa_addr_null_ipv6_stub));
    MOCKER(freeifaddrs).expects(once()).will(invoke(smem_freeifaddrs_ipv6_stub));

    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("::2", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, SM_ERROR);
}

TEST_F(SmemNetCommonIpv6Test, GetLocalIpWithTarget_mask_not_match)
{
    MOCKER(getifaddrs).expects(once()).will(invoke(smem_getifaddrs_ok_ipv6_stub));
    MOCKER(freeifaddrs).expects(once()).will(invoke(smem_freeifaddrs_ipv6_stub));

    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("126.0.0.2", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, SM_ERROR);
}

TEST_F(SmemNetCommonIpv6Test, GetLocalIpWithTarget_Invalid_IP)
{
    std::string localOutIpStr;
    mf_ip_addr localOutIpNum;
    auto ret = GetLocalIpWithTarget("::::0", localOutIpStr, localOutIpNum);
    ASSERT_EQ(ret, SM_INVALID_PARAM);
}