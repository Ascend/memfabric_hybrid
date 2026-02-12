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

#define private public
#define protected public
#include "hybm_entity_default.h"
#include "hybm_mem_segment.h"
#include "hybm_data_operator.h"
#include "hybm_vmm_based_segment.h"
#undef private
#undef protected

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

class HybmEntityDefaultTest : public testing::Test {
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
    EXPECT_EQ(initRet, ock::mf::BM_OK);

    hybm_mem_slice_t slice = nullptr;
    // 测试内存分配（已初始化的情况）size 非法
    auto allocRet = entity.AllocLocalMemory(1024, HYBM_MEM_TYPE_HOST, 0, slice);
    EXPECT_EQ(allocRet, ock::mf::BM_INVALID_PARAM);
    EXPECT_EQ(slice, nullptr);

    // 测试内存分配（已初始化的情况）dramSegment_ 为空
    entity.dramSegment_ = nullptr;
    allocRet = entity.AllocLocalMemory(ock::mf::HYBM_LARGE_PAGE_SIZE, HYBM_MEM_TYPE_HOST, 0, slice);
    EXPECT_EQ(allocRet, ock::mf::BM_INVALID_PARAM);
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
    EXPECT_EQ(reserveRet, ock::mf::BM_NOT_INITIALIZED);

    // 测试内存释放（未初始化的情况）
    auto ret = entity.UnReserveMemorySpace();
    EXPECT_EQ(ret, ock::mf::BM_OK);

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
    EXPECT_EQ(initRet, ock::mf::BM_OK);

    reserveRet = entity.ReserveMemorySpace();
    EXPECT_EQ(reserveRet, ock::mf::BM_OK);

    ret = entity.UnReserveMemorySpace();
    EXPECT_EQ(ret, ock::mf::BM_OK);
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
    EXPECT_EQ(allocRet, ock::mf::BM_NOT_INITIALIZED);
    EXPECT_EQ(slice, nullptr);

    // 测试内存释放（未初始化的情况）
    int32_t freeRet = entity.FreeLocalMemory(slice, 0);
    EXPECT_EQ(freeRet, ock::mf::BM_INVALID_PARAM);

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

    MOCKER_CPP(&ock::mf::MemSegment::InitDeviceInfo,
              int32_t (*)(ock::mf::MemEntityDefault *, int)).stubs().will(returnValue(0));

    int32_t initRet = entity.Initialize(&options);
    EXPECT_EQ(initRet, ock::mf::BM_OK);

    allocRet = entity.AllocLocalMemory(1024, HYBM_MEM_TYPE_HOST, 0, slice);
    EXPECT_EQ(allocRet, ock::mf::BM_INVALID_PARAM);

    allocRet = entity.AllocLocalMemory(ock::mf::HYBM_LARGE_PAGE_SIZE, HYBM_MEM_TYPE_DEVICE, 0, slice);
    EXPECT_EQ(allocRet, ock::mf::BM_INVALID_PARAM);

    freeRet = entity.FreeLocalMemory(slice, 0);
    EXPECT_EQ(freeRet, ock::mf::BM_OK);
}

