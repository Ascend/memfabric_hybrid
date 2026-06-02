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
#include <cstring>

#define private   public
#define protected public
#include "hybm_entity_default.h"
#include "hybm_mem_segment.h"
#include "hybm_data_operator.h"
#include "hybm_va_manager.h"
#include "hybm_vmm_based_segment.h"
#include "dl_acl_api.h"
#undef private
#undef protected

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

namespace {
// Test constants for entity device IDs
constexpr int32_t TEST_DEVICE_ID_INIT = 100;
constexpr int32_t TEST_DEVICE_ID_RESERVE = 200;
constexpr int32_t TEST_DEVICE_ID_GET_PTR = 300;
constexpr int32_t TEST_DEVICE_ID_ALLOC = 400;
constexpr int32_t TEST_DEVICE_ID_REGISTER = 500;
constexpr int32_t TEST_DEVICE_ID_EXPORT = 600;
constexpr int32_t TEST_DEVICE_ID_IMPORT = 700;
constexpr int32_t TEST_DEVICE_ID_REMOVE = 900;
constexpr int32_t TEST_DEVICE_ID_CTX = 1000;
constexpr int32_t TEST_DEVICE_ID_MMAP = 1100;
constexpr int32_t TEST_DEVICE_ID_CHECK_ADDR = 1200;
constexpr int32_t TEST_DEVICE_ID_COPY = 1300;
constexpr int32_t TEST_DEVICE_ID_BATCH_COPY = 1400;
constexpr int32_t TEST_DEVICE_ID_WAIT = 1500;
constexpr int32_t TEST_DEVICE_ID_SDMA = 1600;
constexpr int32_t TEST_DEVICE_ID_DATA_OP = 1700;
constexpr int32_t TEST_DEVICE_ID_SLICE_VA = 1800;
constexpr int32_t TEST_DEVICE_ID_FUNC_MOD = 1900;
constexpr int32_t TEST_DEVICE_ID_TAG_MGR = 2000;
constexpr int32_t TEST_DEVICE_ID_TRANS_MGR_NO_TRANS = 2001;
constexpr int32_t TEST_DEVICE_ID_TRANS_MGR_PREPARE = 2002;
constexpr int32_t TEST_DEVICE_ID_TRANS_MGR_UPDATE = 2003;
constexpr int32_t TEST_DEVICE_ID_IMPORT_EXCHANGE = 2004;
constexpr int32_t TEST_DEVICE_ID_TRANS_PRECHECK = 2005;
constexpr int32_t TEST_DEVICE_ID_TRANS_PRECHECK_INVALID = 2006;
constexpr int32_t TEST_DEVICE_ID_TRANS_CONNECT = 2007;
constexpr int32_t TEST_DEVICE_ID_EXPORT_LONG_NIC = 2100;
constexpr int32_t TEST_DEVICE_ID_EXPORT_APPEND_FAIL = 2101;
constexpr int32_t TEST_DEVICE_ID_EXPORT_TRANS_NULL = 2102;
constexpr int32_t TEST_DEVICE_ID_CTX_NULL = 2300;
constexpr int32_t TEST_DEVICE_ID_CTX_SIZE_LARGE = 2301;
constexpr int32_t TEST_DEVICE_ID_CTX_MEMCPY_FAIL = 2302;
constexpr int32_t TEST_DEVICE_ID_CTX_SUCCESS = 2303;
constexpr int32_t TEST_DEVICE_ID_REMOVE_HBM_FAIL = 2400;
constexpr int32_t TEST_DEVICE_ID_REMOVE_DRAM_FAIL = 2401;
constexpr int32_t TEST_DEVICE_ID_REMOVE_CLEANUP = 2402;
constexpr int32_t TEST_DEVICE_ID_COPY_TRANS = 2500;
constexpr int32_t TEST_DEVICE_ID_COPY_FAIL = 2502;
constexpr int32_t TEST_DEVICE_ID_COPY_NONTRANS = 25021;
constexpr int32_t TEST_DEVICE_ID_COPY_VA_MGR = 25022;
constexpr int32_t TEST_DEVICE_ID_COPY_A5 = 25023;
constexpr int32_t TEST_DEVICE_ID_COPY_SECOND_MAP = 250231;
constexpr int32_t TEST_DEVICE_ID_COPY_NO_GVA = 25024;
constexpr int32_t TEST_DEVICE_ID_BATCH_NULL = 2503;
constexpr int32_t TEST_DEVICE_ID_BATCH_SUCCESS = 2504;
constexpr int32_t TEST_DEVICE_ID_BATCH_FAIL = 2505;
constexpr int32_t TEST_DEVICE_ID_QUANT_NULL = 2506;
constexpr int32_t TEST_DEVICE_ID_QUANT_FAIL = 2507;
constexpr int32_t TEST_DEVICE_ID_QUANT_SUCCESS = 2508;
constexpr int32_t TEST_DEVICE_ID_WAIT_OP = 2509;

// Test constants for memory sizes
constexpr uint64_t TEST_SMALL_SIZE = 1024;
constexpr uint64_t TEST_PAGE_SIZE = ock::mf::HYBM_LARGE_PAGE_SIZE;
constexpr uint64_t TEST_DATA_SIZE = 4096;
constexpr uint64_t TEST_DATA_SIZE_2 = 8192;

// Test constants for ranks
constexpr uint32_t TEST_RANK_0 = 0;
constexpr uint32_t TEST_RANK_1 = 1;
constexpr uint32_t TEST_RANK_2 = 2;
constexpr uint32_t TEST_RANK_3 = 3;
constexpr uint32_t TEST_RANK_6 = 6;
constexpr uint32_t TEST_RANK_8 = 8;
constexpr uint32_t TEST_RANK_9 = 9;
constexpr uint32_t TEST_RANK_11 = 11;
constexpr uint32_t TEST_RANK_12 = 12;
constexpr uint32_t TEST_RANK_13 = 13;
constexpr uint32_t TEST_RANK_COUNT_1 = 1;
constexpr uint32_t TEST_RANK_COUNT_2 = 2;
constexpr uint32_t TEST_RANK_COUNT_3 = 3;

// Test constants for addresses
constexpr uint64_t TEST_ADDR_SRC = 0x1111;
constexpr uint64_t TEST_ADDR_DST = 0x2222;
constexpr uint64_t TEST_ADDR_OFFSET_1 = 0x1000;
constexpr uint64_t TEST_ADDR_OFFSET_2 = 0x1800;
constexpr uint64_t TEST_ADDR_OFFSET_3 = 0x4000;
constexpr uint64_t TEST_ADDR_OFFSET_4 = 0x9000;
constexpr uint64_t TEST_ADDR_OFFSET_5 = 0x9800;

// Test constants for batch operations
constexpr uint32_t TEST_BATCH_SIZE_1 = 1;
constexpr uint32_t TEST_BATCH_SIZE_2 = 2;

// Test constants for context
constexpr int TEST_CONTEXT_VALUE = 123;
constexpr size_t TEST_CONTEXT_SIZE_1 = 1;

// Test constants for transport keys
constexpr uint64_t TEST_KEY_OFFSET = 0x1234;
constexpr uint32_t TEST_KEY_VALUE_1 = 0xABC;
constexpr uint32_t TEST_KEY_VALUE_2 = 0x11;
constexpr uint32_t TEST_KEY_VALUE_3 = 0x22;

// Test constants for invalid values
constexpr uint64_t TEST_INVALID_MAGIC = 0xDEADBEEF;

// Test constants for NIC length
constexpr size_t TEST_LONG_NIC_LENGTH = 64;
} // namespace

class HybmEntityDefaultTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}

    void SetUp() override
    {
        GlobalMockObject::reset();
        auto ret = hybm_init(0, 0);
        EXPECT_EQ(ret, BM_OK);
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
        hybm_uninit();
    }
};

namespace {
class FakeTransportManager : public ock::mf::transport::TransportManager {
public:
    ock::mf::transport::HybmTransPrepareOptions preparedOptions{};
    ock::mf::transport::HybmTransPrepareOptions updatedOptions{};
    ock::mf::transport::HybmTransPrepareOptions connectWithOptions{};
    int prepareCalled{0};
    int connectCalled{0};
    int updateCalled{0};
    int connectWithOptionsCalled{0};

    ock::mf::Result OpenDevice(const ock::mf::transport::TransportOptions & /* options */) override
    {
        return BM_OK;
    }
    ock::mf::Result CloseDevice() override
    {
        return BM_OK;
    }
    ock::mf::Result RegisterMemoryRegion(const ock::mf::transport::TransportMemoryRegion & /* mr */) override
    {
        return BM_OK;
    }
    ock::mf::Result UnregisterMemoryRegion(uint64_t /* addr */) override
    {
        return BM_OK;
    }
    bool QueryHasRegistered(uint64_t /* addr */, uint64_t /* size */) override
    {
        return false;
    }
    ock::mf::Result QueryMemoryKey(uint64_t /* addr */, ock::mf::transport::TransportMemoryKey &key) override
    {
        std::memset(&key, 0, sizeof(key));
        return BM_OK;
    }
    void UpdateMemoryKey(ock::mf::transport::TransportMemoryKey &key, void *addr) noexcept override
    {
        return;
    }

    ock::mf::Result Prepare(const ock::mf::transport::HybmTransPrepareOptions &options) override
    {
        prepareCalled++;
        preparedOptions = options;
        return BM_OK;
    }
    ock::mf::Result RemoveRanks(const std::vector<uint32_t> & /* removedRanks */) override
    {
        return BM_OK;
    }
    ock::mf::Result Connect() override
    {
        connectCalled++;
        return BM_OK;
    }
    ock::mf::Result AsyncConnect() override
    {
        return BM_OK;
    }
    ock::mf::Result WaitForConnected(int64_t /* timeoutNs */) override
    {
        return BM_OK;
    }
    ock::mf::Result UpdateRankOptions(const ock::mf::transport::HybmTransPrepareOptions &options) override
    {
        updateCalled++;
        updatedOptions = options;
        return BM_OK;
    }
    const std::string &GetNic() const override
    {
        return nic_;
    }
    const ock::mf::transport::TransportPrivateData GetPrivateData() const override
    {
        return ock::mf::transport::TransportPrivateData{};
    }
    ock::mf::Result ReadRemote(uint32_t /* rankId */, uint64_t /* lAddr */, uint64_t /* rAddr */,
                               uint64_t /* size */) override
    {
        return BM_OK;
    }
    ock::mf::Result WriteRemote(uint32_t /* rankId */, uint64_t /* lAddr */, uint64_t /* rAddr */,
                                uint64_t /* size */) override
    {
        return BM_OK;
    }
    ock::mf::Result ReadRemoteAsync(uint32_t /* rankId */, uint64_t /* lAddr */, uint64_t /* rAddr */,
                                    uint64_t /* size */) override
    {
        return BM_OK;
    }
    ock::mf::Result WriteRemoteAsync(uint32_t /* rankId */, uint64_t /* lAddr */, uint64_t /* rAddr */,
                                     uint64_t /* size */) override
    {
        return BM_OK;
    }
    ock::mf::Result Synchronize(uint32_t /* rankId */) override
    {
        return BM_OK;
    }
    ock::mf::Result WriteRemoteBatchAsync(uint32_t /* rankId */,
                                          const ock::mf::CopyDescriptor & /* descriptor */) override
    {
        return BM_OK;
    }
    ock::mf::Result ReadRemoteBatchAsync(uint32_t /* rankId */,
                                         const ock::mf::CopyDescriptor & /* descriptor */) override
    {
        return BM_OK;
    }

    ock::mf::Result ConnectWithOptions(const ock::mf::transport::HybmTransPrepareOptions &options) override
    {
        connectWithOptionsCalled++;
        connectWithOptions = options;
        return BM_OK;
    }

private:
    std::string nic_{"fake_nic0"};
};

class FakeTransportManagerLongNic : public FakeTransportManager {
public:
    const std::string &GetNic() const override
    {
        return longNic_;
    }

private:
    // ExportExchangeInfo checks: if (nic.size() >= sizeof(exportInfo.nic)) return BM_ERROR;
    std::string longNic_{std::string(TEST_LONG_NIC_LENGTH, 'x')};
};

class FakeTransportManagerRemoveFail : public FakeTransportManager {
public:
    ock::mf::Result RemoveRanks(const std::vector<uint32_t> & /* removedRanks */) override
    {
        removeCalled++;
        return BM_ERROR;
    }
    int removeCalled{0};
};

class FakeDataOperator : public ock::mf::DataOperator {
public:
    ock::mf::Result Initialize() noexcept override
    {
        return BM_OK;
    }
    void UnInitialize() noexcept override {}
    ock::mf::Result DataCopy(hybm_copy_params &, hybm_data_copy_direction,
                             const ock::mf::ExtOptions &options) noexcept override
    {
        lastDataCopyOptions = options;
        dataCopyCalled = true;
        return BM_OK;
    }
    ock::mf::Result BatchDataCopy(hybm_batch_copy_params &, hybm_data_copy_direction,
                                  const ock::mf::ExtOptions &options) noexcept override
    {
        lastBatchCopyOptions = options;
        batchDataCopyCalled = true;
        return BM_OK;
    }
    ock::mf::Result DataCopyAsync(hybm_copy_params &, hybm_data_copy_direction,
                                  const ock::mf::ExtOptions &) noexcept override
    {
        return BM_OK;
    }
    ock::mf::Result Wait(int32_t) noexcept override
    {
        return BM_OK;
    }
    void TransformVa(void *&, void *&, hybm_data_copy_direction) noexcept override {}
    void CleanUp() noexcept override
    {
        cleaned = true;
    }
    bool cleaned{false};
    bool dataCopyCalled{false};
    bool batchDataCopyCalled{false};
    ock::mf::ExtOptions lastDataCopyOptions{};
    ock::mf::ExtOptions lastBatchCopyOptions{};
};

class FakeDataOperatorWait : public FakeDataOperator {
public:
    ock::mf::Result Wait(int32_t waitId) noexcept override
    {
        waitCalled = true;
        lastWaitId = waitId;
        return waitRet;
    }
    bool waitCalled{false};
    int32_t lastWaitId{-1};
    ock::mf::Result waitRet{BM_OK};
};

class FakeDataOperatorQuant : public FakeDataOperator {
public:
    ock::mf::Result QuantCopy(hybm_quant_copy_params & /* params */) noexcept override
    {
        quantCalled = true;
        return quantRet;
    }
    bool quantCalled{false};
    ock::mf::Result quantRet{BM_OK};
};
} // namespace

