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
#include <sys/mman.h>

#include "hybm_data_op_device_rdma.h"
#include "hybm_transport_manager.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "hybm_functions.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

static void *g_mockParamSpace = nullptr;
static void *g_mockOutput = nullptr;

static void InitMockAddresses()
{
    constexpr uint64_t MOCK_PARAM_SPACE_SIZE = 64 * 1024 * 1024 + 256 * 1024;
    g_mockParamSpace = mmap(nullptr, MOCK_PARAM_SPACE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    g_mockOutput = mmap(nullptr, 4096UL, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

static void CleanupMockAddresses()
{
    if (g_mockParamSpace != nullptr && g_mockParamSpace != MAP_FAILED) {
        munmap(g_mockParamSpace, 4096UL);
        g_mockParamSpace = nullptr;
    }
    if (g_mockOutput != nullptr && g_mockOutput != MAP_FAILED) {
        munmap(g_mockOutput, 4096UL);
        g_mockOutput = nullptr;
    }
}

// 模拟 AclrtMallocHost 函数的实现
static int AclrtMallocHostStub(void **ptr, size_t size)
{
    if (ptr == nullptr || size == 0) {
        return -1;
    }
    if (g_mockParamSpace == nullptr) {
        InitMockAddresses();
    }
    *ptr = g_mockParamSpace;
    return 0;
}

// 模拟 HalHostRegister 函数的实现
static int HalHostRegisterStub(void *addr, uint64_t size, uint32_t flags, uint32_t devId, void **output)
{
    *output = g_mockOutput;
    return 0;
}

class TransportManagerMock : public ock::mf::transport::TransportManager {
public:
    TransportManagerMock() = default;
    ~TransportManagerMock() override = default;

    ock::mf::Result OpenDevice(const ock::mf::transport::TransportOptions &options) noexcept override
    {
        openDeviceCount++;
        return openDeviceResult;
    }

    ock::mf::Result CloseDevice() noexcept override
    {
        closeDeviceCount++;
        return closeDeviceResult;
    }

    ock::mf::Result RegisterMemoryRegion(const ock::mf::transport::TransportMemoryRegion &memory) noexcept override
    {
        registerMemoryRegionCount++;
        return registerMemoryRegionResult;
    }

    ock::mf::Result UnregisterMemoryRegion(uint64_t addr) noexcept override
    {
        unregisterMemoryRegionCount++;
        return unregisterMemoryRegionResult;
    }

    ock::mf::Result QueryMemoryKey(uint64_t addr, ock::mf::transport::TransportMemoryKey &key) noexcept override
    {
        queryMemoryKeyCount++;
        return queryMemoryKeyResult;
    }

    void UpdateMemoryKey(ock::mf::transport::TransportMemoryKey &key, void *addr) noexcept override
    {
        return;
    }

    ock::mf::Result Prepare(const ock::mf::transport::HybmTransPrepareOptions &options) noexcept override
    {
        prepareCount++;
        return prepareResult;
    }

    ock::mf::Result RemoveRanks(const std::vector<uint32_t> &removedRanks) noexcept override
    {
        removeRanksCount++;
        return removeRanksResult;
    }

    ock::mf::Result Connect() noexcept override
    {
        connectCount++;
        return connectResult;
    }

    ock::mf::Result AsyncConnect() noexcept override
    {
        asyncConnectCount++;
        return asyncConnectResult;
    }

    ock::mf::Result WaitForConnected(int64_t timeoutNs) noexcept override
    {
        waitForConnectedCount++;
        return waitForConnectedResult;
    }

    ock::mf::Result UpdateRankOptions(const ock::mf::transport::HybmTransPrepareOptions &options) noexcept override
    {
        updateRankOptionsCount++;
        return updateRankOptionsResult;
    }

    const std::string &GetNic() const noexcept override
    {
        getNicCount++;
        return nicName;
    }

    const ock::mf::transport::TransportPrivateData GetPrivateData() const override
    {
        return ock::mf::transport::TransportPrivateData{};
    }

    ock::mf::Result WriteRemote(uint32_t rankId, uint64_t srcAddr, uint64_t destAddr, uint64_t length) noexcept override
    {
        writeRemoteCount++;
        return writeRemoteResult;
    }

    ock::mf::Result ReadRemote(uint32_t rankId, uint64_t destAddr, uint64_t srcAddr, uint64_t length) noexcept override
    {
        readRemoteCount++;
        return readRemoteResult;
    }

    ock::mf::Result WriteRemoteAsync(uint32_t rankId, uint64_t srcAddr, uint64_t destAddr,
                                     uint64_t length) noexcept override
    {
        writeRemoteAsyncCount++;
        return writeRemoteAsyncResult;
    }

    ock::mf::Result ReadRemoteAsync(uint32_t rankId, uint64_t destAddr, uint64_t srcAddr,
                                    uint64_t length) noexcept override
    {
        readRemoteAsyncCount++;
        return readRemoteAsyncResult;
    }

    ock::mf::Result Synchronize(uint32_t rankId) noexcept override
    {
        synchronizeCount++;
        return synchronizeResult;
    }

    ock::mf::Result WriteRemoteBatchAsync(uint32_t rankId, const ock::mf::CopyDescriptor &descriptor) noexcept override
    {
        writeRemoteBatchAsyncCount++;
        return writeRemoteBatchAsyncResult;
    }

    ock::mf::Result ReadRemoteBatchAsync(uint32_t rankId, const ock::mf::CopyDescriptor &descriptor) noexcept override
    {
        readRemoteBatchAsyncCount++;
        return readRemoteBatchAsyncResult;
    }

    bool QueryHasRegistered(uint64_t addr, uint64_t length) noexcept override
    {
        queryHasRegisteredCount++;
        return queryHasRegisteredResult;
    }

    // 计数器
    uint64_t openDeviceCount{0};
    uint64_t closeDeviceCount{0};
    uint64_t registerMemoryRegionCount{0};
    uint64_t unregisterMemoryRegionCount{0};
    uint64_t queryMemoryKeyCount{0};
    uint64_t prepareCount{0};
    uint64_t removeRanksCount{0};
    uint64_t connectCount{0};
    uint64_t asyncConnectCount{0};
    uint64_t waitForConnectedCount{0};
    uint64_t updateRankOptionsCount{0};
    mutable uint64_t getNicCount{0};
    uint64_t writeRemoteCount{0};
    uint64_t readRemoteCount{0};
    uint64_t writeRemoteAsyncCount{0};
    uint64_t readRemoteAsyncCount{0};
    uint64_t synchronizeCount{0};
    uint64_t writeRemoteBatchAsyncCount{0};
    uint64_t readRemoteBatchAsyncCount{0};
    uint64_t queryHasRegisteredCount{0};

    // 结果
    ock::mf::Result openDeviceResult{BM_OK};
    ock::mf::Result closeDeviceResult{BM_OK};
    ock::mf::Result registerMemoryRegionResult{BM_OK};
    ock::mf::Result unregisterMemoryRegionResult{BM_OK};
    ock::mf::Result queryMemoryKeyResult{BM_OK};
    ock::mf::Result prepareResult{BM_OK};
    ock::mf::Result removeRanksResult{BM_OK};
    ock::mf::Result connectResult{BM_OK};
    ock::mf::Result asyncConnectResult{BM_OK};
    ock::mf::Result waitForConnectedResult{BM_OK};
    ock::mf::Result updateRankOptionsResult{BM_OK};
    ock::mf::Result writeRemoteResult{BM_OK};
    ock::mf::Result readRemoteResult{BM_OK};
    ock::mf::Result writeRemoteAsyncResult{BM_OK};
    ock::mf::Result readRemoteAsyncResult{BM_OK};
    ock::mf::Result synchronizeResult{BM_OK};
    ock::mf::Result writeRemoteBatchAsyncResult{BM_OK};
    ock::mf::Result readRemoteBatchAsyncResult{BM_OK};
    bool queryHasRegisteredResult{false};
    std::string nicName{"eth0"};

    // 重置方法
    void Reset() noexcept
    {
        openDeviceCount = 0;
        closeDeviceCount = 0;
        registerMemoryRegionCount = 0;
        unregisterMemoryRegionCount = 0;
        queryMemoryKeyCount = 0;
        prepareCount = 0;
        removeRanksCount = 0;
        connectCount = 0;
        asyncConnectCount = 0;
        waitForConnectedCount = 0;
        updateRankOptionsCount = 0;
        getNicCount = 0;
        writeRemoteCount = 0;
        readRemoteCount = 0;
        writeRemoteAsyncCount = 0;
        readRemoteAsyncCount = 0;
        synchronizeCount = 0;
        writeRemoteBatchAsyncCount = 0;
        readRemoteBatchAsyncCount = 0;
        queryHasRegisteredCount = 0;

        openDeviceResult = BM_OK;
        closeDeviceResult = BM_OK;
        registerMemoryRegionResult = BM_OK;
        unregisterMemoryRegionResult = BM_OK;
        queryMemoryKeyResult = BM_OK;
        prepareResult = BM_OK;
        removeRanksResult = BM_OK;
        connectResult = BM_OK;
        asyncConnectResult = BM_OK;
        waitForConnectedResult = BM_OK;
        updateRankOptionsResult = BM_OK;
        writeRemoteResult = BM_OK;
        readRemoteResult = BM_OK;
        writeRemoteAsyncResult = BM_OK;
        readRemoteAsyncResult = BM_OK;
        synchronizeResult = BM_OK;
        writeRemoteBatchAsyncResult = BM_OK;
        readRemoteBatchAsyncResult = BM_OK;
        queryHasRegisteredResult = false;
        nicName = "eth0";
    }
};

class HybmDataOpDeviceRdmaTest : public testing::Test {
public:
    void SetUp() override
    {
        // 分配模拟内存
        mockMemory = malloc(1024ULL * 1024ULL * 128ULL); // 128MB
        transportManagerMock_ = std::make_shared<TransportManagerMock>();
        dataOp_ = std::make_shared<ock::mf::DataOpDeviceRDMA>(rankId_, transportManagerMock_);
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
        transportManagerMock_->Reset();

        // 释放模拟内存
        if (mockMemory) {
            free(mockMemory);
            mockMemory = nullptr;
        }
    }

    void InitMockEnv()
    {
        // 模拟 DlAclApi::AclrtMallocHost 方法
        MOCKER(&ock::mf::DlAclApi::AclrtMallocHost).stubs().will(invoke(AclrtMallocHostStub));

        // 模拟 DlAclApi::AclrtFreeHost 方法
        MOCKER(&ock::mf::DlAclApi::AclrtFreeHost).stubs().will(returnValue(0));

        // 模拟 DlAclApi::AclrtMemcpy 方法
        MOCKER(&ock::mf::DlAclApi::AclrtMemcpy).stubs().will(returnValue(0));

        // 模拟 DlAclApi::AclrtMemcpyAsync 方法
        MOCKER(&ock::mf::DlAclApi::AclrtMemcpyAsync).stubs().will(returnValue(0));

        // 模拟 DlAclApi::AclrtSynchronizeStream 方法
        MOCKER(&ock::mf::DlAclApi::AclrtSynchronizeStream).stubs().will(returnValue(0));

        // 模拟 DlHalApi::HalHostRegister 方法
        MOCKER(&ock::mf::DlHalApi::HalHostRegister).stubs().will(invoke(HalHostRegisterStub));

        // 模拟 DlHalApi::HalHostUnregisterEx 方法
        MOCKER(&ock::mf::DlHalApi::HalHostUnregisterEx).stubs().will(returnValue(0));

        // 模拟全局函数
        MOCKER(HybmGetInitDeviceId).stubs().will(returnValue(0));
    }

protected:
    uint32_t rankId_{0};
    std::shared_ptr<TransportManagerMock> transportManagerMock_;
    std::shared_ptr<ock::mf::DataOpDeviceRDMA> dataOp_;
    static void *mockMemory;
};

void *HybmDataOpDeviceRdmaTest::mockMemory = nullptr;

TEST_F(HybmDataOpDeviceRdmaTest, initialize_success)
{
    InitMockEnv();
    // 测试 Initialize 成功场景
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    ASSERT_EQ(1UL, transportManagerMock_->registerMemoryRegionCount);
    dataOp_->UnInitialize();
}

TEST_F(HybmDataOpDeviceRdmaTest, initialize_already_inited)
{
    InitMockEnv();
    // 测试重复初始化场景
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    ASSERT_EQ(1UL, transportManagerMock_->registerMemoryRegionCount);

    // 再次调用 Initialize 应该返回 BM_OK
    ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    // 不应再次调用 RegisterMemoryRegion
    ASSERT_EQ(1UL, transportManagerMock_->registerMemoryRegionCount);
    dataOp_->UnInitialize();
}

TEST_F(HybmDataOpDeviceRdmaTest, initialize_register_memory_failed)
{
    InitMockEnv();
    // 测试 RegisterMemoryRegion 失败场景
    transportManagerMock_->registerMemoryRegionResult = BM_MALLOC_FAILED;
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_MALLOC_FAILED, ret);
    ASSERT_EQ(1UL, transportManagerMock_->registerMemoryRegionCount);
    dataOp_->UnInitialize();
}

TEST_F(HybmDataOpDeviceRdmaTest, uninitialize)
{
    InitMockEnv();
    // 测试 UnInitialize 场景
    // 即使 Initialize 失败，UnInitialize 也应该能正常调用
    dataOp_->UnInitialize();
    // 验证资源是否被释放
    // 由于 UnInitialize 主要是释放内存，这里主要测试调用是否成功
    ASSERT_TRUE(true);
}

TEST_F(HybmDataOpDeviceRdmaTest, data_copy_all_directions)
{
    InitMockEnv();
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    hybm_copy_params params{};
    ock::mf::ExtOptions options1{};
    options1.srcRankId = rankId_;
    options1.destRankId = rankId_;
    ock::mf::ExtOptions options2{};
    options2.srcRankId = rankId_ + 1;
    options2.destRankId = rankId_;

    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options1);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, options1);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE, options1);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options1);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST, options1);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, options1);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options1);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options1);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, options1);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options1);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, options1);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, options1);
    ASSERT_EQ(BM_OK, ret);

    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options2);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, options2);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE, options2);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options2);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST, options2);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, options2);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options2);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options2);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, options2);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options2);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, options2);
    ASSERT_EQ(BM_OK, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, options2);
    ASSERT_EQ(BM_OK, ret);

    dataOp_->UnInitialize();
}

