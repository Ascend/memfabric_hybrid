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

#include "hybm_big_mem.h"
#include "hybm_common_include.h"
#include "hybm_entity_factory.h"
#include "hybm_entity_default.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

class HybmBigMemEntryTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
    }
    static void TearDownTestCase()
    {
    }

    void SetUp() override
    {
        GlobalMockObject::reset();
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
    }
};

class MemEntityStub : public ock::mf::MemEntityDefault {
public:
    explicit MemEntityStub(int32_t id) noexcept : MemEntityDefault(id)
    {
    }
    ~MemEntityStub() override
    {
    }

    // 覆盖所有纯虚接口，避免真正触发底层依赖
    int32_t Initialize(const hybm_options* /* options */) noexcept override
    {
        initCalled = true;
        return initRet;
    }

    void UnInitialize() noexcept override
    {
        uninitCalled = true;
    }

    int32_t ReserveMemorySpace() noexcept override
    {
        reserveCalled = true;
        return reserveRet;
    }

    int32_t UnReserveMemorySpace() noexcept override
    {
        unreserveCalled = true;
        return unreserveRet;
    }

    void* GetReservedMemoryPtr(hybm_mem_type memType) noexcept override
    {
        lastMemType = memType;
        return reservedPtr;
    }

    int32_t AllocLocalMemory(uint64_t size, hybm_mem_type mType, uint32_t flags,
                             hybm_mem_slice_t& slice) noexcept override
    {
        allocCalled = true;
        allocSize = size;
        allocMemType = mType;
        allocFlags = flags;
        slice = allocSlice;
        return allocRet;
    }

    int32_t RegisterLocalMemory(const void* ptr, uint64_t size, uint32_t flags,
                                hybm_mem_slice_t& slice) noexcept override
    {
        regCalled = true;
        regPtr = ptr;
        regSize = size;
        regFlags = flags;
        slice = regSlice;
        return regRet;
    }

    int32_t FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept override
    {
        freeCalled = true;
        freeSlice = slice;
        freeFlags = flags;
        return freeRet;
    }

    int32_t ExportExchangeInfo(ock::mf::ExchangeInfoWriter& /* desc */, uint32_t flags) noexcept override
    {
        exportEntityCalled = true;
        exportFlags = flags;
        return exportEntityRet;
    }

    int32_t ExportExchangeInfo(hybm_mem_slice_t slice, ock::mf::ExchangeInfoWriter& /* desc */,
                               uint32_t flags) noexcept override
    {
        exportSliceCalled = true;
        exportSlice = slice;
        exportFlags = flags;
        return exportSliceRet;
    }

    int32_t ImportExchangeInfo(const ock::mf::ExchangeInfoReader desc[], uint32_t count, void* addresses[],
                               uint32_t flags) noexcept override
    {
        importCalled = true;
        importCount = count;
        importFlags = flags;
        importAddresses = addresses;
        importDesc = desc;
        return importRet;
    }

    int32_t ImportEntityExchangeInfo(const ock::mf::ExchangeInfoReader desc[], uint32_t count,
                                     uint32_t flags) noexcept override
    {
        importEntityCalled = true;
        importEntityCount = count;
        importEntityFlags = flags;
        importEntityDesc = desc;
        return importEntityRet;
    }

    int32_t GetExportSliceInfoSize(size_t& size) noexcept override
    {
        getExportSizeCalled = true;
        size = exportSizeValue;
        return exportSizeRet;
    }

    int32_t RemoveImported(const std::vector<uint32_t>& ranks) noexcept override
    {
        removeImportedCalled = true;
        removedRanks = ranks;
        return removeImportedRet;
    }

    int32_t SetExtraContext(const void* context, uint32_t size) noexcept override
    {
        extraCtxCalled = true;
        extraCtxPtr = context;
        extraCtxSize = size;
        return extraCtxRet;
    }

    int32_t Mmap() noexcept override
    {
        mmapCalled = true;
        return mmapRet;
    }

