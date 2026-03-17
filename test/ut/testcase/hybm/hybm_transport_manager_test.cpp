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
#include <gmock/gmock.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

#include "hybm_transport_manager.h"

using ock::mf::HybmEntityTagInfo;
using ock::mf::HybmEntityTagInfoPtr;
using ock::mf::transport::TransportManager;
using ock::mf::transport::TransportType;

TEST(HybmTransportManagerTest, Create_TagManagerNull_ReturnNull)
{
    auto mgr = TransportManager::Create(ock::mf::transport::TT_COMPOSE, nullptr);
    EXPECT_EQ(mgr, nullptr);
}

TEST(HybmTransportManagerTest, Create_InvalidType_ReturnNull)
{
    HybmEntityTagInfoPtr tagMgr = std::make_shared<HybmEntityTagInfo>();
    auto mgr = TransportManager::Create(static_cast<TransportType>(-1), tagMgr);
    EXPECT_EQ(mgr, nullptr);
}

TEST(HybmTransportManagerTest, Create_Compose_ReturnNotNull)
{
    HybmEntityTagInfoPtr tagMgr = std::make_shared<HybmEntityTagInfo>();
    auto mgr = TransportManager::Create(ock::mf::transport::TT_COMPOSE, tagMgr);
    EXPECT_NE(mgr, nullptr);
}

TEST(HybmTransportManagerTest, GetQpInfo_DefaultImpl_ReturnNull)
{
    HybmEntityTagInfoPtr tagMgr = std::make_shared<HybmEntityTagInfo>();
    auto mgr = TransportManager::Create(ock::mf::transport::TT_COMPOSE, tagMgr);
    ASSERT_NE(mgr, nullptr);
    EXPECT_EQ(mgr->GetQpInfo(), nullptr);
}