// 测试 MemEntityDefault 初始化和反初始化
TEST_F(HybmEntityDefaultTest, Initialize_UnInitialize)
{
    ock::mf::MemEntityDefault entity(TEST_DEVICE_ID_INIT);
    hybm_options options{};
    options.rankId = TEST_RANK_0;
    options.rankCount = TEST_RANK_COUNT_1;

    // 模拟 MemSegment::Create 返回空指针
    union {
        ock::mf::MemSegmentPtr (*func)(const ock::mf::MemSegmentOptions &, int);
    } u{};
    u.func = &ock::mf::MemSegment::Create;
    MOCKER(u.func).stubs().will(returnValue(nullptr));

    // 测试初始化
    int32_t initRet = entity.Initialize(&options);
    EXPECT_EQ(initRet, BM_OK);

    hybm_mem_slice_t slice = nullptr;
    // 测试内存分配（已初始化的情况）size 非法
    auto allocRet = entity.AllocLocalMemory(TEST_SMALL_SIZE, HYBM_MEM_TYPE_HOST, 0, slice);
    EXPECT_EQ(allocRet, BM_INVALID_PARAM);
    EXPECT_EQ(slice, nullptr);

    // 测试内存分配（已初始化的情况）dramSegment_ 为空
    entity.dramSegment_ = nullptr;
    allocRet = entity.AllocLocalMemory(TEST_PAGE_SIZE, HYBM_MEM_TYPE_HOST, 0, slice);
    EXPECT_EQ(allocRet, BM_INVALID_PARAM);
    EXPECT_EQ(slice, nullptr);

    // 测试反初始化
    entity.UnInitialize();
    EXPECT_FALSE(entity.initialized_);
}

// 测试 MemEntityDefault 内存预留和释放
TEST_F(HybmEntityDefaultTest, Reserve_UnReserve_MemorySpace)
{
    ock::mf::MemEntityDefault entity(TEST_DEVICE_ID_RESERVE);

    // 测试内存预留（未初始化的情况）
    int32_t reserveRet = entity.ReserveMemorySpace();
    EXPECT_EQ(reserveRet, BM_NOT_INITIALIZED);

    // 测试内存释放（未初始化的情况）
    auto ret = entity.UnReserveMemorySpace();
    EXPECT_EQ(ret, BM_OK);

    // 模拟 MemSegment::Create 返回空指针
    union {
        ock::mf::MemSegmentPtr (*func)(const ock::mf::MemSegmentOptions &, int);
    } u{};
    u.func = &ock::mf::MemSegment::Create;
    MOCKER(u.func).stubs().will(returnValue(nullptr));

    // 测试初始化
    hybm_options options{};
    options.rankId = TEST_RANK_0;
    options.rankCount = TEST_RANK_COUNT_1;
    options.memType = HYBM_MEM_TYPE_HOST;
    int32_t initRet = entity.Initialize(&options);
    EXPECT_EQ(initRet, BM_OK);

    reserveRet = entity.ReserveMemorySpace();
    EXPECT_EQ(reserveRet, BM_OK);

    ret = entity.UnReserveMemorySpace();
    EXPECT_EQ(ret, BM_OK);
}

// 测试 MemEntityDefault 获取预留内存指针
TEST_F(HybmEntityDefaultTest, GetReservedMemoryPtr)
{
    ock::mf::MemEntityDefault entity(TEST_DEVICE_ID_GET_PTR);

    // 测试获取主机内存指针
    void *hostPtr = entity.GetReservedMemoryPtr(HYBM_MEM_TYPE_HOST);
    EXPECT_EQ(hostPtr, nullptr);

    // 测试获取设备内存指针
    void *devicePtr = entity.GetReservedMemoryPtr(HYBM_MEM_TYPE_DEVICE);
    EXPECT_EQ(devicePtr, nullptr);
}

// 测试 MemEntityDefault 内存分配和释放
TEST_F(HybmEntityDefaultTest, Alloc_Free_LocalMemory)
{
    ock::mf::MemEntityDefault entity(TEST_DEVICE_ID_ALLOC);
    hybm_mem_slice_t slice = nullptr;

    // 测试内存分配（未初始化的情况）
    int32_t allocRet = entity.AllocLocalMemory(TEST_SMALL_SIZE, HYBM_MEM_TYPE_HOST, 0, slice);
    EXPECT_EQ(allocRet, BM_NOT_INITIALIZED);
    EXPECT_EQ(slice, nullptr);

    // 测试内存释放（未初始化的情况）
    int32_t freeRet = entity.FreeLocalMemory(slice, 0);
    EXPECT_EQ(freeRet, BM_INVALID_PARAM);

    // 测试初始化
    // 模拟 MemSegment::Create 返回空指针
    ock::mf::MemSegmentOptions optionsSeg{};
    optionsSeg.segType = ock::mf::HYBM_MST_HBM;
    optionsSeg.maxSize = TEST_PAGE_SIZE;
    optionsSeg.rankCnt = TEST_RANK_COUNT_1;

    ock::mf::MemSegmentPtr segment = std::make_shared<ock::mf::HybmVmmBasedSegment>(optionsSeg, entity.id_);

    hybm_options options{};
    options.rankId = TEST_RANK_0;
    options.rankCount = TEST_RANK_COUNT_1;
    options.memType = HYBM_MEM_TYPE_DEVICE;
    options.maxDRAMSize = TEST_PAGE_SIZE;

    union {
        ock::mf::MemSegmentPtr (*func)(const ock::mf::MemSegmentOptions &, int);
    } u{};
    u.func = &ock::mf::MemSegment::Create;
    MOCKER(u.func).stubs().will(returnValue(segment));

    MOCKER_CPP(&ock::mf::MemSegment::InitDeviceInfo, int32_t (*)(ock::mf::MemEntityDefault *, int))
        .stubs()
        .will(returnValue(0));

    int32_t initRet = entity.Initialize(&options);
    EXPECT_EQ(initRet, BM_OK);

    allocRet = entity.AllocLocalMemory(TEST_SMALL_SIZE, HYBM_MEM_TYPE_HOST, 0, slice);
    EXPECT_EQ(allocRet, BM_INVALID_PARAM);

    allocRet = entity.AllocLocalMemory(TEST_PAGE_SIZE, HYBM_MEM_TYPE_DEVICE, 0, slice);
    EXPECT_EQ(allocRet, BM_INVALID_PARAM);

    freeRet = entity.FreeLocalMemory(slice, TEST_RANK_0);
    EXPECT_EQ(freeRet, BM_OK);
}

