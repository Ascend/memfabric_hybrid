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
    std::string longNic_{std::string(64, 'x')};
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
    ock::mf::MemEntityDefault entity(100);
    hybm_options options{};
    options.rankId = 0;
    options.rankCount = 1;

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
    auto allocRet = entity.AllocLocalMemory(1024, HYBM_MEM_TYPE_HOST, 0, slice);
    EXPECT_EQ(allocRet, BM_INVALID_PARAM);
    EXPECT_EQ(slice, nullptr);

    // 测试内存分配（已初始化的情况）dramSegment_ 为空
    entity.dramSegment_ = nullptr;
    allocRet = entity.AllocLocalMemory(ock::mf::HYBM_LARGE_PAGE_SIZE, HYBM_MEM_TYPE_HOST, 0, slice);
    EXPECT_EQ(allocRet, BM_INVALID_PARAM);
    EXPECT_EQ(slice, nullptr);

    // 测试反初始化
    entity.UnInitialize();
    EXPECT_FALSE(entity.initialized_);
}

// 测试 MemEntityDefault 内存预留和释放
TEST_F(HybmEntityDefaultTest, Reserve_UnReserve_MemorySpace)
{
    ock::mf::MemEntityDefault entity(200);

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
    options.rankId = 0;
    options.rankCount = 1;
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
    ock::mf::MemEntityDefault entity(300);

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
    ock::mf::MemEntityDefault entity(400);
    hybm_mem_slice_t slice = nullptr;

    // 测试内存分配（未初始化的情况）
    int32_t allocRet = entity.AllocLocalMemory(1024, HYBM_MEM_TYPE_HOST, 0, slice);
    EXPECT_EQ(allocRet, BM_NOT_INITIALIZED);
    EXPECT_EQ(slice, nullptr);

    // 测试内存释放（未初始化的情况）
    int32_t freeRet = entity.FreeLocalMemory(slice, 0);
    EXPECT_EQ(freeRet, BM_INVALID_PARAM);

    // 测试初始化
    // 模拟 MemSegment::Create 返回空指针
    ock::mf::MemSegmentOptions optionsSeg{};
    optionsSeg.segType = ock::mf::HYBM_MST_HBM;
    optionsSeg.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    optionsSeg.rankCnt = 1;

    ock::mf::MemSegmentPtr segment = std::make_shared<ock::mf::HybmVmmBasedSegment>(optionsSeg, entity.id_);

    hybm_options options{};
    options.rankId = 0;
    options.rankCount = 1;
    options.memType = HYBM_MEM_TYPE_DEVICE;
    options.maxDRAMSize = ock::mf::HYBM_LARGE_PAGE_SIZE;

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

    allocRet = entity.AllocLocalMemory(1024, HYBM_MEM_TYPE_HOST, 0, slice);
    EXPECT_EQ(allocRet, BM_INVALID_PARAM);

    allocRet = entity.AllocLocalMemory(ock::mf::HYBM_LARGE_PAGE_SIZE, HYBM_MEM_TYPE_DEVICE, 0, slice);
    EXPECT_EQ(allocRet, BM_INVALID_PARAM);

    freeRet = entity.FreeLocalMemory(slice, 0);
    EXPECT_EQ(freeRet, BM_OK);
}