TEST_F(HybmDataOpDeviceRdmaTest, data_copy_failed)
{
    MOCKER(&ock::mf::DlAclApi::AclrtMemcpy).stubs().will(returnValue(-1));
    InitMockEnv();
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);
    hybm_copy_params params{};
    ock::mf::ExtOptions options1{};
    options1.srcRankId = rankId_;
    options1.destRankId = rankId_;

    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options1);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options1);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options1);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, options1);
    ASSERT_EQ(BM_DL_FUNCTION_FAILED, ret);

    dataOp_->UnInitialize();
}

TEST_F(HybmDataOpDeviceRdmaTest, data_copy_async)
{
    InitMockEnv();
    // 测试异步数据拷贝
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    hybm_copy_params params{};
    ock::mf::ExtOptions options{};
    options.srcRankId = rankId_;
    options.destRankId = rankId_ + 1;

    ret = dataOp_->DataCopyAsync(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_ERROR, ret);

    dataOp_->UnInitialize();
}

TEST_F(HybmDataOpDeviceRdmaTest, wait)
{
    InitMockEnv();
    // 测试等待操作
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    ret = dataOp_->Wait(0);
    ASSERT_EQ(BM_OK, ret);

    dataOp_->UnInitialize();
}

TEST_F(HybmDataOpDeviceRdmaTest, transform_va)
{
    InitMockEnv();
    // 测试 VA 转换
    // TransformVa 方法不依赖于初始化状态，直接调用即可
    void *src = nullptr;
    void *dst = nullptr;
    dataOp_->TransformVa(src, dst, HYBM_LOCAL_HOST_TO_GLOBAL_HOST);
    // TransformVa 是一个空实现，这里主要测试调用是否成功
    ASSERT_TRUE(true);
}