    void Unmap() noexcept override
    {
        unmapCalled = true;
    }

    bool SdmaReaches(uint32_t remoteRank) const noexcept override
    {
        (void)remoteRank;
        return sdmaReachable;
    }

    hybm_data_op_type CanReachDataOperators(uint32_t remoteRank) const noexcept override
    {
        (void)remoteRank;
        return reachTypes;
    }

    bool CheckAddressInEntity(const void* ptr, uint64_t length) const noexcept override
    {
        (void)ptr;
        (void)length;
        return checkAddrResult;
    }

    int32_t CopyData(hybm_copy_params& /* params */, hybm_data_copy_direction /* direction */, void* /* stream */,
                     uint32_t /* flags */) noexcept override
    {
        return ock::mf::BM_OK;
    }

    int32_t BatchCopyData(hybm_batch_copy_params& /* params */, hybm_data_copy_direction /* direction */,
                          void* /* stream */, uint32_t /* flags */) noexcept override
    {
        return ock::mf::BM_OK;
    }

    int32_t Wait() noexcept override
    {
        return ock::mf::BM_OK;
    }

    // 状态记录字段，方便 UT 验证
    bool initCalled{false};
    int32_t initRet{ock::mf::BM_OK};

    bool uninitCalled{false};

    bool reserveCalled{false};
    int32_t reserveRet{ock::mf::BM_OK};

    bool unreserveCalled{false};
    int32_t unreserveRet{ock::mf::BM_OK};

    void* reservedPtr{nullptr};
    hybm_mem_type lastMemType{HYBM_MEM_TYPE_HOST};

    bool allocCalled{false};
    uint64_t allocSize{0};
    hybm_mem_type allocMemType{HYBM_MEM_TYPE_HOST};
    uint32_t allocFlags{0};
    hybm_mem_slice_t allocSlice{reinterpret_cast<hybm_mem_slice_t>(0x1234)};
    int32_t allocRet{ock::mf::BM_OK};

    bool regCalled{false};
    const void* regPtr{nullptr};
    uint64_t regSize{0};
    uint32_t regFlags{0};
    hybm_mem_slice_t regSlice{reinterpret_cast<hybm_mem_slice_t>(0x5678)};
    int32_t regRet{ock::mf::BM_OK};

    bool freeCalled{false};
    hybm_mem_slice_t freeSlice{nullptr};
    uint32_t freeFlags{0};
    int32_t freeRet{ock::mf::BM_OK};

    bool exportEntityCalled{false};
    bool exportSliceCalled{false};
    hybm_mem_slice_t exportSlice{nullptr};
    uint32_t exportFlags{0};
    int32_t exportEntityRet{ock::mf::BM_OK};
    int32_t exportSliceRet{ock::mf::BM_OK};

    bool getExportSizeCalled{false};
    size_t exportSizeValue{128};
    int32_t exportSizeRet{ock::mf::BM_OK};

    bool importCalled{false};
    const ock::mf::ExchangeInfoReader* importDesc{nullptr};
    uint32_t importCount{0};
    uint32_t importFlags{0};
    void** importAddresses{nullptr};
    int32_t importRet{ock::mf::BM_OK};

    bool importEntityCalled{false};
    const ock::mf::ExchangeInfoReader* importEntityDesc{nullptr};
    uint32_t importEntityCount{0};
    uint32_t importEntityFlags{0};
    int32_t importEntityRet{ock::mf::BM_OK};

    bool removeImportedCalled{false};
    std::vector<uint32_t> removedRanks;
    int32_t removeImportedRet{ock::mf::BM_OK};

    bool extraCtxCalled{false};
    const void* extraCtxPtr{nullptr};
    uint32_t extraCtxSize{0};
    int32_t extraCtxRet{ock::mf::BM_OK};

    bool mmapCalled{false};
    int32_t mmapRet{ock::mf::BM_OK};

    bool unmapCalled{false};

