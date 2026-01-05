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
#include <mockcpp/mockcpp.hpp>

#include "hybm_logger.h"
#include "hybm_data_op_factory.h"
#include "hybm_compose_data_op.h"

class DataOperatorMock : public ock::mf::DataOperator {
public:
    DataOperatorMock(std::string nm) : name{std::move(nm)} {}

    ock::mf::Result Initialize() noexcept override;
    void UnInitialize() noexcept override;
    ock::mf::Result DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                             const ock::mf::ExtOptions &options) noexcept override;
    ock::mf::Result BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                  const ock::mf::ExtOptions &options) noexcept override;
    ock::mf::Result DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                                  const ock::mf::ExtOptions &options) noexcept override;
    ock::mf::Result Wait(int32_t waitId) noexcept override;
    void Reset() noexcept;

    uint64_t initializeCount{0};
    uint64_t uninitializeCount{0};
    uint64_t dataCopyCount{0};
    uint64_t batchDataCopyCount{0};
    uint64_t dataCopyAsyncCount{0};
    uint64_t waitCount{0};
    ock::mf::Result initializeResult{ock::mf::BErrorCode::BM_OK};
    ock::mf::Result dataCopyResult{ock::mf::BErrorCode::BM_OK};
    ock::mf::Result batchDataCopyResult{ock::mf::BErrorCode::BM_OK};
    ock::mf::Result dataCopyAsyncResult{ock::mf::BErrorCode::BM_OK};
    ock::mf::Result waitResult{ock::mf::BErrorCode::BM_OK};

    const std::string name;
};

ock::mf::Result DataOperatorMock::Initialize() noexcept
{
    initializeCount++;
    return initializeResult;
}

void DataOperatorMock::UnInitialize() noexcept
{
    uninitializeCount++;
}

ock::mf::Result DataOperatorMock::DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                                           const ock::mf::ExtOptions &options) noexcept
{
    dataCopyCount++;
    return dataCopyResult;
}

ock::mf::Result DataOperatorMock::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                                const ock::mf::ExtOptions &options) noexcept
{
    batchDataCopyCount++;
    return batchDataCopyResult;
}

ock::mf::Result DataOperatorMock::DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                                                const ock::mf::ExtOptions &options) noexcept
{
    dataCopyAsyncCount++;
    return dataCopyAsyncResult;
}

ock::mf::Result DataOperatorMock::Wait(int32_t waitId) noexcept
{
    waitCount++;
    return waitResult;
}

void DataOperatorMock::Reset() noexcept
{
    initializeCount = 0;
    uninitializeCount = 0;
    dataCopyCount = 0;
    batchDataCopyCount = 0;
    dataCopyAsyncCount = 0;
    waitCount = 0;
    initializeResult = ock::mf::BErrorCode::BM_OK;
    dataCopyResult = ock::mf::BErrorCode::BM_OK;
    batchDataCopyResult = ock::mf::BErrorCode::BM_OK;
    dataCopyAsyncResult = ock::mf::BErrorCode::BM_OK;
    waitResult = ock::mf::BErrorCode::BM_OK;
}

class HybmComposeDataOpTest : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;

public:
    static std::shared_ptr<ock::mf::DataOperator> CreateSdmaDataOperator();
    static std::shared_ptr<ock::mf::DataOperator>
    CreateDevRdmaDataOperator(uint32_t rankId, const std::shared_ptr<ock::mf::transport::TransportManager> &tm);
    static std::shared_ptr<ock::mf::DataOperator>
    CreateHostRdmaDataOperator(uint32_t rankId, const std::shared_ptr<ock::mf::transport::TransportManager> &tm);

protected:
    uint32_t OpOr()
    {
        return 0U;
    }

    template<class E, class... Args>
    uint32_t OpOr(E head, Args... rest)
    {
        auto headU32 = static_cast<uint32_t>(head);
        auto left = OpOr(rest...);
        return headU32 | left;
    }

    static std::shared_ptr<DataOperatorMock> sdmaDataOpMock;
    static std::shared_ptr<DataOperatorMock> devRdmaDataOpMock;
    static std::shared_ptr<DataOperatorMock> hostRdmaDataOpMock;
};

std::shared_ptr<DataOperatorMock> HybmComposeDataOpTest::sdmaDataOpMock = std::make_shared<DataOperatorMock>("sdma");
std::shared_ptr<DataOperatorMock> HybmComposeDataOpTest::devRdmaDataOpMock =
    std::make_shared<DataOperatorMock>("dev_rdma");
