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

#include "host_hcom_helper.h"

using namespace ock::mf;
using namespace ock::mf::transport::host;

// AnalysisNic: 非法格式应返回 BM_INVALID_PARAM。
TEST(HostHcomHelperTest, AnalysisNicInvalidFormat)
{
    std::string proto;
    std::string ip;
    uint32_t port = 0;

    Result ret = HostHcomHelper::AnalysisNic("not_an_ip", proto, ip, port);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

// AnalysisNic: 端口越界时返回 BM_INVALID_PARAM。
TEST(HostHcomHelperTest, AnalysisNicInvalidPort)
{
    std::string proto;
    std::string ip;
    uint32_t port = 0;

    // 端口太小
    Result ret1 = HostHcomHelper::AnalysisNic("tcp://127.0.0.1:80", proto, ip, port);
    EXPECT_EQ(ret1, BM_INVALID_PARAM);

    // 端口太大
    Result ret2 = HostHcomHelper::AnalysisNic("tcp://127.0.0.1:70000", proto, ip, port);
    EXPECT_EQ(ret2, BM_INVALID_PARAM);
}

// AnalysisNic: IP 语法错误时返回 BM_INVALID_PARAM。
TEST(HostHcomHelperTest, AnalysisNicInvalidIp)
{
    std::string proto;
    std::string ip;
    uint32_t port = 0;

    Result ret = HostHcomHelper::AnalysisNic("tcp://999.999.999.999:2048", proto, ip, port);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

// AnalysisNic: UBC 协议前缀下，合法 EID 能被识别。
TEST(HostHcomHelperTest, AnalysisNicUbcPrefixValid)
{
    std::string proto;
    std::string ip;
    uint32_t port = 0;

    // 形如 8 段 4 位 16 进制的 EID。
    std::string nic = std::string(UBC_PROTOCOL_PREFIX) + "0000:0000:0000:0000:0000:0000:0000:0001";
    Result ret = HostHcomHelper::AnalysisNic(nic, proto, ip, port);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(proto, UBC_PROTOCOL_PREFIX);
    EXPECT_EQ(ip, "0000:0000:0000:0000:0000:0000:0000:0001");
    EXPECT_NE(port, 0U);
}

// HybmDopTransHcomProtocol: 根据 hybmDop 不同位返回不同的 Service_Type。
TEST(HostHcomHelperTest, HybmDopTransHcomProtocolBasic)
{
    // TCP 优先级最高。
    EXPECT_EQ(HostHcomHelper::HybmDopTransHcomProtocol(HYBM_DOP_TYPE_HOST_TCP, "tcp://127.0.0.1:2048"), C_SERVICE_TCP);

    // 只有 URMA 时返回 C_SERVICE_UBC。
    EXPECT_EQ(HostHcomHelper::HybmDopTransHcomProtocol(HYBM_DOP_TYPE_HOST_URMA, "ubc://xxxx"), C_SERVICE_UBC);

    // 只有 RDMA 时返回 C_SERVICE_RDMA。
    EXPECT_EQ(HostHcomHelper::HybmDopTransHcomProtocol(HYBM_DOP_TYPE_HOST_RDMA, "rdma://xxxx"), C_SERVICE_RDMA);

    // 同时开启 TCP + RDMA 时仍应优先 TCP。
    uint32_t dopTcpRdma = HYBM_DOP_TYPE_HOST_TCP | HYBM_DOP_TYPE_HOST_RDMA;
    EXPECT_EQ(HostHcomHelper::HybmDopTransHcomProtocol(dopTcpRdma, "tcp://127.0.0.1:2048"), C_SERVICE_TCP);
}

// AnalysisNic: 带掩码的有效格式应返回 BM_OK。
TEST(HostHcomHelperTest, AnalysisNicWithMaskValid)
{
    std::string proto;
    std::string ip;
    uint32_t port = 0;

    // 使用本地回环地址进行测试，这样更可靠
    Result ret = HostHcomHelper::AnalysisNic("tcp://127.0.0.1/8:2048", proto, ip, port);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(proto, "tcp://");
    EXPECT_NE(ip.empty(), true); // 应该找到本地 IP
    EXPECT_EQ(port, 2048U);
}

// AnalysisNic: 带掩码的无效格式应返回 BM_INVALID_PARAM。
TEST(HostHcomHelperTest, AnalysisNicWithMaskInvalidFormat)
{
    std::string proto;
    std::string ip;
    uint32_t port = 0;

    // 无效格式: 掩码格式错误
    Result ret1 = HostHcomHelper::AnalysisNic("tcp://192.168.1.1/abc:2048", proto, ip, port);
    EXPECT_EQ(ret1, BM_INVALID_PARAM);

    // 无效格式: 掩码范围错误
    Result ret2 = HostHcomHelper::AnalysisNic("tcp://192.168.1.1/33:2048", proto, ip, port);
    EXPECT_EQ(ret2, BM_INVALID_PARAM);

    // 无效格式: 掩码为负数
    Result ret3 = HostHcomHelper::AnalysisNic("tcp://192.168.1.1/-1:2048", proto, ip, port);
    EXPECT_EQ(ret3, BM_INVALID_PARAM);
}

// AnalysisNic: 带掩码的无效端口应返回 BM_INVALID_PARAM。
TEST(HostHcomHelperTest, AnalysisNicWithMaskInvalidPort)
{
    std::string proto;
    std::string ip;
    uint32_t port = 0;

    // 端口太小
    Result ret1 = HostHcomHelper::AnalysisNic("tcp://192.168.1.1/24:1023", proto, ip, port);
    EXPECT_EQ(ret1, BM_INVALID_PARAM);

    // 端口太大
    Result ret2 = HostHcomHelper::AnalysisNic("tcp://192.168.1.1/24:65536", proto, ip, port);
    EXPECT_EQ(ret2, BM_INVALID_PARAM);
}

// AnalysisNic: 带掩码的无效 IP 应返回 BM_INVALID_PARAM。
TEST(HostHcomHelperTest, AnalysisNicWithMaskInvalidIp)
{
    std::string proto;
    std::string ip;
    uint32_t port = 0;

    // 无效的 IP 地址
    Result ret = HostHcomHelper::AnalysisNic("tcp://999.999.999.999/24:2048", proto, ip, port);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}