    bool sdmaReachable{false};
    hybm_data_op_type reachTypes{HYBM_DOP_TYPE_DEFAULT};

    bool checkAddrResult{false};
};

// ============= hybm_create_entity / hybm_destroy_entity =============

TEST_F(HybmBigMemEntryTest, hybm_create_entity_not_inited)
{
    MOCKER_CPP(HybmHasInited, bool (*)(void)).stubs().will(returnValue(false));

    hybm_options options{};
    auto entity = hybm_create_entity(0, &options, 0);
    EXPECT_EQ(entity, nullptr);
}

TEST_F(HybmBigMemEntryTest, hybm_create_entity_factory_returns_null)
{
    MOCKER_CPP(HybmHasInited, bool (*)(void)).stubs().will(returnValue(true));

    // mock MemEntityFactory::GetOrCreateEngine 返回空
    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(uint16_t, uint32_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, uint16_t, uint32_t);
    } u{};
    u.method = &ock::mf::MemEntityFactory::GetOrCreateEngine;
    MOCKER(u.func).stubs().will(returnValue(ock::mf::EngineImplPtr{}));

    hybm_options options{};
    auto entity = hybm_create_entity(1, &options, 0);
    EXPECT_EQ(entity, nullptr);
}

TEST_F(HybmBigMemEntryTest, hybm_create_entity_initialize_failed)
{
    MOCKER_CPP(HybmHasInited, bool (*)(void)).stubs().will(returnValue(true));

    // 准备 stub 对象
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->initRet = ock::mf::BM_ERROR;

    // GetOrCreateEngine 返回 stub
    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(uint16_t, uint32_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, uint16_t, uint32_t);
    } u{};
    u.method = &ock::mf::MemEntityFactory::GetOrCreateEngine;
    MOCKER(u.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    // RemoveEngine 也 stub 掉，防止真的访问 map
    union {
        bool (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        bool (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } r{};
    r.method = &ock::mf::MemEntityFactory::RemoveEngine;
    MOCKER(r.func).stubs().will(returnValue(true));

    hybm_options options{};
    auto entity = hybm_create_entity(0, &options, 0);
    EXPECT_EQ(entity, nullptr);
    EXPECT_TRUE(stub->initCalled);
}

TEST_F(HybmBigMemEntryTest, hybm_create_and_destroy_entity_success)
{
    MOCKER_CPP(HybmHasInited, bool (*)(void)).stubs().will(returnValue(true));

    auto stub = std::make_shared<MemEntityStub>(0);

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(uint16_t, uint32_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, uint16_t, uint32_t);
    } u{};
    u.method = &ock::mf::MemEntityFactory::GetOrCreateEngine;

    MOCKER(u.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;
    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    union {
        bool (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        bool (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } r{};
    r.method = &ock::mf::MemEntityFactory::RemoveEngine;
    MOCKER(r.func).stubs().will(returnValue(true));

    hybm_options options{};
    auto entity = hybm_create_entity(0, &options, 0);
    ASSERT_NE(entity, nullptr);
    EXPECT_TRUE(stub->initCalled);

    hybm_destroy_entity(entity, 0);
    EXPECT_TRUE(stub->uninitCalled);
}

// ============= hybm_reserve_mem_space / hybm_unreserve_mem_space =============

TEST_F(HybmBigMemEntryTest, hybm_reserve_mem_space_invalid_entity)
{
    auto ret = hybm_reserve_mem_space(nullptr, 0);
    EXPECT_EQ(ret, ock::mf::BM_INVALID_PARAM);
}

TEST_F(HybmBigMemEntryTest, hybm_reserve_mem_space_find_engine_null)
{
    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;
    MOCKER(f.func).stubs().will(returnValue(ock::mf::EngineImplPtr{}));

    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x1);
    auto ret = hybm_reserve_mem_space(fakeEntity, 0);
    EXPECT_EQ(ret, ock::mf::BM_INVALID_PARAM);
}

TEST_F(HybmBigMemEntryTest, hybm_reserve_mem_space_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->reserveRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x2);
    auto ret = hybm_reserve_mem_space(fakeEntity, 0);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(stub->reserveCalled);
}

TEST_F(HybmBigMemEntryTest, hybm_unreserve_mem_space_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->unreserveRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x3);
    auto ret = hybm_unreserve_mem_space(fakeEntity, 0);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(stub->unreserveCalled);
}

// ============= hybm_get_memory_ptr =============

TEST_F(HybmBigMemEntryTest, hybm_get_memory_ptr_null_entity)
{
    auto ptr = hybm_get_memory_ptr(nullptr, HYBM_MEM_TYPE_HOST);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(HybmBigMemEntryTest, hybm_get_memory_ptr_success)
{
    MemEntityStub stub(0);
    stub.reservedPtr = reinterpret_cast<void*>(0xABC);

    auto ptr = hybm_get_memory_ptr(reinterpret_cast<hybm_entity_t>(&stub), HYBM_MEM_TYPE_DEVICE);
    EXPECT_EQ(ptr, stub.reservedPtr);
    EXPECT_EQ(stub.lastMemType, HYBM_MEM_TYPE_DEVICE);
}

// ============= hybm_alloc_local_memory / hybm_free_local_memory / hybm_register_local_memory =============

TEST_F(HybmBigMemEntryTest, hybm_alloc_local_memory_invalid_entity)
{
    auto slice = hybm_alloc_local_memory(nullptr, HYBM_MEM_TYPE_HOST, 1024, 0);
    EXPECT_EQ(slice, nullptr);
}

TEST_F(HybmBigMemEntryTest, hybm_alloc_local_memory_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->allocRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x4);
    auto slice = hybm_alloc_local_memory(fakeEntity, HYBM_MEM_TYPE_HOST, 4096, 1);
    EXPECT_EQ(slice, stub->allocSlice);
    EXPECT_TRUE(stub->allocCalled);
    EXPECT_EQ(stub->allocSize, 4096U);
    EXPECT_EQ(stub->allocMemType, HYBM_MEM_TYPE_HOST);
    EXPECT_EQ(stub->allocFlags, 1U);
}

TEST_F(HybmBigMemEntryTest, hybm_free_local_memory_invalid_params)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->freeRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;
    MOCKER(f.func).stubs().will(returnValue(ock::mf::EngineImplPtr{}));

    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x5);
    auto ret = hybm_free_local_memory(fakeEntity, reinterpret_cast<hybm_mem_slice_t>(0x1), 1, 0);
    EXPECT_EQ(ret, ock::mf::BM_INVALID_PARAM);
}