// 测试 MemEntityDefault 内存注册
TEST_F(HybmEntityDefaultTest, RegisterLocalMemory)
{
    ock::mf::MemEntityDefault entity(TEST_DEVICE_ID_REGISTER);

    // 测试内存注册（未初始化的情况）
    int buf = 0;
    hybm_mem_slice_t slice = nullptr;
    auto ret = entity.RegisterLocalMemory(&buf, sizeof(buf), 0, slice);
    EXPECT_EQ(slice, nullptr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
    ret = entity.RegisterLocalMemory(&buf, 0, 0, slice);
    EXPECT_EQ(slice, nullptr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    // 测试初始化
    ock::mf::MemSegmentOptions optionsSeg{};
    optionsSeg.segType = ock::mf::HYBM_MST_DRAM;
    optionsSeg.maxSize = TEST_PAGE_SIZE;
    optionsSeg.rankCnt = TEST_RANK_COUNT_2;
    ock::mf::MemSegmentPtr segment = std::make_shared<ock::mf::HybmVmmBasedSegment>(optionsSeg, entity.id_);

    MOCKER_CPP(&ock::mf::HybmVmmBasedSegment::ReserveMemorySpace, int32_t (*)(ock::mf::HybmVmmBasedSegment *, void **))
        .stubs()
        .will(returnValue(0));

    hybm_options options{};
    options.rankId = TEST_RANK_0;
    options.rankCount = TEST_RANK_COUNT_2;
    options.memType = HYBM_MEM_TYPE_HOST;
    entity.dramSegment_ = segment;

    int32_t initRet = entity.Initialize(&options);
    EXPECT_EQ(initRet, BM_OK);

    ret = entity.RegisterLocalMemory(&buf, sizeof(buf), 0, slice);
    EXPECT_EQ(ret, BM_OK);
}

// 测试 MemEntityDefault 导出交换信息
TEST_F(HybmEntityDefaultTest, ExportExchangeInfo)
{
    ock::mf::MemEntityDefault entity(TEST_DEVICE_ID_EXPORT);

    hybm_exchange_info hbmSliceInfo;
    bzero(&hbmSliceInfo, sizeof(hybm_exchange_info));
    ock::mf::ExchangeInfoWriter writer(&hbmSliceInfo);

    // 测试导出实体信息（未初始化的情况）
    int32_t exportRet = entity.ExportEntityExchangeInfo(writer, 0);
    EXPECT_EQ(exportRet, BM_NOT_INITIALIZED);

    // 测试导出切片信息（未初始化的情况）
    exportRet = entity.ExportSliceExchangeInfo(nullptr, writer, 0);
    EXPECT_EQ(exportRet, BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 导入交换信息
TEST_F(HybmEntityDefaultTest, ImportExchangeInfo)
{
    ock::mf::MemEntityDefault entity(TEST_DEVICE_ID_INIT);
    hybm_exchange_info info{};
    void *addresses[TEST_RANK_COUNT_1] = {nullptr};

    // 测试导入切片信息（未初始化的情况）
    ock::mf::ExchangeInfoReader readers;
    readers.Reset(&info);
    int32_t importRet = entity.ImportSliceExchangeInfo(&readers, TEST_RANK_COUNT_1, addresses, 0);
    EXPECT_EQ(importRet, BM_NOT_INITIALIZED);

    // 测试初始化
    hybm_options options{};
    options.rankId = TEST_RANK_0;
    options.rankCount = TEST_RANK_COUNT_1;
    int32_t initRet = entity.Initialize(&options);
    EXPECT_EQ(initRet, BM_OK);

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(0));

    importRet = entity.ImportSliceExchangeInfo(&readers, TEST_RANK_COUNT_1, addresses, 0);
    EXPECT_EQ(importRet, BM_OK);

    entity.UnInitialize();
    EXPECT_FALSE(entity.initialized_);
}

// 测试 MemEntityDefault 移除导入的内存
TEST_F(HybmEntityDefaultTest, RemoveImported)
{
    ock::mf::MemEntityDefault entity(TEST_DEVICE_ID_REMOVE);
    std::vector<uint32_t> ranks = {TEST_RANK_1, TEST_RANK_2, TEST_RANK_3};

    // 测试移除导入的内存（未初始化的情况）
    int32_t removeRet = entity.RemoveImported(ranks);
    EXPECT_EQ(removeRet, BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 设置额外上下文
TEST_F(HybmEntityDefaultTest, SetExtraContext)
{
    ock::mf::MemEntityDefault entity(TEST_DEVICE_ID_CTX);
    int ctx = TEST_CONTEXT_VALUE;

    // 测试设置额外上下文（未初始化的情况）
    int32_t setCtxRet = entity.SetExtraContext(&ctx, sizeof(ctx));
    EXPECT_EQ(setCtxRet, BM_NOT_INITIALIZED);
}

TEST_F(HybmEntityDefaultTest, SetExtraContext_ContextNull_ReturnInvalidParam)
{
    int32_t deviceId = TEST_DEVICE_ID_CTX_NULL;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.bmType = HYBM_TYPE_HOST_INITIATE;

    auto ret = entity.SetExtraContext(nullptr, TEST_CONTEXT_SIZE_1);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmEntityDefaultTest, SetExtraContext_SizeTooLarge_ReturnInvalidParam)
{
    int32_t deviceId = TEST_DEVICE_ID_CTX_SIZE_LARGE;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.bmType = HYBM_TYPE_HOST_INITIATE;

    int ctx = TEST_CONTEXT_VALUE;
    auto ret = entity.SetExtraContext(&ctx, ock::mf::HYBM_DEVICE_USER_CONTEXT_PRE_SIZE + 1);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmEntityDefaultTest, SetExtraContext_MemcpyFail_ReturnError)
{
    int32_t deviceId = TEST_DEVICE_ID_CTX_MEMCPY_FAIL;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.bmType = HYBM_TYPE_HOST_INITIATE;

    int ctx = TEST_CONTEXT_VALUE;
    MOCKER_CPP(&ock::mf::DlAclApi::AclrtMemcpy, int32_t (*)(void *, size_t, const void *, size_t, int32_t))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_ERROR)));

    auto ret = entity.SetExtraContext(&ctx, sizeof(ctx));
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, SetExtraContext_Success_ReturnOk)
{
    int32_t deviceId = TEST_DEVICE_ID_CTX_SUCCESS;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.bmType = HYBM_TYPE_HOST_INITIATE; // UpdateHybmDeviceInfo will return OK directly

    int ctx = TEST_CONTEXT_VALUE;
    MOCKER_CPP(&ock::mf::DlAclApi::AclrtMemcpy, int32_t (*)(void *, size_t, const void *, size_t, int32_t))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    auto ret = entity.SetExtraContext(&ctx, sizeof(ctx));
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, RemoveImported_HbmSegmentFail_ReturnError)
{
    int32_t deviceId = TEST_DEVICE_ID_REMOVE_HBM_FAIL;
    uint32_t rankCnt = TEST_RANK_COUNT_2;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;

    ock::mf::MemSegmentOptions optionsHbm{};
    optionsHbm.segType = ock::mf::HYBM_MST_HBM;
    optionsHbm.rankCnt = rankCnt;
    auto hbmSeg = std::make_shared<ock::mf::HybmVmmBasedSegment>(optionsHbm, entity.id_);
    entity.hbmSegment_ = hbmSeg;

    std::vector<uint32_t> ranks{TEST_RANK_1, TEST_RANK_2};
    MOCKER_CPP(&ock::mf::HybmVmmBasedSegment::RemoveImported,
               int32_t (*)(ock::mf::HybmVmmBasedSegment *, const std::vector<uint32_t> &))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_ERROR)));

    auto ret = entity.RemoveImported(ranks);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, RemoveImported_DramSegmentFail_ReturnError)
{
    int32_t deviceId = TEST_DEVICE_ID_REMOVE_DRAM_FAIL;
    uint32_t totalCnt = TEST_RANK_COUNT_2;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;

    ock::mf::MemSegmentOptions optionsDram{};
    optionsDram.segType = ock::mf::HYBM_MST_DRAM;
    optionsDram.rankCnt = totalCnt;
    auto dramSeg = std::make_shared<ock::mf::HybmVmmBasedSegment>(optionsDram, entity.id_);
    entity.dramSegment_ = dramSeg;

    std::vector<uint32_t> ranks{TEST_RANK_1, TEST_RANK_2};
    MOCKER_CPP(&ock::mf::HybmVmmBasedSegment::RemoveImported,
               int32_t (*)(ock::mf::HybmVmmBasedSegment *, const std::vector<uint32_t> &))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_ERROR)));

    auto ret = entity.RemoveImported(ranks);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, RemoveImported_CleanupAndEraseAndRemoveRanks)
{
    int32_t deviceId = TEST_DEVICE_ID_REMOVE_CLEANUP;
    uint32_t totalCnt = TEST_RANK_COUNT_3;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;

    // segments succeed
    ock::mf::MemSegmentOptions optionsHbm{};
    optionsHbm.segType = ock::mf::HYBM_MST_HBM;
    optionsHbm.rankCnt = totalCnt;
    auto hbmSeg = std::make_shared<ock::mf::HybmVmmBasedSegment>(optionsHbm, entity.id_);
    entity.hbmSegment_ = hbmSeg;
    MOCKER_CPP(&ock::mf::HybmVmmBasedSegment::RemoveImported,
               int32_t (*)(ock::mf::HybmVmmBasedSegment *, const std::vector<uint32_t> &))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    // data operator cleanup
    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;

    // transport remove ranks fails but should not fail RemoveImported
    auto trans = std::make_shared<FakeTransportManagerRemoveFail>();
    entity.transportManager_ = trans;

    // populate maps then erase
    ock::mf::EntityExportInfo r1{};
    r1.rankId = TEST_RANK_1;
    entity.importedRanks_[TEST_RANK_1] = r1;
    entity.importedMemories_[TEST_RANK_1] = {};
    ock::mf::EntityExportInfo rankSec{};
    uint32_t r2RankId = TEST_RANK_2;
    rankSec.rankId = r2RankId;
    entity.importedRanks_[r2RankId] = rankSec;
    entity.importedMemories_[r2RankId] = {};

    std::vector<uint32_t> ranks{TEST_RANK_1, TEST_RANK_2};
    auto ret = entity.RemoveImported(ranks);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(dop->cleaned);
    EXPECT_EQ(entity.importedRanks_.count(TEST_RANK_1), 0U);
    EXPECT_EQ(entity.importedRanks_.count(r2RankId), 0U);
    EXPECT_EQ(entity.importedMemories_.count(TEST_RANK_1), 0U);
    EXPECT_EQ(entity.importedMemories_.count(r2RankId), 0U);
    EXPECT_EQ(trans->removeCalled, 1);
}