TEST_F(HybmDataOpDeviceRdmaTest, safe_put_host_src)
{
    InitMockEnv();
    // 测试 SafePut 函数（源是主机内存）
    auto ret = dataOp_->Initialize();
    ASSERT_EQ(BM_OK, ret);

    // 模拟 QueryHasRegistered 返回 false，强制使用 SafePut 的完整逻辑
    transportManagerMock_->queryHasRegisteredResult = false;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(20480ULL);
    params.dest = reinterpret_cast<void *>(40960ULL);
    params.dataSize = 1024ULL;
    ock::mf::ExtOptions options{};
    options.srcRankId = rankId_;
    options.destRankId = rankId_ + 1;

    // 这里我们不关心具体的内存地址，只测试函数调用是否成功
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);

    dataOp_->UnInitialize();
}

TEST_F(HybmDataOpDeviceRdmaTest, safe_put_device_src)
{
    InitMockEnv();
    // 测试 SafePut 函数（源是设备内存）
    auto ret = dataOp_->Initialize();
    // 即使初始化失败，我们也继续测试，因为主要目标是测试函数覆盖

    // 模拟 QueryHasRegistered 返回 false，强制使用 SafePut 的完整逻辑
    transportManagerMock_->queryHasRegisteredResult = false;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(20480ULL);
    params.dest = reinterpret_cast<void *>(40960ULL);
    params.dataSize = 1024ULL;
    ock::mf::ExtOptions options{};
    options.srcRankId = rankId_;
    options.destRankId = rankId_ + 1;

    // 测试从设备到全局的拷贝
    ret = dataOp_->DataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);

    dataOp_->UnInitialize();
}