TEST_F(HybmBigMemEntryTest, hybm_free_local_memory_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->freeRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x6);
    auto slice = reinterpret_cast<hybm_mem_slice_t>(0x111);
    auto ret = hybm_free_local_memory(fakeEntity, slice, 1, 2);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(stub->freeCalled);
    EXPECT_EQ(stub->freeSlice, slice);
    EXPECT_EQ(stub->freeFlags, 2U);
}

TEST_F(HybmBigMemEntryTest, hybm_register_local_memory_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->regRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    int buf = 0;
    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x7);
    auto slice = hybm_register_local_memory(fakeEntity, &buf, 64, 3);
    EXPECT_EQ(slice, stub->regSlice);
    EXPECT_TRUE(stub->regCalled);
    EXPECT_EQ(stub->regPtr, &buf);
    EXPECT_EQ(stub->regSize, 64U);
    EXPECT_EQ(stub->regFlags, 3U);
}

// ============= hybm_export / hybm_export_slice_size / hybm_import =============

TEST_F(HybmBigMemEntryTest, hybm_export_entity_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->exportEntityRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    hybm_exchange_info info{};
    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x8);
    auto ret = hybm_export(fakeEntity, nullptr, HYBM_FLAG_EXPORT_ENTITY, &info);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(stub->exportEntityCalled);
}