TEST_F(HybmEntityDefaultTest, CopyData_TransScene_UsesLocateAddrAndRank)
{
    int32_t deviceId = TEST_DEVICE_ID_COPY_TRANS;
    uint32_t rankId = TEST_RANK_9;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = rankId;
    entity.options_.scene = HYBM_SCENE_TRANS;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(TEST_ADDR_SRC);
    params.dest = reinterpret_cast<void *>(TEST_ADDR_DST);
    uint64_t dataSize = TEST_DATA_SIZE;
    params.dataSize = dataSize;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, CopyData_DataCopyFail_ReturnErrorCode)
{
    int32_t deviceId = TEST_DEVICE_ID_COPY_FAIL;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = TEST_RANK_0;
    entity.options_.scene = HYBM_SCENE_TRANS;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    // Use FakeDataOperator but override DataCopy via mockcpp
    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;
    MOCKER_CPP(&FakeDataOperator::DataCopy, ock::mf::Result (*)(FakeDataOperator *, hybm_copy_params &,
                                                                hybm_data_copy_direction, const ock::mf::ExtOptions &))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_ERROR)));

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(TEST_ADDR_SRC);
    params.dest = reinterpret_cast<void *>(TEST_ADDR_DST);
    uint64_t dataSize = TEST_DATA_SIZE;
    params.dataSize = dataSize;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, CopyData_NonTransScene_UseLocalRankForAddrOutOfGvmRange)
{
    int32_t deviceId = TEST_DEVICE_ID_COPY_NONTRANS;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = TEST_RANK_6;
    entity.options_.scene = HYBM_SCENE_DEFAULT;
    entity.options_.enable56BitsGva = false;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));
    MOCKER_CPP(&ock::mf::DlAclApi::GetAscendSocType, ock::mf::AscendSocType (*)())
        .stubs()
        .will(returnValue(ock::mf::AscendSocType::ASCEND_910C));

    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(TEST_ADDR_SRC);
    params.dest = reinterpret_cast<void *>(TEST_ADDR_DST);
    params.dataSize = TEST_DATA_SIZE;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, CopyData_NonTransScene_UseRankFromVaManagerInSocRange)
{
    int32_t deviceId = TEST_DEVICE_ID_COPY_VA_MGR;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = TEST_RANK_3;
    entity.options_.scene = HYBM_SCENE_DEFAULT;
    entity.options_.enable56BitsGva = false;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));
    MOCKER_CPP(&ock::mf::DlAclApi::GetAscendSocType, ock::mf::AscendSocType (*)())
        .stubs()
        .will(returnValue(ock::mf::AscendSocType::ASCEND_910C));

    ock::mf::HybmVaManager::GetInstance().ClearAll();
    ASSERT_EQ(ock::mf::HybmVaManager::GetInstance().Initialize(ock::mf::AscendSocType::ASCEND_910C), BM_OK);
    ASSERT_EQ(
        ock::mf::HybmVaManager::GetInstance().AddVaInfoFromExternal(
            {{ock::mf::HYBM_GVM_START_ADDR + TEST_ADDR_OFFSET_1, 0, 0}, TEST_ADDR_OFFSET_3, HYBM_MEM_TYPE_HOST},
            0, TEST_RANK_11),
        BM_OK);
    ASSERT_EQ(
        ock::mf::HybmVaManager::GetInstance().AddVaInfoFromExternal(
            {{ock::mf::HYBM_GVM_START_ADDR + TEST_ADDR_OFFSET_4, 0, 0}, TEST_ADDR_OFFSET_3, HYBM_MEM_TYPE_HOST},
            0, TEST_RANK_12),
        BM_OK);

    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(ock::mf::HYBM_GVM_START_ADDR + TEST_ADDR_OFFSET_2);
    params.dest = reinterpret_cast<void *>(ock::mf::HYBM_GVM_START_ADDR + TEST_ADDR_OFFSET_5);
    params.dataSize = TEST_DATA_SIZE;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(dop->dataCopyCalled);
    EXPECT_EQ(dop->lastDataCopyOptions.srcRankId, TEST_RANK_11);
    EXPECT_EQ(dop->lastDataCopyOptions.destRankId, TEST_RANK_12);
    ock::mf::HybmVaManager::GetInstance().ClearAll();
}

TEST_F(HybmEntityDefaultTest, CopyData_NonTransScene_A5Soc_UseRankFromVaManagerInA5Range)
{
    int32_t deviceId = TEST_DEVICE_ID_COPY_A5;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = TEST_RANK_8;
    entity.options_.scene = HYBM_SCENE_DEFAULT;
    entity.options_.enable56BitsGva = false;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));
    MOCKER_CPP(&ock::mf::DlAclApi::GetAscendSocType, ock::mf::AscendSocType (*)())
        .stubs()
        .will(returnValue(ock::mf::AscendSocType::ASCEND_950));

    ock::mf::HybmVaManager::GetInstance().ClearAll();
    ASSERT_EQ(ock::mf::HybmVaManager::GetInstance().Initialize(ock::mf::AscendSocType::ASCEND_950), BM_OK);
    ASSERT_EQ(
        ock::mf::HybmVaManager::GetInstance().AddVaInfoFromExternal(
            {{ock::mf::HYBM_GVM_START_ADDR_A5 + TEST_ADDR_OFFSET_1, 0, 0}, TEST_ADDR_OFFSET_3, HYBM_MEM_TYPE_HOST}, 0,
            TEST_RANK_13),
        BM_OK);

    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(ock::mf::HYBM_GVM_START_ADDR_A5 + TEST_ADDR_OFFSET_2);
    params.dest = reinterpret_cast<void *>(TEST_ADDR_DST);
    params.dataSize = TEST_DATA_SIZE;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(dop->dataCopyCalled);
    EXPECT_EQ(dop->lastDataCopyOptions.srcRankId, TEST_RANK_13);
    EXPECT_EQ(dop->lastDataCopyOptions.destRankId, TEST_RANK_8);
    ock::mf::HybmVaManager::GetInstance().ClearAll();
}

