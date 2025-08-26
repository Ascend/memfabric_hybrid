/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>
#include "mockcpp/mockcpp.hpp"

#include "hybm.h"
#include "hybm_big_mem.h"
#include "hybm_data_op.h"
#include "hybm_types.h"
#include "dl_acl_api.h"
#include "hybm_entity_factory.h"

using namespace ock::mf;

namespace {
const uint64_t g_rankSize = 1;
const uint64_t g_localMemSize = 1024 * 1024 * 1024;
const hybm_options g_rdma_options1 = {HYBM_TYPE_HBM_HOST_INITIATE, HYBM_DOP_TYPE_ROCE, HYBM_SCOPE_CROSS_NODE,
                                      HYBM_RANK_TYPE_STATIC, g_rankSize, 0, 0, g_localMemSize, 0, true,
                                      HYBM_ROLE_PEER, "tcp://127.0.0.1:10002"};
const hybm_options g_rdma_options2 = {HYBM_TYPE_DRAM_HOST_INITIATE, HYBM_DOP_TYPE_ROCE, HYBM_SCOPE_CROSS_NODE,
                                      HYBM_RANK_TYPE_STATIC, g_rankSize, 0, 0, g_localMemSize, 0, true,
                                      HYBM_ROLE_PEER, "tcp://127.0.0.1:10002"};
const uint64_t g_allocSize = 2 * 1024 * 1024;
const uint64_t g_copy2dSize = 128;
}

struct TestData {
    const hybm_options* options;
    std::string name;
};

class HybmDataOpEntryTest : public ::testing::TestWithParam<TestData> {
protected:
    static void SetUpTestSuite()
    {
        EXPECT_EQ(hybm_init(0, HYBM_LOAD_FLAG_NEED_HOST_RDMA | HYBM_LOAD_FLAG_NEED_DEVICE_RDMA), BM_OK);
        hybm_set_log_level(0);
    }

    static void TearDownTestSuite()
    {
        hybm_uninit();
    }

    void SetUp() override
    {
        const auto& param = GetParam();
        uint16_t id = 1;
        uint32_t flags = 0;

        entity = hybm_create_entity(id, param.options, flags);
        ASSERT_NE(entity, nullptr) << "Failed to create entity with options: " << param.name;

        EXPECT_EQ(hybm_reserve_mem_space(entity, 0, &reservedMem), BM_OK);
        slice = hybm_alloc_local_memory(entity, HYBM_MEM_TYPE_DEVICE, g_allocSize, 0);
        ASSERT_NE(slice, nullptr);

        hybm_exchange_info exInfo;
        bzero(&exInfo, sizeof(hybm_exchange_info));
        hybm_exchange_info entityInfo;
        bzero(&entityInfo, sizeof(hybm_exchange_info));
        EXPECT_EQ(hybm_export(entity, slice, 0, &exInfo), BM_OK);
        EXPECT_EQ(hybm_entity_export(entity, 0, &entityInfo), BM_OK);
        void* addresses[1] = { nullptr };
        hybm_exchange_info allInfo = exInfo;
        EXPECT_EQ(hybm_import(entity, &allInfo, 1, addresses, 0), BM_OK);
        EXPECT_EQ(hybm_mmap(entity, 0), BM_OK);
        allInfo = entityInfo;
        EXPECT_EQ(hybm_entity_import(entity, &allInfo, 1, 0), BM_OK);
    }

    void TearDown() override
    {
        if (entity) {
            hybm_unmap(entity, 0);
            hybm_remove_imported(entity, 0, 0);
            hybm_free_local_memory(entity, slice, g_allocSize, 0);
            hybm_unreserve_mem_space(entity, 0, reservedMem);
            hybm_destroy_entity(entity, 0);
        }
        GlobalMockObject::verify();
    }

    hybm_entity_t entity = nullptr;
    hybm_mem_slice_t slice = nullptr;
    void* reservedMem = nullptr;

    void DataCopyOk()
    {
        hybm_copy_params copy_params = {reservedMem, reservedMem, g_allocSize};
        auto ret = hybm_data_copy(entity, &copy_params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);
        ret = hybm_data_copy(entity, &copy_params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);
        ret = hybm_data_copy(entity, &copy_params, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);
        ret = hybm_data_copy(entity, &copy_params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);
        ret = hybm_data_copy(entity, &copy_params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);
    }

    void DataCopy2dOk()
    {
        hybm_copy_2d_params copy_2d_params = {reservedMem, g_copy2dSize, reservedMem,
                                              g_copy2dSize, g_copy2dSize, g_copy2dSize};
        auto ret = hybm_data_copy_2d(entity, &copy_2d_params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);
        ret = hybm_data_copy_2d(entity, &copy_2d_params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);
        ret = hybm_data_copy_2d(entity, &copy_2d_params, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);
        ret = hybm_data_copy_2d(entity, &copy_2d_params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);
        ret = hybm_data_copy_2d(entity, &copy_2d_params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_OK);
    }