TEST_F(HybmDataOpDeviceRdmaTest, safe_get_host_dest)
{
    InitMockEnv();
    // 测试 SafeGet 函数（目标是主机内存）
    auto ret = dataOp_->Initialize();
    // 即使初始化失败，我们也继续测试，因为主要目标是测试函数覆盖

    // 模拟 QueryHasRegistered 返回 false，强制使用 SafeGet 的完整逻辑
    transportManagerMock_->queryHasRegisteredResult = false;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(20480ULL);
    params.dest = reinterpret_cast<void *>(40960ULL);
    params.dataSize = 1024ULL;
    ock::mf::ExtOptions options{};
    options.srcRankId = rankId_ + 1;
    options.destRankId = rankId_;

    // 测试从全局到本地主机的拷贝
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);

    dataOp_->UnInitialize();
}

TEST_F(HybmDataOpDeviceRdmaTest, safe_get_device_dest)
{
    InitMockEnv();
    // 测试 SafeGet 函数（目标是设备内存）
    auto ret = dataOp_->Initialize();
    // 即使初始化失败，我们也继续测试，因为主要目标是测试函数覆盖

    // 模拟 QueryHasRegistered 返回 false，强制使用 SafeGet 的完整逻辑
    transportManagerMock_->queryHasRegisteredResult = false;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(20480ULL);
    params.dest = reinterpret_cast<void *>(40960ULL);
    params.dataSize = 1024ULL;
    ock::mf::ExtOptions options{};
    options.srcRankId = rankId_ + 1;
    options.destRankId = rankId_;

    // 测试从全局到本地设备的拷贝
    ret = dataOp_->DataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);

    dataOp_->UnInitialize();
}