std::shared_ptr<DataOperatorMock> HybmComposeDataOpTest::hostRdmaDataOpMock =
    std::make_shared<DataOperatorMock>("host_rdma");

void HybmComposeDataOpTest::SetUp()
{
    sdmaDataOpMock->Reset();
    devRdmaDataOpMock->Reset();
    hostRdmaDataOpMock->Reset();
}

void HybmComposeDataOpTest::TearDown()
{
    GlobalMockObject::verify();
    sdmaDataOpMock->Reset();
    devRdmaDataOpMock->Reset();
    hostRdmaDataOpMock->Reset();
}

std::shared_ptr<ock::mf::DataOperator> HybmComposeDataOpTest::CreateSdmaDataOperator()
{
    return sdmaDataOpMock;
}

std::shared_ptr<ock::mf::DataOperator>
HybmComposeDataOpTest::CreateDevRdmaDataOperator(uint32_t rankId,
                                                 const std::shared_ptr<ock::mf::transport::TransportManager> &tm)
{
    return devRdmaDataOpMock;
}

std::shared_ptr<ock::mf::DataOperator>
HybmComposeDataOpTest::CreateHostRdmaDataOperator(uint32_t rankId,
                                                  const std::shared_ptr<ock::mf::transport::TransportManager> &tm)
{
    return hostRdmaDataOpMock;
}

TEST_F(HybmComposeDataOpTest, initialize_with_bm_type_ai_core)
{
    hybm_options options;
    options.bmType = HYBM_TYPE_AI_CORE_INITIATE;
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).expects(never()).will(returnValue(nullptr));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).expects(never()).will(returnValue(nullptr));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).expects(never()).will(returnValue(nullptr));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
}