// 测试 MemEntityDefault 内存注册
TEST_F(HybmEntityDefaultTest, RegisterLocalMemory)
{
    ock::mf::MemEntityDefault entity(500);

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
    optionsSeg.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    optionsSeg.rankCnt = 2;
    ock::mf::MemSegmentPtr segment = std::make_shared<ock::mf::HybmVmmBasedSegment>(optionsSeg, entity.id_);

    MOCKER_CPP(&ock::mf::HybmVmmBasedSegment::ReserveMemorySpace, int32_t (*)(ock::mf::HybmVmmBasedSegment *, void **))
        .stubs()
        .will(returnValue(0));

    hybm_options options{};
    options.rankId = 0;
    options.rankCount = 2;
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
    ock::mf::MemEntityDefault entity(600);

    hybm_exchange_info hbmSliceInfo;
    bzero(&hbmSliceInfo, sizeof(hybm_exchange_info));
    ock::mf::ExchangeInfoWriter writer(&hbmSliceInfo);

    // 测试导出实体信息（未初始化的情况）
    int32_t exportRet = entity.ExportExchangeInfo(writer, 0);
    EXPECT_EQ(exportRet, BM_NOT_INITIALIZED);

    // 测试导出切片信息（未初始化的情况）
    exportRet = entity.ExportExchangeInfo(nullptr, writer, 0);
    EXPECT_EQ(exportRet, BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 导入交换信息
TEST_F(HybmEntityDefaultTest, ImportExchangeInfo)
{
    ock::mf::MemEntityDefault entity(100);
    hybm_exchange_info info{};
    void *addresses[1] = {nullptr};

    // 测试导入切片信息（未初始化的情况）
    int32_t importRet = entity.ImportExchangeInfo(&info, 1, addresses, 0);
    EXPECT_EQ(importRet, BM_NOT_INITIALIZED);

    // 测试初始化
    hybm_options options{};
    options.rankId = 0;
    options.rankCount = 1;
    int32_t initRet = entity.Initialize(&options);
    EXPECT_EQ(initRet, BM_OK);

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(0));

    importRet = entity.ImportExchangeInfo(&info, 1, addresses, 0);
    EXPECT_EQ(importRet, BM_OK);

    entity.UnInitialize();
    EXPECT_FALSE(entity.initialized_);
}

// 测试 MemEntityDefault 移除导入的内存
TEST_F(HybmEntityDefaultTest, RemoveImported)
{
    ock::mf::MemEntityDefault entity(900);
    std::vector<uint32_t> ranks = {1, 2, 3};

    // 测试移除导入的内存（未初始化的情况）
    int32_t removeRet = entity.RemoveImported(ranks);
    EXPECT_EQ(removeRet, BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 设置额外上下文
TEST_F(HybmEntityDefaultTest, SetExtraContext)
{
    ock::mf::MemEntityDefault entity(1000);
    int ctx = 123;

    // 测试设置额外上下文（未初始化的情况）
    int32_t setCtxRet = entity.SetExtraContext(&ctx, sizeof(ctx));
    EXPECT_EQ(setCtxRet, BM_NOT_INITIALIZED);
}

TEST_F(HybmEntityDefaultTest, SetExtraContext_ContextNull_ReturnInvalidParam)
{
    int32_t deviceId = 2300;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.bmType = HYBM_TYPE_HOST_INITIATE;

    auto ret = entity.SetExtraContext(nullptr, 1);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmEntityDefaultTest, SetExtraContext_SizeTooLarge_ReturnInvalidParam)
{
    int32_t deviceId = 2301;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.bmType = HYBM_TYPE_HOST_INITIATE;

    int ctx = 123;
    auto ret = entity.SetExtraContext(&ctx, ock::mf::HYBM_DEVICE_USER_CONTEXT_PRE_SIZE + 1);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST_F(HybmEntityDefaultTest, SetExtraContext_MemcpyFail_ReturnError)
{
    int32_t deviceId = 2302;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.bmType = HYBM_TYPE_HOST_INITIATE;

    int ctx = 123;
    MOCKER_CPP(&ock::mf::DlAclApi::AclrtMemcpy, int32_t (*)(void *, size_t, const void *, size_t, int32_t))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_ERROR)));

    auto ret = entity.SetExtraContext(&ctx, sizeof(ctx));
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, SetExtraContext_Success_ReturnOk)
{
    int32_t deviceId = 2303;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.bmType = HYBM_TYPE_HOST_INITIATE; // UpdateHybmDeviceInfo will return OK directly

    int ctx = 123;
    MOCKER_CPP(&ock::mf::DlAclApi::AclrtMemcpy, int32_t (*)(void *, size_t, const void *, size_t, int32_t))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    auto ret = entity.SetExtraContext(&ctx, sizeof(ctx));
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, RemoveImported_HbmSegmentFail_ReturnError)
{
    int32_t deviceId = 2400;
    uint32_t rankCnt = 2;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;

    ock::mf::MemSegmentOptions optionsHbm{};
    optionsHbm.segType = ock::mf::HYBM_MST_HBM;
    optionsHbm.rankCnt = rankCnt;
    auto hbmSeg = std::make_shared<ock::mf::HybmVmmBasedSegment>(optionsHbm, entity.id_);
    entity.hbmSegment_ = hbmSeg;

    std::vector<uint32_t> ranks{1, 2};
    MOCKER_CPP(&ock::mf::HybmVmmBasedSegment::RemoveImported,
               int32_t (*)(ock::mf::HybmVmmBasedSegment *, const std::vector<uint32_t> &))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_ERROR)));

    auto ret = entity.RemoveImported(ranks);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, RemoveImported_DramSegmentFail_ReturnError)
{
    int32_t deviceId = 2401;
    uint32_t totalCnt = 2;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;

    ock::mf::MemSegmentOptions optionsDram{};
    optionsDram.segType = ock::mf::HYBM_MST_DRAM;
    optionsDram.rankCnt = totalCnt;
    auto dramSeg = std::make_shared<ock::mf::HybmVmmBasedSegment>(optionsDram, entity.id_);
    entity.dramSegment_ = dramSeg;

    std::vector<uint32_t> ranks{1, 2};
    MOCKER_CPP(&ock::mf::HybmVmmBasedSegment::RemoveImported,
               int32_t (*)(ock::mf::HybmVmmBasedSegment *, const std::vector<uint32_t> &))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_ERROR)));

    auto ret = entity.RemoveImported(ranks);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, RemoveImported_CleanupAndEraseAndRemoveRanks)
{
    int32_t deviceId = 2402;
    uint32_t totalCnt = 3;
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
    r1.rankId = 1;
    entity.importedRanks_[1] = r1;
    entity.importedMemories_[1] = {};
    ock::mf::EntityExportInfo rankSec{};
    uint32_t r2RankId = 2;
    rankSec.rankId = r2RankId;
    entity.importedRanks_[r2RankId] = rankSec;
    entity.importedMemories_[r2RankId] = {};

    std::vector<uint32_t> ranks{1, 2};
    auto ret = entity.RemoveImported(ranks);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(dop->cleaned);
    EXPECT_EQ(entity.importedRanks_.count(1), 0U);
    EXPECT_EQ(entity.importedRanks_.count(r2RankId), 0U);
    EXPECT_EQ(entity.importedMemories_.count(1), 0U);
    EXPECT_EQ(entity.importedMemories_.count(r2RankId), 0U);
    EXPECT_EQ(trans->removeCalled, 1);
}