    void DataCopyReturnInvalidParam()
    {
        hybm_copy_params copy_params1 = {nullptr, nullptr, 0};
        auto ret = hybm_data_copy(nullptr, &copy_params1, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        ret = hybm_data_copy(entity, &copy_params1, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        hybm_copy_params copy_params2 = {reservedMem, nullptr, 0};
        ret = hybm_data_copy(entity, &copy_params2, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        hybm_copy_params copy_params3 = {reservedMem, reservedMem, 0};
        ret = hybm_data_copy(entity, &copy_params3, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        hybm_copy_params copy_params4 = {reservedMem, reservedMem, g_allocSize};
        ret = hybm_data_copy(entity, &copy_params4, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);

        auto startAddr = reservedMem;
        auto endAddr = reservedMem + g_localMemSize * g_rankSize;
        hybm_copy_params copy_params5 = {startAddr - 1, startAddr - 1, g_allocSize};
        ret = hybm_data_copy(entity, &copy_params5, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        hybm_copy_params copy_params6 = {endAddr, endAddr, g_allocSize};
        ret = hybm_data_copy(entity, &copy_params6, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        ret = hybm_data_copy(entity, &copy_params6, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        ret = hybm_data_copy(entity, &copy_params5, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        ret = hybm_data_copy(entity, &copy_params6, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        ret = hybm_data_copy(entity, &copy_params6, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
    }

    void DataCopy2dReturnInvalidParam()
    {
        auto startAddr = reservedMem;
        auto endAddr = reservedMem + g_localMemSize * g_rankSize;

        hybm_copy_2d_params copy_2d_params1 = {nullptr, 0, nullptr, 0, 0, 0 };
        auto ret = hybm_data_copy_2d(nullptr, &copy_2d_params1, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        ret = hybm_data_copy_2d(entity, &copy_2d_params1, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        hybm_copy_2d_params copy_2d_params2 = {startAddr, 0, nullptr, 0, 0, 0 };
        ret = hybm_data_copy_2d(entity, &copy_2d_params2, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        hybm_copy_2d_params copy_2d_params3 = {startAddr, g_allocSize, nullptr, 0, 0, 0 };
        ret = hybm_data_copy_2d(entity, &copy_2d_params3, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        hybm_copy_2d_params copy_2d_params4 = {startAddr, g_allocSize, startAddr, 0, 0, 0 };
        ret = hybm_data_copy_2d(entity, &copy_2d_params4, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        hybm_copy_2d_params copy_2d_params5 = {startAddr, g_allocSize, startAddr, g_allocSize, 0, 0 };
        ret = hybm_data_copy_2d(entity, &copy_2d_params5, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        hybm_copy_2d_params copy_2d_params6 = {startAddr, g_allocSize, startAddr, g_allocSize, g_allocSize, 0 };
        ret = hybm_data_copy_2d(entity, &copy_2d_params6, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        hybm_copy_2d_params copy_2d_params7 = {startAddr, g_allocSize, startAddr, g_allocSize, g_allocSize, 1 };
        ret = hybm_data_copy_2d(entity, &copy_2d_params7, HYBM_DATA_COPY_DIRECTION_BUTT, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);

        hybm_copy_2d_params copy_2d_params8 = {startAddr - 1, g_allocSize, startAddr - 1, g_allocSize, g_allocSize, 1 };
        ret = hybm_data_copy_2d(entity, &copy_2d_params8, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        hybm_copy_2d_params copy_2d_params9 = {endAddr, g_allocSize, endAddr, g_allocSize, g_allocSize, 1 };
        ret = hybm_data_copy_2d(entity, &copy_2d_params9, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        ret = hybm_data_copy_2d(entity, &copy_2d_params9, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        ret = hybm_data_copy_2d(entity, &copy_2d_params8, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        ret = hybm_data_copy_2d(entity, &copy_2d_params9, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
        ret = hybm_data_copy_2d(entity, &copy_2d_params9, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, nullptr, 0);
        EXPECT_EQ(ret, BM_INVALID_PARAM);
    }
};

TEST_P(HybmDataOpEntryTest, hybm_data_copy_ShouldReturnOK)
{
    DataCopyOk();
}

TEST_P(HybmDataOpEntryTest, hybm_data_copy_2d_ShouldReturnOK)
{
    DataCopy2dOk();
}

TEST_P(HybmDataOpEntryTest, hybm_data_copy_ShouldReturnInvalidParam)
{
    DataCopyReturnInvalidParam();
}

TEST_P(HybmDataOpEntryTest, hybm_data_copy_2d_ShouldReturnInvalidParam)
{
    DataCopy2dReturnInvalidParam();
}

INSTANTIATE_TEST_SUITE_P(
    HybmOptions,
    HybmDataOpEntryTest,
    ::testing::Values(
        TestData{&g_rdma_options1, "RDMA_HBM"},
        TestData{&g_rdma_options2, "RDMA_DRAM"}
    ),
    [](const ::testing::TestParamInfo<TestData>& info) {
        return info.param.name;
    }
);