TEST_F(HybmComposeDataOpTest, initialize_with_sdma_only)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_SDMA;
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->initializeCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, initialize_with_dev_rdma_only)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_DEVICE_RDMA;
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, devRdmaDataOpMock->initializeCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, devRdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, initialize_with_host_rdma_only)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_HOST_RDMA;
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, hostRdmaDataOpMock->initializeCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, hostRdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, initialize_with_data_op_all)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    uint32_t type = static_cast<uint32_t>(HYBM_DOP_TYPE_SDMA);
    type |= static_cast<uint32_t>(HYBM_DOP_TYPE_DEVICE_RDMA);
    type |= static_cast<uint32_t>(HYBM_DOP_TYPE_HOST_RDMA);
    options.bmDataOpType = static_cast<hybm_data_op_type>(type);
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->initializeCount);
    ASSERT_EQ(1UL, devRdmaDataOpMock->initializeCount);
    ASSERT_EQ(1UL, hostRdmaDataOpMock->initializeCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, hostRdmaDataOpMock->uninitializeCount);
    ASSERT_EQ(1UL, devRdmaDataOpMock->uninitializeCount);
    ASSERT_EQ(1UL, hostRdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, initialize_with_data_op_all_sdma_failed)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    uint32_t type = static_cast<uint32_t>(HYBM_DOP_TYPE_SDMA);
    type |= static_cast<uint32_t>(HYBM_DOP_TYPE_DEVICE_RDMA);
    type |= static_cast<uint32_t>(HYBM_DOP_TYPE_HOST_RDMA);
    options.bmDataOpType = static_cast<hybm_data_op_type>(type);
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    sdmaDataOpMock->initializeResult = ock::mf::BErrorCode::BM_ERROR;
    auto ret = dataOp.Initialize();
    ASSERT_NE(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->initializeCount);
    ASSERT_EQ(0UL, devRdmaDataOpMock->initializeCount);
    ASSERT_EQ(0UL, hostRdmaDataOpMock->initializeCount);

    dataOp.UnInitialize();
    ASSERT_EQ(0UL, hostRdmaDataOpMock->uninitializeCount);
    ASSERT_EQ(0UL, devRdmaDataOpMock->uninitializeCount);
    ASSERT_EQ(0UL, hostRdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, initialize_with_data_op_all_dev_rdma_failed)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    uint32_t type = static_cast<uint32_t>(HYBM_DOP_TYPE_SDMA);
    type |= static_cast<uint32_t>(HYBM_DOP_TYPE_DEVICE_RDMA);
    type |= static_cast<uint32_t>(HYBM_DOP_TYPE_HOST_RDMA);
    options.bmDataOpType = static_cast<hybm_data_op_type>(type);
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    devRdmaDataOpMock->initializeResult = ock::mf::BErrorCode::BM_ERROR;
    auto ret = dataOp.Initialize();
    ASSERT_NE(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->initializeCount);
    ASSERT_EQ(1UL, devRdmaDataOpMock->initializeCount);
    ASSERT_EQ(0UL, hostRdmaDataOpMock->initializeCount);

    dataOp.UnInitialize();
    ASSERT_EQ(0UL, hostRdmaDataOpMock->uninitializeCount);
    ASSERT_EQ(0UL, devRdmaDataOpMock->uninitializeCount);
    ASSERT_EQ(0UL, hostRdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, initialize_with_data_op_all_host_rdma_failed)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    uint32_t type = static_cast<uint32_t>(HYBM_DOP_TYPE_SDMA);
    type |= static_cast<uint32_t>(HYBM_DOP_TYPE_DEVICE_RDMA);
    type |= static_cast<uint32_t>(HYBM_DOP_TYPE_HOST_RDMA);
    options.bmDataOpType = static_cast<hybm_data_op_type>(type);
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    hostRdmaDataOpMock->initializeResult = ock::mf::BErrorCode::BM_ERROR;
    auto ret = dataOp.Initialize();
    ASSERT_NE(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->initializeCount);
    ASSERT_EQ(1UL, devRdmaDataOpMock->initializeCount);
    ASSERT_EQ(1UL, hostRdmaDataOpMock->initializeCount);

    dataOp.UnInitialize();
    ASSERT_EQ(0UL, hostRdmaDataOpMock->uninitializeCount);
    ASSERT_EQ(0UL, devRdmaDataOpMock->uninitializeCount);
    ASSERT_EQ(0UL, hostRdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, data_copy_sdma_success)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_SDMA;
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(returnValue(OpOr(HYBM_DOP_TYPE_SDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->initializeCount);

    hybm_copy_params copyParams{};
    ock::mf::ExtOptions extOptions{};
    ret = dataOp.DataCopy(copyParams, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, extOptions);
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->dataCopyCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, data_copy_all_sdma_first_success)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType =
        static_cast<hybm_data_op_type>(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA));
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(
        returnValue(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);

    hybm_copy_params copyParams{};
    ock::mf::ExtOptions extOptions{};
    ret = dataOp.DataCopy(copyParams, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, extOptions);
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->dataCopyCount);
    ASSERT_EQ(0UL, devRdmaDataOpMock->dataCopyCount);
    ASSERT_EQ(0UL, hostRdmaDataOpMock->dataCopyCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, data_copy_all_sdma_failed_dev_rdma_success)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType =
        static_cast<hybm_data_op_type>(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA));
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(
        returnValue(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));
    sdmaDataOpMock->dataCopyResult = ock::mf::BErrorCode::BM_DL_FUNCTION_FAILED;

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);

    hybm_copy_params copyParams{};
    ock::mf::ExtOptions extOptions{};
    ret = dataOp.DataCopy(copyParams, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, extOptions);
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->dataCopyCount);
    ASSERT_EQ(1UL, devRdmaDataOpMock->dataCopyCount);
    ASSERT_EQ(0UL, hostRdmaDataOpMock->dataCopyCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, batch_data_copy_sdma_success)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_SDMA;
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(returnValue(OpOr(HYBM_DOP_TYPE_SDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->initializeCount);

    hybm_batch_copy_params copyParams{};
    ock::mf::ExtOptions extOptions{};
    ret = dataOp.BatchDataCopy(copyParams, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, extOptions);
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->batchDataCopyCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, batch_data_copy_all_sdma_first_success)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType =
        static_cast<hybm_data_op_type>(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA));
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(
        returnValue(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);

    hybm_batch_copy_params copyParams{};
    ock::mf::ExtOptions extOptions{};
    ret = dataOp.BatchDataCopy(copyParams, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, extOptions);
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->batchDataCopyCount);
    ASSERT_EQ(0UL, devRdmaDataOpMock->batchDataCopyCount);
    ASSERT_EQ(0UL, hostRdmaDataOpMock->batchDataCopyCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, batch_data_copy_all_sdma_failed_dev_rdma_success)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType =
        static_cast<hybm_data_op_type>(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA));
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(
        returnValue(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));
    sdmaDataOpMock->batchDataCopyResult = ock::mf::BErrorCode::BM_DL_FUNCTION_FAILED;

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);

    hybm_batch_copy_params copyParams{};
    ock::mf::ExtOptions extOptions{};
    ret = dataOp.BatchDataCopy(copyParams, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, extOptions);
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->batchDataCopyCount);
    ASSERT_EQ(1UL, devRdmaDataOpMock->batchDataCopyCount);
    ASSERT_EQ(0UL, hostRdmaDataOpMock->batchDataCopyCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, data_copy_async_sdma_success)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_SDMA;
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(returnValue(OpOr(HYBM_DOP_TYPE_SDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->initializeCount);

    hybm_copy_params copyParams{};
    ock::mf::ExtOptions extOptions{};
    ret = dataOp.DataCopyAsync(copyParams, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, extOptions);
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->dataCopyAsyncCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, data_copy_async_all_sdma_first_success)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType =
        static_cast<hybm_data_op_type>(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA));
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(
        returnValue(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);

    hybm_copy_params copyParams{};
    ock::mf::ExtOptions extOptions{};
    ret = dataOp.DataCopyAsync(copyParams, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, extOptions);
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->dataCopyAsyncCount);
    ASSERT_EQ(0UL, devRdmaDataOpMock->dataCopyAsyncCount);
    ASSERT_EQ(0UL, hostRdmaDataOpMock->dataCopyAsyncCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, data_copy_async_all_sdma_failed_dev_rdma_success)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType =
        static_cast<hybm_data_op_type>(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA));
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(
        returnValue(OpOr(HYBM_DOP_TYPE_SDMA, HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));
    sdmaDataOpMock->dataCopyAsyncResult = ock::mf::BErrorCode::BM_DL_FUNCTION_FAILED;

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);

    hybm_copy_params copyParams{};
    ock::mf::ExtOptions extOptions{};
    ret = dataOp.DataCopyAsync(copyParams, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, extOptions);
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->dataCopyAsyncCount);
    ASSERT_EQ(1UL, devRdmaDataOpMock->dataCopyAsyncCount);
    ASSERT_EQ(0UL, hostRdmaDataOpMock->dataCopyAsyncCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, dwait_sdma_success)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_SDMA;
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(returnValue(OpOr(HYBM_DOP_TYPE_SDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->initializeCount);

    ret = dataOp.Wait(0);
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->waitCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, dwait_sdma_failed)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType = HYBM_DOP_TYPE_SDMA;
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(returnValue(OpOr(HYBM_DOP_TYPE_SDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateSdmaDataOperator).stubs().will(invoke(CreateSdmaDataOperator));
    sdmaDataOpMock->waitResult = ock::mf::BErrorCode::BM_DL_FUNCTION_FAILED;

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->initializeCount);

    ret = dataOp.Wait(0);
    ASSERT_NE(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(1UL, sdmaDataOpMock->waitCount);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, sdmaDataOpMock->uninitializeCount);
}

TEST_F(HybmComposeDataOpTest, dwait_no_sdma_failed)
{
    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.bmDataOpType = static_cast<hybm_data_op_type>(OpOr(HYBM_DOP_TYPE_DEVICE_RDMA, HYBM_DOP_TYPE_HOST_RDMA));
    auto tag = std::make_shared<ock::mf::HybmEntityTagInfo>();
    tag->TagInfoInit(options);

    union {
        uint32_t (ock::mf::HybmEntityTagInfo::*getRank2RankOpType)(uint32_t rankId1, uint32_t rankId2);
        uint32_t (*mocker)(ock::mf::HybmEntityTagInfo *, uint32_t, uint32_t);
    } u;
    u.getRank2RankOpType = &ock::mf::HybmEntityTagInfo::GetRank2RankOpType;
    MOCKER(u.mocker).stubs().will(returnValue(OpOr(HYBM_DOP_TYPE_SDMA)));
    MOCKER(ock::mf::DataOperatorFactory::CreateDevRdmaDataOperator).stubs().will(invoke(CreateDevRdmaDataOperator));
    MOCKER(ock::mf::DataOperatorFactory::CreateHostRdmaDataOperator).stubs().will(invoke(CreateHostRdmaDataOperator));

    ock::mf::HostComposeDataOp dataOp(options, nullptr, tag);
    auto ret = dataOp.Initialize();
    ASSERT_EQ(ock::mf::BErrorCode::BM_OK, ret);
    ASSERT_EQ(0UL, sdmaDataOpMock->initializeCount);
    ASSERT_EQ(1UL, devRdmaDataOpMock->initializeCount);
    ASSERT_EQ(1UL, hostRdmaDataOpMock->initializeCount);

    ret = dataOp.Wait(0);
    ASSERT_NE(ock::mf::BErrorCode::BM_OK, ret);

    dataOp.UnInitialize();
    ASSERT_EQ(1UL, devRdmaDataOpMock->uninitializeCount);
    ASSERT_EQ(1UL, hostRdmaDataOpMock->uninitializeCount);
}