TEST_F(HybmEntityDefaultTest, CopyData_TransScene_UsesLocateAddrAndRank)
{
    int32_t deviceId = 2500;
    uint32_t rankId = 9;
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
    params.src = reinterpret_cast<void *>(0x1111);
    params.dest = reinterpret_cast<void *>(0x2222);
    uint64_t dataSize = 4096;
    params.dataSize = dataSize;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, CopyData_DataCopyFail_ReturnErrorCode)
{
    int32_t deviceId = 2502;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = 0;
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
    params.src = reinterpret_cast<void *>(0x1111);
    params.dest = reinterpret_cast<void *>(0x2222);
    uint64_t dataSize = 4096;
    params.dataSize = dataSize;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, CopyData_NonTransScene_UseLocalRankForAddrOutOfGvmRange)
{
    int32_t deviceId = 25021UL;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = 6UL;
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
    params.src = reinterpret_cast<void *>(0x1111);
    params.dest = reinterpret_cast<void *>(0x2222);
    params.dataSize = 4096ULL;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(dop->dataCopyCalled);
    EXPECT_EQ(dop->lastDataCopyOptions.srcRankId, 6U);
    EXPECT_EQ(dop->lastDataCopyOptions.destRankId, 6U);
}

TEST_F(HybmEntityDefaultTest, CopyData_NonTransScene_UseRankFromVaManagerInSocRange)
{
    int32_t deviceId = 25022;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = 3UL;
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
    ASSERT_EQ(ock::mf::HybmVaManager::GetInstance().AddVaInfoFromExternal(
                  {{ock::mf::HYBM_GVM_START_ADDR + 0x1000, 0, 0}, 0x4000, HYBM_MEM_TYPE_HOST}, 0, 11UL),
              BM_OK);
    ASSERT_EQ(ock::mf::HybmVaManager::GetInstance().AddVaInfoFromExternal(
                  {{ock::mf::HYBM_GVM_START_ADDR + 0x9000, 0, 0}, 0x4000, HYBM_MEM_TYPE_HOST}, 0, 12UL),
              BM_OK);

    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(ock::mf::HYBM_GVM_START_ADDR + 0x1800);
    params.dest = reinterpret_cast<void *>(ock::mf::HYBM_GVM_START_ADDR + 0x9800);
    params.dataSize = 4096ULL;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(dop->dataCopyCalled);
    EXPECT_EQ(dop->lastDataCopyOptions.srcRankId, 11U);
    EXPECT_EQ(dop->lastDataCopyOptions.destRankId, 12U);
    ock::mf::HybmVaManager::GetInstance().ClearAll();
}