// 测试 MemEntityDefault 内存注册
TEST_F(HybmEntityDefaultTest, RegisterLocalMemory)
{
    ock::mf::MemEntityDefault entity(500);

    // 测试内存注册（未初始化的情况）
    int buf = 0;
    hybm_mem_slice_t slice;
    auto ret = entity.RegisterLocalMemory(&buf, sizeof(buf), 0, slice);
    EXPECT_EQ(slice, nullptr);
    EXPECT_EQ(ret, ock::mf::BM_INVALID_PARAM);
    ret = entity.RegisterLocalMemory(&buf, 0, 0, slice);
    EXPECT_EQ(slice, nullptr);
    EXPECT_EQ(ret, ock::mf::BM_INVALID_PARAM);

    // 测试初始化
    ock::mf::MemSegmentOptions optionsSeg{};
    optionsSeg.segType = ock::mf::HYBM_MST_DRAM;
    optionsSeg.maxSize = ock::mf::HYBM_LARGE_PAGE_SIZE;
    optionsSeg.rankCnt = 2;
    ock::mf::MemSegmentPtr segment = std::make_shared<ock::mf::HybmVmmBasedSegment>(optionsSeg, entity.id_);

    MOCKER_CPP(&ock::mf::HybmVmmBasedSegment::ReserveMemorySpace,
              int32_t (*)(ock::mf::HybmVmmBasedSegment *, void **)).stubs().will(returnValue(0));

    hybm_options options{};
    options.rankId = 0;
    options.rankCount = 2;
    options.memType = HYBM_MEM_TYPE_HOST;
    entity.dramSegment_ = segment;

    int32_t initRet = entity.Initialize(&options);
    EXPECT_EQ(initRet, ock::mf::BM_OK);

    ret = entity.RegisterLocalMemory(&buf, sizeof(buf), 0, slice);
    EXPECT_EQ(ret, ock::mf::BM_UNDER_API_UNLOAD);
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
    EXPECT_EQ(exportRet, ock::mf::BM_NOT_INITIALIZED);

    // 测试导出切片信息（未初始化的情况）
    exportRet = entity.ExportExchangeInfo(nullptr, writer, 0);
    EXPECT_EQ(exportRet, ock::mf::BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 导入交换信息
TEST_F(HybmEntityDefaultTest, ImportExchangeInfo)
{
    ock::mf::MemEntityDefault entity(100);
    hybm_exchange_info info{};
    void *addresses[1] = {nullptr};

    // 测试导入切片信息（未初始化的情况）
    int32_t importRet = entity.ImportExchangeInfo(&info, 1, addresses, 0);
    EXPECT_EQ(importRet, ock::mf::BM_NOT_INITIALIZED);

    // 测试初始化
    hybm_options options{};
    options.rankId = 0;
    options.rankCount = 1;
    int32_t initRet = entity.Initialize(&options);
    EXPECT_EQ(initRet, ock::mf::BM_OK);

    MOCKER_CPP(&ock::mf::MemEntityDefault::SetThreadAclDevice,
              int32_t (*)(ock::mf::MemEntityDefault *)).stubs().will(returnValue(0));

    importRet = entity.ImportExchangeInfo(&info, 1, addresses, 0);
    EXPECT_EQ(importRet, ock::mf::BM_OK);

    entity.UnInitialize();
    EXPECT_FALSE(entity.initialized_);
}

// 测试 MemEntityDefault 获取导出切片信息大小
TEST_F(HybmEntityDefaultTest, GetExportSliceInfoSize)
{
    ock::mf::MemEntityDefault entity(800);
    size_t size = 0;

    // 测试获取导出切片信息大小（未初始化的情况）
    int32_t getSizeRet = entity.GetExportSliceInfoSize(size);
    EXPECT_EQ(getSizeRet, ock::mf::BM_ERROR);
    EXPECT_EQ(size, 0);
}

// 测试 MemEntityDefault 移除导入的内存
TEST_F(HybmEntityDefaultTest, RemoveImported)
{
    ock::mf::MemEntityDefault entity(900);
    std::vector<uint32_t> ranks = {1, 2, 3};

    // 测试移除导入的内存（未初始化的情况）
    int32_t removeRet = entity.RemoveImported(ranks);
    EXPECT_EQ(removeRet, ock::mf::BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 设置额外上下文
TEST_F(HybmEntityDefaultTest, SetExtraContext)
{
    ock::mf::MemEntityDefault entity(1000);
    int ctx = 123;

    // 测试设置额外上下文（未初始化的情况）
    int32_t setCtxRet = entity.SetExtraContext(&ctx, sizeof(ctx));
    EXPECT_EQ(setCtxRet, ock::mf::BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 内存映射和解除映射
TEST_F(HybmEntityDefaultTest, Mmap_Unmap)
{
    ock::mf::MemEntityDefault entity(1100);

    // 测试内存映射（未初始化的情况）
    int32_t mmapRet = entity.Mmap();
    EXPECT_EQ(mmapRet, ock::mf::BM_NOT_INITIALIZED);

    // 测试内存解除映射（未初始化的情况）
    entity.Unmap();
}

// 测试 MemEntityDefault 地址检查
TEST_F(HybmEntityDefaultTest, CheckAddressInEntity)
{
    ock::mf::MemEntityDefault entity(1200);

    // 测试地址检查（未初始化的情况）
    int buf = 0;
    bool inEntity = entity.CheckAddressInEntity(&buf, sizeof(buf));
    EXPECT_FALSE(inEntity);
}

// 测试 MemEntityDefault 数据复制
TEST_F(HybmEntityDefaultTest, CopyData)
{
    ock::mf::MemEntityDefault entity(1300);
    hybm_copy_params params{};

    // 测试数据复制（未初始化的情况）
    int32_t copyRet = entity.CopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(copyRet, ock::mf::BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 批量数据复制
TEST_F(HybmEntityDefaultTest, BatchCopyData)
{
    ock::mf::MemEntityDefault entity(1400);
    hybm_batch_copy_params params{};

    // 测试批量数据复制（未初始化的情况）
    int32_t batchCopyRet = entity.BatchCopyData(params, HYBM_LOCAL_HOST_TO_GLOBAL_HOST, nullptr, 0);
    EXPECT_EQ(batchCopyRet, ock::mf::BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault 等待操作
TEST_F(HybmEntityDefaultTest, Wait)
{
    ock::mf::MemEntityDefault entity(1500);

    // 测试等待操作（未初始化的情况）
    int32_t waitRet = entity.Wait();
    EXPECT_EQ(waitRet, ock::mf::BM_NOT_INITIALIZED);
}

// 测试 MemEntityDefault SDMA 可达性检查
TEST_F(HybmEntityDefaultTest, SdmaReaches)
{
    ock::mf::MemEntityDefault entity(1600);

    // 测试 SDMA 可达性检查（未初始化的情况）
    bool reaches = entity.SdmaReaches(1);
    EXPECT_FALSE(reaches);
}

// 测试 MemEntityDefault 数据操作类型检查
TEST_F(HybmEntityDefaultTest, CanReachDataOperators)
{
    ock::mf::MemEntityDefault entity(1700);

    // 测试数据操作类型检查（未初始化的情况）
    hybm_data_op_type opType = entity.CanReachDataOperators(1);
    EXPECT_EQ(opType, HYBM_DOP_TYPE_DEFAULT);
}

// 测试 MemEntityDefault 获取切片虚拟地址
TEST_F(HybmEntityDefaultTest, GetSliceVa)
{
    ock::mf::MemEntityDefault entity(1800);

    // 测试获取切片虚拟地址（未初始化的情况）
    void *va = entity.GetSliceVa(nullptr);
    EXPECT_EQ(va, nullptr);
}

// 测试 MemEntityDefault 参数检查
TEST_F(HybmEntityDefaultTest, CheckOptions)
{
    // 测试空选项
    int32_t ret = ock::mf::MemEntityDefault::CheckOptions(nullptr);
    EXPECT_EQ(ret, ock::mf::BM_INVALID_PARAM);

    // 测试有效的基本选项
    hybm_options options{};
    options.rankId = 0;
    options.rankCount = 1;
    ret = ock::mf::MemEntityDefault::CheckOptions(&options);
    EXPECT_EQ(ret, ock::mf::BM_OK);
}

// 测试 MemEntityDefault 功能修改拦截
TEST_F(HybmEntityDefaultTest, MemEntityDefault_FunctionModification_Intercept)
{
    ock::mf::MemEntityDefault entity(1900);
    hybm_options options{};
    options.rankId = 0;
    options.rankCount = 1;

    // 测试参数检查的一致性
    int32_t ret1 = ock::mf::MemEntityDefault::CheckOptions(&options);
    int32_t ret2 = ock::mf::MemEntityDefault::CheckOptions(&options);
    EXPECT_EQ(ret1, ret2);
    EXPECT_EQ(ret1, ock::mf::BM_OK);

    // 测试空参数检查的一致性
    int32_t nullRet1 = ock::mf::MemEntityDefault::CheckOptions(nullptr);
    int32_t nullRet2 = ock::mf::MemEntityDefault::CheckOptions(nullptr);
    EXPECT_EQ(nullRet1, nullRet2);
    EXPECT_EQ(nullRet1, ock::mf::BM_INVALID_PARAM);
}
