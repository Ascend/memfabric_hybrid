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

#include "hybm_types.h"
#include "device/device_rdma_helper.h"

using namespace ock::mf;
using namespace ock::mf::transport::device;

TEST(DeviceRdmaHelperTest, ParseDeviceNicPortRejectsInvalidAndAcceptsValid)
{
    uint16_t port = 0;

    // regex mismatch
    EXPECT_EQ(ParseDeviceNic("bad-nic", port), BM_INVALID_PARAM);

    // port invalid (0)
    EXPECT_EQ(ParseDeviceNic("tcp://127.0.0.1:0", port), BM_INVALID_PARAM);

    // port too large
    EXPECT_EQ(ParseDeviceNic("tcp://127.0.0.1:70000", port), BM_INVALID_PARAM);

    // ok
    EXPECT_EQ(ParseDeviceNic("tcp://127.0.0.1:12345", port), BM_OK);
    EXPECT_EQ(port, 12345);
}

TEST(DeviceRdmaHelperTest, GenerateDeviceNicFormatsTcpUrl)
{
    in_addr ip{};
    ASSERT_NE(inet_aton("10.1.2.3", &ip), 0);

    EXPECT_EQ(GenerateDeviceNic(ip, 2345), "tcp://10.1.2.3:2345");
}