TEST_F(HybmEntityDefaultTest, CopyData_NonTransScene_A5Soc_UseRankFromVaManagerInA5Range)
{
    int32_t deviceId = 25023;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = 8UL;
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
    ASSERT_EQ(ock::mf::HybmVaManager::GetInstance().AddVaInfoFromExternal(
                  {{ock::mf::HYBM_GVM_START_ADDR_A5 + 0x1000, 0, 0}, 0x4000, HYBM_MEM_TYPE_HOST}, 0, 13UL),
              BM_OK);

    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;

    hybm_copy_params params{};
    params.src = reinterpret_cast<void *>(ock::mf::HYBM_GVM_START_ADDR_A5 + 0x1800);
    params.dest = reinterpret_cast<void *>(0x2222);
    params.dataSize = 4096ULL;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(dop->dataCopyCalled);
    EXPECT_EQ(dop->lastDataCopyOptions.srcRankId, 13U);
    EXPECT_EQ(dop->lastDataCopyOptions.destRankId, 8U);
    ock::mf::HybmVaManager::GetInstance().ClearAll();
}

TEST_F(HybmEntityDefaultTest, CopyData_NonTransScene_Enable56BitsGvaOutOfA5Range_UseLocalRank)
{
    int32_t deviceId = 250231;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = 8UL;
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
    params.dest = reinterpret_cast<void *>(0x2222);
    params.dataSize = 4096ULL;

    auto ret = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(dop->dataCopyCalled);
    EXPECT_EQ(dop->lastDataCopyOptions.srcRankId, 8U);
    EXPECT_EQ(dop->lastDataCopyOptions.destRankId, 8U);
}

TEST_F(HybmEntityDefaultTest, CopyData_NonTransScene_Enable56BitsGvaAddrWithoutRegisteredGva_UseLocalRank)
{
    int32_t deviceId = 25024;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = 8UL;
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
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(dop->dataCopyCalled);
    EXPECT_EQ(dop->lastDataCopyOptions.srcRankId, 8U);
    EXPECT_EQ(dop->lastDataCopyOptions.destRankId, 8U);
    ock::mf::HybmVaManager::GetInstance().ClearAll();
}

