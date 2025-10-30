/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include <iostream>
#include "hybm.h"
#include "hybm_types.h"
#include "devmm_svm_gva.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "hybm_dp_device_rdma.h"

using namespace ock;
using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace {
    void *g_src = reinterpret_cast<void *>(0x000000000000ULL);
    void *g_dst = reinterpret_cast<void *>(0x000000010000ULL);
    const uint64_t g_size = 1024;
}
class HybmDataOpDevRdmaTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HybmDataOpDevRdmaTest, DataCopyOperation_ReturnError)
{
    DataOpDeviceRDMA entity(0,
        ock::mf::transport::TransportManager::Create(ock::mf::transport::TransportType::TT_HCCP));
    entity.Initialize();

    ExtOptions options{};
    options.srcRankId = 0;
    options.destRankId = 1;
    hybm_copy_params params = {g_src, nullptr, 1};
    auto ret = entity.DataCopy(params, HYBM_DATA_COPY_DIRECTION_BUTT, options);
    EXPECT_EQ(ret, BM_ERROR);

    options.srcRankId = 1;
    options.destRankId = 0;
    params = {nullptr, g_dst, 1};
    ret = entity.DataCopy(params, HYBM_DATA_COPY_DIRECTION_BUTT, options);
    EXPECT_EQ(ret, BM_ERROR);

    ret = entity.DataCopyAsync(params, HYBM_DATA_COPY_DIRECTION_BUTT, options);
    EXPECT_EQ(ret, BM_ERROR);

    ret = entity.Wait(2);
    EXPECT_EQ(ret, BM_ERROR);
    entity.UnInitialize();
}