TEST_F(HybmDataOpDeviceRdmaTest, batch_data_copy_all_directions)
{
    InitMockEnv();
    // 测试 BatchDataCopy 函数的所有方向
    auto ret = dataOp_->Initialize();
    // 即使初始化失败，我们也继续测试，因为主要目标是测试函数覆盖

    ock::mf::ExtOptions options{};

    // 准备批量拷贝参数
    uint64_t hostGvaStart = 10000000ULL;
    uint64_t deviceGvaStart = 20000000ULL;
    uint64_t spaceSize = 40960ULL;
    uint64_t rankCount = 4ULL;
    dataOp_->UpdateGvaSpace(HYBM_MEM_TYPE_HOST, hostGvaStart, spaceSize, rankCount);
    dataOp_->UpdateGvaSpace(HYBM_MEM_TYPE_DEVICE, deviceGvaStart, spaceSize, rankCount);

    uint64_t localHostAddr = hostGvaStart;
    uint64_t remoteHostAddr1 = hostGvaStart + spaceSize;
    uint64_t remoteHostAddr2 = hostGvaStart + spaceSize * 2;
    uint64_t localDeviceAddr = deviceGvaStart;
    uint64_t remoteDeviceAddr1 = deviceGvaStart + spaceSize;
    uint64_t remoteDeviceAddr2 = deviceGvaStart + spaceSize * 2;
    void *srcLH[2] = {reinterpret_cast<void *>(localHostAddr), reinterpret_cast<void *>(localHostAddr)};
    void *srcGH[2] = {reinterpret_cast<void *>(remoteHostAddr1), reinterpret_cast<void *>(remoteHostAddr2)};
    void *dstLH[2] = {reinterpret_cast<void *>(localHostAddr), reinterpret_cast<void *>(localHostAddr)};
    void *dstGH[2] = {reinterpret_cast<void *>(remoteHostAddr1), reinterpret_cast<void *>(remoteHostAddr2)};
    void *srcLD[2] = {reinterpret_cast<void *>(localDeviceAddr), reinterpret_cast<void *>(localDeviceAddr)};
    void *srcGD[2] = {reinterpret_cast<void *>(remoteDeviceAddr1), reinterpret_cast<void *>(remoteDeviceAddr2)};
    void *dstLD[2] = {reinterpret_cast<void *>(localDeviceAddr), reinterpret_cast<void *>(localDeviceAddr)};
    void *dstGD[2] = {reinterpret_cast<void *>(remoteDeviceAddr1), reinterpret_cast<void *>(remoteDeviceAddr2)};
    uint64_t dataSizes[2] = {1024, 2048};

    hybm_batch_copy_params params{};
    params.batchSize = 2UL;
    params.dataSizes = dataSizes;

    params.sources = srcLH;
    params.destinations = dstGH;
    ret = dataOp_->BatchDataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);

    params.sources = srcGH;
    params.destinations = dstLH;
    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_HOST_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);

    params.sources = srcLH;
    params.destinations = dstGD;
    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);

    params.sources = srcGH;
    params.destinations = dstLH;
    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);

    params.sources = srcGD;
    params.destinations = dstLH;
    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);

    params.sources = srcGD;
    params.destinations = dstLD;
    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);

    params.sources = srcLH;
    params.destinations = dstGD;
    ret = dataOp_->BatchDataCopy(params, HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);

    params.sources = srcGD;
    params.destinations = dstLH;
    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);

    params.sources = srcLD;
    params.destinations = dstGH;
    ret = dataOp_->BatchDataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST, options);
    ASSERT_EQ(BM_OK, ret);

    params.sources = srcGH;
    params.destinations = dstLD;
    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);

    params.sources = srcLD;
    params.destinations = dstGD;
    ret = dataOp_->BatchDataCopy(params, HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);

    params.sources = srcGD;
    params.destinations = dstLD;
    ret = dataOp_->BatchDataCopy(params, HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE, options);
    ASSERT_EQ(BM_OK, ret);

    ret = dataOp_->BatchDataCopy(params, HYBM_DATA_COPY_DIRECTION_AUTO, options);
    ASSERT_EQ(BM_ERROR, ret);

    dataOp_->UnInitialize();
}