TEST_F(HybmEntityDefaultTest, BatchCopyData_DataOperatorNull_ReturnError)
{
    int32_t deviceId = 2503;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.scene = HYBM_SCENE_TRANS;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    hybm_batch_copy_params params{};
    params.batchSize = 1;
    auto ret = entity.BatchCopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, BatchCopyData_GroupAndSuccess)
{
    int32_t deviceId = 2504;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = 0;
    entity.options_.scene = HYBM_SCENE_TRANS;

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice, int32_t (*)(ock::mf::MemEntityDefault *))
        .stubs()
        .will(returnValue(static_cast<int32_t>(BM_OK)));

    auto dop = std::make_shared<FakeDataOperator>();
    entity.dataOperator_ = dop;
    void *srcs[2] = {reinterpret_cast<void *>(0x1), reinterpret_cast<void *>(0x2)};
    void *dsts[2] = {reinterpret_cast<void *>(0x3), reinterpret_cast<void *>(0x4)};
    uint64_t sizes[2] = {4096, 8192};
    hybm_batch_copy_params params{};
    params.sources = srcs;
    params.destinations = dsts;
    params.dataSizes = sizes;
    uint32_t batchSize = 2;
    params.batchSize = batchSize;

    auto ret = entity.BatchCopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, BatchCopyData_BatchCopyFail_ReturnErrorCode)
{
    int32_t deviceId = 2505;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = 0;
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
    uint64_t sizes[1] = {4096};
    hybm_batch_copy_params params{};
    params.sources = srcs;
    params.destinations = dsts;
    params.dataSizes = sizes;
    params.batchSize = 1;

    auto ret = entity.BatchCopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(ret, BM_ERROR);
}

TEST_F(HybmEntityDefaultTest, QuantCopy_DataOperatorNull_ReturnError)
{
    int32_t deviceId = 2506;
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
    int32_t deviceId = 2507;
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
    int32_t deviceId = 2508;
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
    int32_t deviceId = 2509;
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
    int32_t deviceId = 1100;
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
    int32_t deviceId = 1200;
    ock::mf::MemEntityDefault entity(deviceId);

    // 测试地址检查（未初始化的情况）
    int buf = 0;
    bool inEntity = entity.CheckAddressInEntity(&buf, sizeof(buf));
    EXPECT_FALSE(inEntity);
}