TEST_F(HybmEntityDefaultTest, CopyData_NonTransScene_Enable56BitsGvaOutOfA5Range_UseLocalRank)
{
    int32_t deviceId = TEST_DEVICE_ID_COPY_SECOND_MAP;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = TEST_RANK_8;
    entity.options_.scene = HYBM_SCENE_DEFAULT;
    entity.options_.enable56BitsGva = true;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));
    MOCKER_CPP(&ock::mf::DlAclApi::GetAscendSocType, ock::mf::AscendSocType (*)())
        .stubs()
        .will(returnValue(ock::mf::AscendSocType::ASCEND_910C));

    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(ock::mf::HYBM_GVM_START_ADDR);
    params.dest = reinterpret_cast<void *>(TEST_ADDR_DST);
    params.dataSize = TEST_DATA_SIZE;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, CopyData_NonTransScene_Enable56BitsGvaAddrWithoutRegisteredGva_UseLocalRank)
{
    int32_t deviceId = TEST_DEVICE_ID_COPY_NO_GVA;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = TEST_RANK_8;
    entity.options_.scene = HYBM_SCENE_DEFAULT;
    entity.options_.enable56BitsGva = true;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));
    MOCKER_CPP(&ock::mf::DlAclApi::GetAscendSocType, ock::mf::AscendSocType (*)())
        .stubs()
        .will(returnValue(ock::mf::AscendSocType::ASCEND_910C));

    ock::mf::HybmVaManager::GetInstance().ClearAll();
    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(ock::mf::HYBM_56BITS_GVA_START_ADDR + 0x1000);
    params.dest = reinterpret_cast<void *>(0x2222);
    params.dataSize = 4096ULL;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_ERROR);
    ock::mf::HybmVaManager::GetInstance().ClearAll();
}

TEST_F(HybmEntityDefaultTest, BatchCopyData_DataOperatorNull_ReturnError)
{
    int32_t deviceId = TEST_DEVICE_ID_BATCH_NULL;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.scene = HYBM_SCENE_TRANS;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    hybm_batch_copy_params params{};
    params.batchSize = TEST_BATCH_SIZE_1;
    auto ret = entity.BatchCopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, BatchCopyData_GroupAndSuccess)
{
    int32_t deviceId = TEST_DEVICE_ID_BATCH_SUCCESS;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = TEST_RANK_0;
    entity.options_.scene = HYBM_SCENE_TRANS;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;
    void *srcs[2] = {reinterpret_cast<void *>(0x1), reinterpret_cast<void *>(0x2)};
    void *dsts[2] = {reinterpret_cast<void *>(0x3), reinterpret_cast<void *>(0x4)};
    uint64_t sizes[TEST_BATCH_SIZE_2] = {TEST_DATA_SIZE, TEST_DATA_SIZE_2};
    hybm_batch_copy_params params{};
    params.sources = srcs;
    params.destinations = dsts;
    params.dataSizes = sizes;
    uint32_t batchSize = TEST_BATCH_SIZE_2;
    params.batchSize = batchSize;

    auto ret = entity.BatchCopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, BatchCopyData_BatchCopyFail_ReturnErrorCode)
{
    int32_t deviceId = TEST_DEVICE_ID_BATCH_FAIL;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = TEST_RANK_0;
    entity.options_.scene = HYBM_SCENE_TRANS;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;
    MOCKER_CPP(&FakeDataOperator::BatchDataCopy,
               ock::mf::Result (*)(FakeDataOperator *, hybm_batch_copy_params &, hybm_data_copy_direction,
                                   const ock::mf::ExtOptions &))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_ERROR)));
    void *srcs[1] = {reinterpret_cast<void *>(0x1)};
    void *dsts[1] = {reinterpret_cast<void *>(0x2)};
    uint64_t sizes[TEST_BATCH_SIZE_1] = {TEST_DATA_SIZE};
    hybm_batch_copy_params params{};
    params.sources = srcs;
    params.destinations = dsts;
    params.dataSizes = sizes;
    params.batchSize = TEST_BATCH_SIZE_1;

    auto ret = entity.BatchCopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, QuantCopy_DataOperatorNull_ReturnError)
{
    int32_t deviceId = TEST_DEVICE_ID_QUANT_NULL;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    hybm_quant_copy_params params{};
    auto ret = entity.QuantCopy(params);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, QuantCopy_QuantFail_ReturnErrorCode)
{
    int32_t deviceId = TEST_DEVICE_ID_QUANT_FAIL;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    auto dop = std::make_shared<FakeDataOperatorQuant>();
    dop->quantRet = BM_ERROR;
    entity.dataOperator_ = dop;

    hybm_quant_copy_params params{};
    auto ret = entity.QuantCopy(params);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_TRUE(dop->quantCalled);
}

TEST_F(HybmEntityDefaultTest, QuantCopy_Success_ReturnOk)
{
    int32_t deviceId = TEST_DEVICE_ID_QUANT_SUCCESS;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    auto dop = std::make_shared<FakeDataOperatorQuant>();
    dop->quantRet = BM_OK;
    entity.dataOperator_ = dop;

    hybm_quant_copy_params params{};
    auto ret = entity.QuantCopy(params);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(dop->quantCalled);
}

TEST_F(HybmEntityDefaultTest, Wait_ReturnsDataOperatorWait)
{
    int32_t deviceId = TEST_DEVICE_ID_WAIT_OP;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;

    auto dop = std::make_shared<FakeDataOperatorWait>();
    dop->waitRet = BM_OK;
    entity.dataOperator_ = dop;

    auto ret = entity.Wait();
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(dop->waitCalled);
    EXPECT_EQ(dop->lastWaitId, 0);
}

// 测试 MemEntityDefault 内存映射和解除映射
TEST_F(HybmEntityDefaultTest, Mmap_Unmap)
{
    int32_t deviceId = TEST_DEVICE_ID_MMAP;
    ock::mf::MemEntityDefault entity(deviceId);

    // 测试内存映射（未初始化的情况）
    int32_t mmapRet = entity.Mmap();
    EXPECT_EQ(mmapRet, BM_NOT_INITIALIZED);

    // 测试内存解除映射（未初始化的情况）
    entity.Unmap();
}

// 测试 MemEntityDefault 地址检查
TEST_F(HybmEntityDefaultTest, CheckAddressInEntity)
{
    int32_t deviceId = TEST_DEVICE_ID_CHECK_ADDR;
    ock::mf::MemEntityDefault entity(deviceId);

    // 测试地址检查（未初始化的情况）
    int buf = 0;
    bool inEntity = entity.CheckAddressInEntity(&buf, sizeof(buf));
    EXPECT_FALSE(inEntity);
}

