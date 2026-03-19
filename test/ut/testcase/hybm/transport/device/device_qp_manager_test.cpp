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

#include "hybm_types.h"

#define private public
#define protected public
#include "device/device_qp_manager.h"
#undef private
#undef protected

using namespace ock::mf;
using namespace ock::mf::transport;
using namespace ock::mf::transport::device;

namespace {
class DeviceQpManagerTest final : public DeviceQpManager {
public:
    DeviceQpManagerTest(uint32_t deviceId, uint32_t rankId, uint32_t rankCount, sockaddr_in devNet)
        : DeviceQpManager(deviceId, rankId, rankCount, devNet, HYBM_ROLE_PEER)
    {
    }

    int SetRemoteRankInfo(const std::unordered_map<uint32_t, ConnectRankInfo>&) noexcept override
    {
        return BM_OK;
    }
    int Startup(void*) noexcept override
    {
        return BM_OK;
    }
    void Shutdown() noexcept override
    {
    }
    UserQpInfo* GetQpHandleWithRankId(uint32_t) noexcept override
    {
        return nullptr;
    }
    void PutQpHandle(UserQpInfo*) const noexcept override
    {
    }
};
}  // namespace

TEST(DeviceQpManagerTest, Ip2NetSetsFamilyAddrAndPortZero)
{
    in_addr ip{};
    inet_aton("10.1.2.3", &ip);

    sockaddr_in net = Ip2Net(ip);
    EXPECT_EQ(net.sin_family, AF_INET);
    EXPECT_EQ(net.sin_addr.s_addr, ip.s_addr);
    EXPECT_EQ(net.sin_port, 0);
}

TEST(DeviceQpManagerTest, DefaultBaseImplementationsReturnOkOrTrivialValues)
{
    sockaddr_in devNet{};
    inet_aton("127.0.0.1", &devNet.sin_addr);
    devNet.sin_port = htons(12345);
    devNet.sin_family = AF_INET;

    DeviceQpManagerTest mgr(0, 0, 1, devNet);

    EXPECT_EQ(mgr.WaitingConnectionReady(), BM_OK);
    EXPECT_EQ(mgr.GetQpInfoAddress(), nullptr);

    EXPECT_EQ(mgr.RemoveRanks({0U, 1U}), BM_OK);
    EXPECT_TRUE(mgr.CheckQpReady({0U}));
}