// 测试 MemEntityDefault 数据复制
TEST_F(HybmEntityDefaultTest, CopyData)
{
    int32_t deviceId = 1300;
    ock::mf::MemEntityDefault entity(deviceId);
    hybm_copy_params params{};

    // 测试数据复制（未初始化的情况）
    int32_t copyRet = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(copyRet, BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 批量数据复制
TEST_F(HybmEntityDefaultTest, BatchCopyData)
{
    int32_t deviceId = 1400;
    ock::mf::MemEntityDefault entity(deviceId);
    hybm_batch_copy_params params{};

    // 测试批量数据复制（未初始化的情况）
    int32_t batchCopyRet = entity.BatchCopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(batchCopyRet, BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 等待操作
TEST_F(HybmEntityDefaultTest, Wait)
{
    int32_t deviceId = 1500;
    ock::mf::MemEntityDefault entity(deviceId);

    // 测试等待操作（未初始化的情况）
    int32_t waitRet = entity.Wait();
    EXPECT_EQ(waitRet, BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault SDMA 可达性检查
TEST_F(HybmEntityDefaultTest, SdmaReaches)
{
    int32_t deviceId = 1600;
    ock::mf::MemEntityDefault entity(deviceId);

    // 测试 SDMA 可达性检查（未初始化的情况）
    bool reaches = entity.SdmaReaches(1);
    EXPECT_FALSE(reaches);
}

// 测试 MemEntityDefault 数据操作类型检查
TEST_F(HybmEntityDefaultTest, CanReachDataOperators)
{
    int32_t deviceId = 1700;
    ock::mf::MemEntityDefault entity(deviceId);

    // 测试数据操作类型检查（未初始化的情况）
    hybm_data_op_type opType = entity.CanReachDataOperators(1);
    EXPECT_EQ(opType, HYBM_DOP_TYPE_DEFAULT);
}

// 测试 MemEntityDefault 获取切片虚拟地址
TEST_F(HybmEntityDefaultTest, GetSliceVa)
{
    int32_t deviceId = 1800;
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
    options.rankId = 0;
    options.rankCount = 1;
    ret = ock::mf::MemEntityDefault::CheckOptions(&options);
    EXPECT_EQ(ret, BM_OK);
}

// 测试 MemEntityDefault 功能修改拦截
TEST_F(HybmEntityDefaultTest, MemEntityDefault_FunctionModification_Intercept)
{
    int32_t deviceId = 1900;
    ock::mf::MemEntityDefault entity(deviceId);
    hybm_options options{};
    options.rankId = 0;
    options.rankCount = 1;

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
    int32_t deviceId = 2000;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.tagManager_ = std::make_shared<ock::mf::HybmEntityTagInfo>();

    ock::mf::EntityExportInfo r0{};
    r0.rankId = 0;
    std::strncpy(r0.tag, "tag_0", sizeof(r0.tag) - 1);
    entity.importedRanks_[0] = r0;

    ock::mf::EntityExportInfo r1{};
    r1.rankId = 1;
    std::strncpy(r1.tag, "tag_1", sizeof(r1.tag) - 1);
    entity.importedRanks_[1] = r1;

    auto ret = entity.ImportForTagManager();
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(entity.tagManager_->GetTagByRank(0), "tag_0");
    EXPECT_EQ(entity.tagManager_->GetTagByRank(1), "tag_1");
}

TEST_F(HybmEntityDefaultTest, ImportForTransportManager_NoTransport_ReturnOk)
{
    int32_t deviceId = 2001;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.transportManager_ = nullptr;
    auto ret = entity.ImportForTransportManager();
    EXPECT_EQ(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, ImportForTransportManager_PrepareConnect_FirstTime)
{
    int32_t deviceId = 2002;
    ock::mf::MemEntityDefault entity(deviceId);
    auto fake = std::make_shared<FakeTransportManager>();
    entity.transportManager_ = fake;
    entity.transportPrepared_ = false;

    ock::mf::EntityExportInfo r0{};
    r0.rankId = 0;
    r0.role = static_cast<uint16_t>(HYBM_ROLE_SENDER);
    std::strncpy(r0.nic, "nic0", sizeof(r0.nic) - 1);
    entity.importedRanks_[0] = r0;

    ock::mf::EntityExportInfo r1{};
    r1.rankId = 1;
    r1.role = static_cast<uint16_t>(HYBM_ROLE_RECEIVER);
    std::strncpy(r1.nic, "nic1", sizeof(r1.nic) - 1);
    entity.importedRanks_[1] = r1;

    auto ret = entity.ImportForTransportManager();
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(entity.transportPrepared_);
    EXPECT_EQ(fake->prepareCalled, 1);
    EXPECT_EQ(fake->connectCalled, 1);
    EXPECT_EQ(fake->preparedOptions.options.size(), 2U);
    EXPECT_EQ(fake->preparedOptions.options.at(0).nic, std::string("nic0"));
    EXPECT_EQ(fake->preparedOptions.options.at(1).nic, std::string("nic1"));
}

TEST_F(HybmEntityDefaultTest, ImportForTransportManager_Update_WhenPrepared)
{
    int32_t deviceId = 2003;
    ock::mf::MemEntityDefault entity(deviceId);
    auto fake = std::make_shared<FakeTransportManager>();
    entity.transportManager_ = fake;
    entity.transportPrepared_ = true;

    ock::mf::EntityExportInfo r0{};
    r0.rankId = 0;
    r0.role = static_cast<uint16_t>(HYBM_ROLE_PEER);
    std::strncpy(r0.nic, "nic0", sizeof(r0.nic) - 1);
    entity.importedRanks_[0] = r0;

    auto ret = entity.ImportForTransportManager();
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(fake->updateCalled, 1);
    EXPECT_EQ(fake->connectCalled, 0);
    EXPECT_EQ(fake->updatedOptions.options.size(), 1U);
    EXPECT_EQ(fake->updatedOptions.options.at(0).nic, std::string("nic0"));
}

TEST_F(HybmEntityDefaultTest, ImportEntityExchangeInfo_Basic)
{
    int32_t deviceId = 2004;
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
    e0.rankId = 0;
    e0.role = static_cast<uint16_t>(HYBM_ROLE_SENDER);
    std::strncpy(e0.tag, "tag_0", sizeof(e0.tag) - 1);
    std::strncpy(e0.nic, "nic0", sizeof(e0.nic) - 1);
    w0.Append(e0);

    hybm_exchange_info ex1{};
    bzero(&ex1, sizeof(ex1));
    ock::mf::ExchangeInfoWriter w1(&ex1);
    ock::mf::EntityExportInfo e1{};
    e1.rankId = 1;
    e1.role = static_cast<uint16_t>(HYBM_ROLE_RECEIVER);
    std::strncpy(e1.tag, "tag_1", sizeof(e1.tag) - 1);
    std::strncpy(e1.nic, "nic1", sizeof(e1.nic) - 1);
    w1.Append(e1);

    ock::mf::ExchangeInfoReader readers[2] = {ock::mf::ExchangeInfoReader(&ex0), ock::mf::ExchangeInfoReader(&ex1)};
    auto ret = entity.ImportEntityExchangeInfo(readers, 2, 0);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(entity.tagManager_->GetTagByRank(0), "tag_0");
    EXPECT_EQ(entity.tagManager_->GetTagByRank(1), "tag_1");
    EXPECT_TRUE(entity.transportPrepared_);
}

TEST_F(HybmEntityDefaultTest, ImportForTransportPrecheck_ParseEntityAndSlice)
{
    int32_t deviceId = 2005;
    ock::mf::MemEntityDefault entity(deviceId);

    // desc[0]: entity export info
    hybm_exchange_info exEntity{};
    bzero(&exEntity, sizeof(exEntity));
    ock::mf::ExchangeInfoWriter wEntity(&exEntity);
    ock::mf::EntityExportInfo e0{};
    uint32_t rankId = 3;
    e0.rankId = rankId;
    e0.role = static_cast<uint16_t>(HYBM_ROLE_PEER);
    std::strncpy(e0.tag, "tag_3", sizeof(e0.tag) - 1);
    std::strncpy(e0.nic, "nic3", sizeof(e0.nic) - 1);
    wEntity.Append(e0);

    // desc[1]: slice transport key
    hybm_exchange_info exSlice{};
    bzero(&exSlice, sizeof(exSlice));
    ock::mf::ExchangeInfoWriter wSlice(&exSlice);
    ock::mf::SliceExportTransportKey k0(ock::mf::DRAM_SLICE_EXPORT_INFO_MAGIC, rankId, 0x1234);
    std::memset(&k0.key, 0, sizeof(k0.key));
    k0.key.keys[0] = 0xABC;
    wSlice.Append(k0);

    bool importInfoEntity = false;
    uint32_t count = 2;
    ock::mf::ExchangeInfoReader descArr[count] = {ock::mf::ExchangeInfoReader(&exEntity),
                                                  ock::mf::ExchangeInfoReader(&exSlice)};
    auto ret = entity.ImportForTransportPrecheck(descArr, count, importInfoEntity);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(importInfoEntity);
    EXPECT_EQ(entity.importedRanks_.count(rankId), 1U);
    EXPECT_EQ(entity.importedMemories_.count(rankId), 1U);
    ASSERT_EQ(entity.importedMemories_[rankId].size(), 1U);
    EXPECT_EQ(entity.importedMemories_[rankId].begin()->keys[0], 0xABC);
}

TEST_F(HybmEntityDefaultTest, ImportForTransportPrecheck_InvalidMagic_Fail)
{
    int32_t deviceId = 2006;
    ock::mf::MemEntityDefault entity(deviceId);
    hybm_exchange_info exInfo{};
    bzero(&exInfo, sizeof(exInfo));
    ock::mf::ExchangeInfoWriter writer(&exInfo);
    uint64_t badMagic = 0xDEADBEEF;
    writer.Append(badMagic);
    ock::mf::ExchangeInfoReader desc(&exInfo);
    bool importInfoEntity = false;
    uint32_t count = 1;
    auto ret = entity.ImportForTransportPrecheck(&desc, count, importInfoEntity);
    EXPECT_NE(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, ImportForTransport_ConnectWithOptions)
{
    int32_t deviceId = 2007;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.bmType = HYBM_TYPE_HOST_INITIATE; // avoid device meta memcpy
    entity.tagManager_ = std::make_shared<ock::mf::HybmEntityTagInfo>();
    auto fake = std::make_shared<FakeTransportManager>();
    entity.transportManager_ = fake;

    ock::mf::EntityExportInfo r1{};
    r1.rankId = 1;
    r1.role = static_cast<uint16_t>(HYBM_ROLE_RECEIVER);
    std::strncpy(r1.nic, "nic1", sizeof(r1.nic) - 1);
    std::strncpy(r1.tag, "tag_1", sizeof(r1.tag) - 1);
    entity.importedRanks_[1] = r1;
    ock::mf::transport::TransportMemoryKey k1{};
    std::memset(&k1, 0, sizeof(k1));
    k1.keys[0] = 0x11;
    ock::mf::transport::TransportMemoryKey k2{};
    std::memset(&k2, 0, sizeof(k2));
    k2.keys[0] = 0x22;
    entity.importedMemories_[1] = std::set<ock::mf::transport::TransportMemoryKey>{k1, k2};

    // importInfoEntity = true will also import tag and update device info (bmType host => no memcpy)
    auto ret = entity.ImportForTransport(true);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(fake->connectWithOptionsCalled, 1);
    ASSERT_EQ(fake->connectWithOptions.options.count(1), 1U);
    EXPECT_EQ(fake->connectWithOptions.options.at(1).nic, std::string("nic1"));
    EXPECT_EQ(fake->connectWithOptions.options.at(1).memKeys.size(), 2U);
    EXPECT_EQ(entity.tagManager_->GetTagByRank(1), "tag_1");
}

TEST_F(HybmEntityDefaultTest, ExportExchangeInfo_WithLongNic_ReturnError)
{
    int32_t deviceId = 2100;
    uint32_t rankCount = 2;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = 0;
    entity.options_.rankCount = rankCount;
    entity.options_.scene = HYBM_SCENE_DEFAULT;
    std::strncpy(entity.options_.tag, "tag0", sizeof(entity.options_.tag) - 1);
    entity.transportManager_ = std::make_shared<FakeTransportManagerLongNic>();

    hybm_exchange_info ex{};
    bzero(&ex, sizeof(ex));
    ock::mf::ExchangeInfoWriter writer(&ex);

    auto ret = entity.ExportExchangeInfo(writer, 0);
    EXPECT_NE(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, ExportExchangeInfo_AppendFail_ReturnError)
{
    int32_t deviceId = 2101;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = 0;
    entity.options_.rankCount = 1;
    entity.options_.scene = HYBM_SCENE_DEFAULT;
    std::strncpy(entity.options_.tag, "tag0", sizeof(entity.options_.tag) - 1);
    entity.transportManager_ = nullptr;

    // writer with nullptr forces Append() to fail (BM_ASSERT_RETURN -> -1)
    ock::mf::ExchangeInfoWriter badWriter(nullptr);
    auto ret = entity.ExportExchangeInfo(badWriter, 0);
    EXPECT_NE(ret, BM_OK);
}

TEST_F(HybmEntityDefaultTest, ExportExchangeInfo_TransScene_HbmSegmentNull_ReturnError)
{
    int32_t deviceId = 2102;
    uint32_t rankCount = 2;
    ock::mf::MemEntityDefault entity(deviceId);
    entity.initialized_ = true;
    entity.options_.rankId = 0;
    entity.options_.rankCount = rankCount;
    entity.options_.scene = HYBM_SCENE_TRANS;
    std::strncpy(entity.options_.tag, "tag0", sizeof(entity.options_.tag) - 1);
    entity.transportManager_ = nullptr;
    entity.hbmSegment_ = nullptr;

    hybm_exchange_info ex{};
    bzero(&ex, sizeof(ex));
    ock::mf::ExchangeInfoWriter writer(&ex);

    auto ret = entity.ExportExchangeInfo(writer, 0);
    EXPECT_NE(ret, BM_OK);
}