// 测试 MemEntityDefault 数据复制
TEST_F(HybmEntityDefaultTest, CopyData)
{
    int32_t deviceId = TEST_DEVICE_ID_COPY;
    ock::mf::MemEntityDefault entity(deviceId);
    hybm_copy_params params{};

    // 测试数据复制（未初始化的情况）
    int32_t copyRet = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(copyRet, BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 批量数据复制
TEST_F(HybmEntityDefaultTest, BatchCopyData)
{
    int32_t deviceId = TEST_DEVICE_ID_BATCH_COPY;
    ock::mf::MemEntityDefault entity(deviceId);
    hybm_batch_copy_params params{};

    // 测试批量数据复制（未初始化的情况）
    int32_t batchCopyRet = entity.BatchCopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(batchCopyRet, BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 等待操作
TEST_F(HybmEntityDefaultTest, Wait)
{
    int32_t deviceId = TEST_DEVICE_ID_WAIT;
    ock::mf::MemEntityDefault entity(deviceId);

    // 测试等待操作（未初始化的情况）
    int32_t waitRet = entity.Wait();
    EXPECT_EQ(waitRet, BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault SDMA 可达性检查
TEST_F(HybmEntityDefaultTest, SdmaReaches)
{
    int32_t deviceId = TEST_DEVICE_ID_SDMA;
    ock::mf::MemEntityDefault entity(deviceId);

    // 测试 SDMA 可达性检查（未初始化的情况）
    bool reaches = entity.SdmaReaches(TEST_RANK_1);
    EXPECT_FALSE(reaches);
}

// 测试 MemEntityDefault 数据操作类型检查
TEST_F(HybmEntityDefaultTest, CanReachDataOperators)
{
    int32_t deviceId = TEST_DEVICE_ID_DATA_OP;
    ock::mf::MemEntityDefault entity(deviceId);

    // 测试数据操作类型检查（未初始化的情况）
    hybm_data_op_type opType = entity.CanReachDataOperators(TEST_RANK_1);
    EXPECT_EQ(opType, HYBM_DOP_TYPE_DEFAULT);
}

// 测试 MemEntityDefault 获取切片虚拟地址
TEST_F(HybmEntityDefaultTest, GetSliceVa)
{
    int32_t deviceId = TEST_DEVICE_ID_SLICE_VA;
    ock::mf::MemEntityDefault entity(deviceId);

    // 测试获取切片虚拟地址（未初始化的情况）
    void *va = entity.GetSliceVa(nullptr);
    EXPECT_EQ(va, nullptr);
}

// 测试 MemEntityDefault 参数检查
TEST_F(HybmEntityDefaultTest, CheckOptions)
{
    // 测试空选项
    int32_t ret = ock::mf::MemEntityDefault::CheckOptions(nullptr);
    EXPECT_EQ(ret, BM_INVALID_PARAM);

    // 测试有效的基本选项
    hybm_options options{};
    options.rankId = TEST_RANK_0;
    options.rankCount = TEST_RANK_COUNT_1;
    ret = ock::mf::MemEntityDefault::CheckOptions(&options);
    EXPECT_EQ(ret, BM_OK);
}

// 测试 MemEntityDefault 功能修改拦截
TEST_F(HybmEntityDefaultTest, MemEntityDefault_FunctionModification_Intercept)
{
    int32_t deviceId = TEST_DEVICE_ID_FUNC_MOD;
    ock::mf::MemEntityDefault entity(deviceId);
    hybm_options options{};
    options.rankId = TEST_RANK_0;
    options.rankCount = TEST_RANK_COUNT_1;

    // 测试参数检查的一致性
    int32_t ret1 = ock::mf::MemEntityDefault::CheckOptions(&options);
    int32_t ret2 = ock::mf::MemEntityDefault::CheckOptions(&options);
    EXPECT_EQ(ret1, ret2);
    EXPECT_EQ(ret1, BM_OK);

    // 测试空参数检查的一致性
    int32_t nullRet1 = ock::mf::MemEntityDefault::CheckOptions(nullptr);
    int32_t nullRet2 = ock::mf::MemEntityDefault::CheckOptions(nullptr);
    EXPECT_EQ(nullRet1, nullRet2);
    EXPECT_EQ(nullRet1, BM_INVALID_PARAM);
}

TEST_F(HybmEntityDefaultTest, ImportForTagManager_Success)
{
    int32_t deviceId = TEST_DEVICE_ID_TAG_MGR;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.tagManager_ = std::make_shared<ock::mf::HybmEntityTagInfo>();

    ock::mf::EntityExportInfo r0{};
    r0.rankId = TEST_RANK_0;
    std::strncpy(r0.tag, "tag_0", sizeof(r0.tag) - 1);
    entity.importedRanks_[TEST_RANK_0] = r0;

    ock::mf::EntityExportInfo r1{};
    r1.rankId = TEST_RANK_1;
    std::strncpy(r1.tag, "tag_1", sizeof(r1.tag) - 1);
    entity.importedRanks_[TEST_RANK_1] = r1;

    auto ret = entity.ImportForTagManager();
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(entity.tagManager_->GetTagByRank(TEST_RANK_0), "tag_0");
    EXPECT_EQ(entity.tagManager_->GetTagByRank(TEST_RANK_1), "tag_1");
}

TEST_F(HybmEntityDefaultTest, ImportForTransportManager_NoTransport_ReturnOk)
{
    int32_t deviceId = TEST_DEVICE_ID_TRANS_MGR_NO_TRANS;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.transportManager_ = nullptr;
    auto ret = entity.ImportForTransportManager();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, ImportForTransportManager_PrepareConnect_FirstTime)
{
    int32_t deviceId = TEST_DEVICE_ID_TRANS_MGR_PREPARE;
    ock::mf::MemEntityDefault entity(deviceId);
    auto fake = std::make_shared<FakeTransportManager>();
    entity.transportManager_ = fake;
    entity.transportPrepared_ = false;

    ock::mf::EntityExportInfo r0{};
    r0.rankId = TEST_RANK_0;
    r0.role = static_cast<uint16_t>(HYBM_ROLE_SENDER);
    std::strncpy(r0.nic, "nic0", sizeof(r0.nic) - 1);
    entity.importedRanks_[TEST_RANK_0] = r0;

    ock::mf::EntityExportInfo r1{};
    r1.rankId = TEST_RANK_1;
    r1.role = static_cast<uint16_t>(HYBM_ROLE_RECEIVER);
    std::strncpy(r1.nic, "nic1", sizeof(r1.nic) - 1);
    entity.importedRanks_[TEST_RANK_1] = r1;

    auto ret = entity.ImportForTransportManager();
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(entity.transportPrepared_);
    EXPECT_EQ(fake->prepareCalled, 1);
    EXPECT_EQ(fake->connectCalled, 1);
    EXPECT_EQ(fake->preparedOptions.options.size(), TEST_RANK_COUNT_2);
    EXPECT_EQ(fake->preparedOptions.options.at(TEST_RANK_0).nic, std::string("nic0"));
    EXPECT_EQ(fake->preparedOptions.options.at(TEST_RANK_1).nic, std::string("nic1"));
}

TEST_F(HybmEntityDefaultTest, ImportForTransportManager_Update_WhenPrepared)
{
    int32_t deviceId = TEST_DEVICE_ID_TRANS_MGR_UPDATE;
    ock::mf::MemEntityDefault entity(deviceId);
    auto fake = std::make_shared<FakeTransportManager>();
    entity.transportManager_ = fake;
    entity.transportPrepared_ = true;

    ock::mf::EntityExportInfo r0{};
    r0.rankId = TEST_RANK_0;
    r0.role = static_cast<uint16_t>(HYBM_ROLE_PEER);
    std::strncpy(r0.nic, "nic0", sizeof(r0.nic) - 1);
    entity.importedRanks_[TEST_RANK_0] = r0;

    auto ret = entity.ImportForTransportManager();
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(fake->updateCalled, 1);
    EXPECT_EQ(fake->connectCalled, 0);
    EXPECT_EQ(fake->updatedOptions.options.size(), TEST_RANK_COUNT_1);
    EXPECT_EQ(fake->updatedOptions.options.at(TEST_RANK_0).nic, std::string("nic0"));
}

TEST_F(HybmEntityDefaultTest, ImportEntityExchangeInfo_Basic)
{
    int32_t deviceId = TEST_DEVICE_ID_IMPORT_EXCHANGE;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.bmType = HYBM_TYPE_HOST_INITIATE; // avoid device meta memcpy path
    entity.options_.scene = HYBM_SCENE_DEFAULT;
    entity.tagManager_ = std::make_shared<ock::mf::HybmEntityTagInfo>();
    entity.transportManager_ = std::make_shared<FakeTransportManager>();

    hybm_exchange_info ex0{};
    bzero(&ex0, sizeof(ex0));
    ock::mf::ExchangeInfoWriter w0(&ex0);
    ock::mf::EntityExportInfo e0{};
    e0.rankId = TEST_RANK_0;
    e0.role = static_cast<uint16_t>(HYBM_ROLE_SENDER);
    std::strncpy(e0.tag, "tag_0", sizeof(e0.tag) - 1);
    std::strncpy(e0.nic, "nic0", sizeof(e0.nic) - 1);
    w0.Append(e0);

    hybm_exchange_info ex1{};
    bzero(&ex1, sizeof(ex1));
    ock::mf::ExchangeInfoWriter w1(&ex1);
    ock::mf::EntityExportInfo e1{};
    e1.rankId = TEST_RANK_1;
    e1.role = static_cast<uint16_t>(HYBM_ROLE_RECEIVER);
    std::strncpy(e1.tag, "tag_1", sizeof(e1.tag) - 1);
    std::strncpy(e1.nic, "nic1", sizeof(e1.nic) - 1);
    w1.Append(e1);

    ock::mf::ExchangeInfoReader readers[TEST_RANK_COUNT_2] = {
        ock::mf::ExchangeInfoReader(&ex0), ock::mf::ExchangeInfoReader(&ex1)
    };
    auto ret = entity.ImportEntityExchangeInfo(readers, TEST_RANK_COUNT_2, 0);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(entity.tagManager_->GetTagByRank(TEST_RANK_0), "tag_0");
    EXPECT_EQ(entity.tagManager_->GetTagByRank(TEST_RANK_1), "tag_1");
    EXPECT_TRUE(entity.transportPrepared_);
}

TEST_F(HybmEntityDefaultTest, ImportForTransport_ConnectWithOptions)
{
    int32_t deviceId = TEST_DEVICE_ID_TRANS_CONNECT;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.bmType = HYBM_TYPE_HOST_INITIATE; // avoid device meta memcpy
    entity.tagManager_ = std::make_shared<ock::mf::HybmEntityTagInfo>();
    auto fake = std::make_shared<FakeTransportManager>();
    entity.transportManager_ = fake;

    ock::mf::EntityExportInfo r1{};
    r1.rankId = TEST_RANK_1;
    r1.role = static_cast<uint16_t>(HYBM_ROLE_RECEIVER);
    std::strncpy(r1.nic, "nic1", sizeof(r1.nic) - 1);
    std::strncpy(r1.tag, "tag_1", sizeof(r1.tag) - 1);
    entity.importedRanks_[TEST_RANK_1] = r1;
    ock::mf::transport::TransportMemoryKey k1{};
    std::memset(&k1, 0, sizeof(k1));
    k1.keys[0] = TEST_KEY_VALUE_2;
    ock::mf::transport::TransportMemoryKey k2{};
    std::memset(&k2, 0, sizeof(k2));
    k2.keys[0] = TEST_KEY_VALUE_3;
    entity.importedMemories_[TEST_RANK_1] = std::set<ock::mf::transport::TransportMemoryKey>{k1, k2};

    // importInfoEntity = true will also import tag and update device info (bmType host => no memcpy)
    auto ret = entity.ImportForTransport();
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(fake->connectWithOptionsCalled, 1);
    ASSERT_EQ(fake->connectWithOptions.options.count(TEST_RANK_1), 1U);
    EXPECT_EQ(fake->connectWithOptions.options.at(TEST_RANK_1).nic, std::string("nic1"));
    EXPECT_EQ(fake->connectWithOptions.options.at(TEST_RANK_1).memKeys.size(), TEST_RANK_COUNT_2);
}

TEST_F(HybmEntityDefaultTest, ExportExchangeInfo_WithLongNic_ReturnError)
{
    int32_t deviceId = TEST_DEVICE_ID_EXPORT_LONG_NIC;
    uint32_t rankCount = TEST_RANK_COUNT_2;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = TEST_RANK_0;
    entity.options_.rankCount = rankCount;
    entity.options_.scene = HYBM_SCENE_DEFAULT;
    std::strncpy(entity.options_.tag, "tag0", sizeof(entity.options_.tag) - 1);
    entity.transportManager_ = std::make_shared<FakeTransportManagerLongNic>();

    hybm_exchange_info ex{};
    bzero(&ex, sizeof(ex));
    ock::mf::ExchangeInfoWriter writer(&ex);

    auto ret = entity.ExportEntityExchangeInfo(writer, 0);
    EXPECT_NE(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, ExportExchangeInfo_AppendFail_ReturnError)
{
    int32_t deviceId = TEST_DEVICE_ID_EXPORT_APPEND_FAIL;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = TEST_RANK_0;
    entity.options_.rankCount = TEST_RANK_COUNT_1;
    entity.options_.scene = HYBM_SCENE_DEFAULT;
    std::strncpy(entity.options_.tag, "tag0", sizeof(entity.options_.tag) - 1);
    entity.transportManager_ = nullptr;

    // writer with nullptr forces Append() to fail (BM_ASSERT_RETURN -> -1)
    ock::mf::ExchangeInfoWriter badWriter(nullptr);
    auto ret = entity.ExportEntityExchangeInfo(badWriter, TEST_RANK_0);
    EXPECT_NE(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, ExportExchangeInfo_TransScene_HbmSegmentNull_ReturnError)
{
    int32_t deviceId = TEST_DEVICE_ID_EXPORT_TRANS_NULL;
    uint32_t rankCount = TEST_RANK_COUNT_2;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = TEST_RANK_0;
    entity.options_.rankCount = rankCount;
    entity.options_.scene = HYBM_SCENE_TRANS;
    std::strncpy(entity.options_.tag, "tag0", sizeof(entity.options_.tag) - 1);
    entity.transportManager_ = nullptr;
    entity.hbmSegment_ = nullptr;

    hybm_exchange_info ex{};
    bzero(&ex, sizeof(ex));
    ock::mf::ExchangeInfoWriter writer(&ex);

    auto ret = entity.ExportEntityExchangeInfo(writer, 0);
    EXPECT_NE(ret, BM_OK);
}