TEST_F(HybmBigMemEntryTest, hybm_export_slice_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->exportSliceRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    hybm_exchange_info info{};
    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x9);
    auto slice = reinterpret_cast<hybm_mem_slice_t>(0x123);
    auto ret = hybm_export(fakeEntity, slice, 0, &info);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(stub->exportSliceCalled);
    EXPECT_EQ(stub->exportSlice, slice);
}

TEST_F(HybmBigMemEntryTest, hybm_export_slice_size_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->exportSizeValue = 256;
    stub->exportSizeRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    size_t size = 0;
    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x10);
    auto ret = hybm_export_slice_size(fakeEntity, &size);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(stub->getExportSizeCalled);
    EXPECT_EQ(size, stub->exportSizeValue);
}

TEST_F(HybmBigMemEntryTest, hybm_import_slices_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->importRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    hybm_exchange_info infos[2]{};
    void* addresses[2]{};
    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x11);
    auto ret = hybm_import(fakeEntity, infos, 2, addresses, 0);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(stub->importCalled);
    EXPECT_EQ(stub->importCount, 2U);
}

TEST_F(HybmBigMemEntryTest, hybm_import_entity_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->importEntityRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    hybm_exchange_info infos[1]{};
    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x12);
    auto ret = hybm_import(fakeEntity, infos, 1, nullptr, HYBM_FLAG_EXPORT_ENTITY);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(stub->importEntityCalled);
}

// ============= hybm_mmap / hybm_unmap =============

TEST_F(HybmBigMemEntryTest, hybm_mmap_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->mmapRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x13);
    auto ret = hybm_mmap(fakeEntity, 0);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(stub->mmapCalled);
}

TEST_F(HybmBigMemEntryTest, hybm_unmap_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x14);
    hybm_unmap(fakeEntity, 0);
    EXPECT_TRUE(stub->unmapCalled);
}

// ============= hybm_entity_reach_types / hybm_remove_imported / hybm_set_extra_context =============

TEST_F(HybmBigMemEntryTest, hybm_entity_reach_types_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->reachTypes = static_cast<hybm_data_op_type>(HYBM_DOP_TYPE_SDMA | HYBM_DOP_TYPE_DEVICE_RDMA);

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    hybm_data_op_type reach{};
    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x15);
    auto ret = hybm_entity_reach_types(fakeEntity, 1, reach, 0);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_EQ(reach, stub->reachTypes);
}

TEST_F(HybmBigMemEntryTest, hybm_remove_imported_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->removeImportedRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x16);
    auto ret = hybm_remove_imported(fakeEntity, 3, 0);
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(stub->removeImportedCalled);
    ASSERT_EQ(stub->removedRanks.size(), 1U);
    EXPECT_EQ(stub->removedRanks[0], 3U);
}

TEST_F(HybmBigMemEntryTest, hybm_set_extra_context_success)
{
    auto stub = std::make_shared<MemEntityStub>(0);
    stub->extraCtxRet = ock::mf::BM_OK;

    union {
        ock::mf::EngineImplPtr (ock::mf::MemEntityFactory::*method)(hybm_entity_t);
        ock::mf::EngineImplPtr (*func)(ock::mf::MemEntityFactory*, hybm_entity_t);
    } f{};
    f.method = &ock::mf::MemEntityFactory::FindEngineByPtr;

    MOCKER(f.func).stubs().will(returnValue(std::static_pointer_cast<ock::mf::MemEntityDefault>(stub)));

    int ctx = 123;
    auto fakeEntity = reinterpret_cast<hybm_entity_t>(0x17);
    auto ret = hybm_set_extra_context(fakeEntity, &ctx, sizeof(ctx));
    EXPECT_EQ(ret, ock::mf::BM_OK);
    EXPECT_TRUE(stub->extraCtxCalled);
    EXPECT_EQ(stub->extraCtxPtr, &ctx);
    EXPECT_EQ(stub->extraCtxSize, sizeof(ctx));
}