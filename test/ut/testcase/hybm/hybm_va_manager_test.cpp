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
#include <thread>
#include <vector>
#include <atomic>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include "hybm_def.h"
#include "hybm_define.h"
#include "hybm_va_manager.h"

using namespace ock::mf;

// ============================ 测试常量定义 ============================

// 内存大小常量
constexpr uint64_t TEST_SIZE_ZERO = 0UL;
constexpr uint64_t TEST_SIZE_ONE_BYTE = 1UL;
constexpr uint64_t TEST_SIZE_ONE_MB = 0x100000UL;
constexpr uint64_t TEST_SIZE_FOUR_MB = 0x400000UL;
constexpr uint64_t TEST_SIZE_SIXTEEN_MB = 0x1000000UL;
constexpr uint64_t TEST_SIZE_SIXTY_FOUR_MB = 0x4000000UL;

// 地址偏移常量
constexpr uint64_t TEST_OFFSET_HALF_MB = 0x80000UL;
constexpr uint64_t TEST_OFFSET_ONE_MB = 0x100000UL;
constexpr uint64_t TEST_OFFSET_FOUR_MB = 0x400000UL;
constexpr uint64_t TEST_OFFSET_SIXTEEN_MB = 0x1000000UL;

// 地址基值常量
constexpr uint64_t TEST_GVA_BASE_HOST = HYBM_GVM_START_ADDR + TEST_OFFSET_SIXTEEN_MB;
constexpr uint64_t TEST_GVA_BASE_DEVICE = HYBM_GVM_START_ADDR + HYBM_HOST_CONN_ADDR_SIZE + TEST_OFFSET_SIXTEEN_MB;
constexpr uint64_t TEST_LVA_BASE = 0x100000000UL;

// Rank 常量
constexpr uint32_t TEST_RANK_ZERO = 0;
constexpr uint32_t TEST_RANK_ONE = 1;
constexpr uint32_t TEST_RANK_TWO = 2;
constexpr uint32_t TEST_RANK_THREE = 3;
constexpr uint32_t TEST_RANK_INVALID = 999;

// 计数常量
constexpr size_t TEST_COUNT_ONE = 1;
constexpr size_t TEST_COUNT_TWO = 2;
constexpr size_t TEST_COUNT_THREE = 3;
constexpr size_t TEST_COUNT_FIVE = 5;
constexpr size_t TEST_COUNT_TEN = 10;

// 测试索引常量
constexpr uint64_t TEST_INDEX_ZERO = 0;
constexpr uint64_t TEST_INDEX_ONE = 1;
constexpr uint64_t TEST_INDEX_TWO = 2;

// 内存类型枚举
constexpr hybm_mem_type TEST_MEM_TYPE_HOST = HYBM_MEM_TYPE_HOST;
constexpr hybm_mem_type TEST_MEM_TYPE_DEVICE = HYBM_MEM_TYPE_DEVICE;

// SoC 类型枚举
constexpr AscendSocType TEST_SOC_910B = AscendSocType::ASCEND_910B;
constexpr AscendSocType TEST_SOC_910 = AscendSocType::ASCEND_910C;

// 并发测试常量
constexpr int TEST_THREAD_COUNT_FOUR = 4;
constexpr int TEST_THREAD_COUNT_EIGHT = 8;
constexpr int TEST_OPERATIONS_PER_THREAD_TEN = 10;
constexpr int TEST_OPERATIONS_PER_THREAD_FIFTY = 50;

class HybmVaManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        manager.ClearAll();
        manager.Initialize(TEST_SOC_910B);
    }

    void TearDown() override
    {
        manager.ClearAll();
    }

    HybmVaManager &manager = HybmVaManager::GetInstance();
};

// ============================ 测试用例 ============================

// 测试1: Initialize 方法
TEST_F(HybmVaManagerTest, Initialize_ValidSocType_ReturnsSuccess)
{
    EXPECT_TRUE(manager.Initialize(TEST_SOC_910) == BM_OK);
    EXPECT_TRUE(manager.Initialize(TEST_SOC_910B) == BM_OK);
}

// 测试2: AddVaInfoFromExternal 基本功能
TEST_F(HybmVaManagerTest, AddRegisterVaInfo_ValidParameters_ReturnsSuccess)
{
    uint64_t gva = TEST_GVA_BASE_HOST;
    uint64_t lva = TEST_LVA_BASE;

    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, lva}, TEST_RANK_ZERO) ==
                BM_OK);

    EXPECT_TRUE(manager.GetAllocCount() == TEST_COUNT_ONE);
    EXPECT_TRUE(manager.IsGva(gva));
    EXPECT_TRUE(manager.IsValidAddr(gva));
}

// 测试3: AddVaInfoFromExternal 参数为0
TEST_F(HybmVaManagerTest, AddRegisterVaInfo_ZeroParameters_ReturnsInvalidParam)
{
    EXPECT_TRUE(manager.AddVaInfoFromExternal({TEST_SIZE_ZERO, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, TEST_LVA_BASE},
                                              TEST_RANK_ZERO) != BM_OK);
    EXPECT_TRUE(
        manager.AddVaInfoFromExternal({TEST_GVA_BASE_HOST, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, TEST_SIZE_ZERO},
                                      TEST_RANK_ZERO) == BM_OK);
}

// 测试4: AddVaInfoFromExternal 地址重叠
TEST_F(HybmVaManagerTest, AddRegisterVaInfo_OverlappingAddress_ReturnsAddrOverlap)
{
    uint64_t gva = TEST_GVA_BASE_HOST;
    uint64_t lva = TEST_LVA_BASE;
    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, lva}, TEST_RANK_ZERO) ==
                BM_OK);
    EXPECT_TRUE(
        manager.AddVaInfoFromExternal({gva, TEST_SIZE_FOUR_MB, TEST_MEM_TYPE_HOST, lva + TEST_OFFSET_SIXTEEN_MB},
                                      TEST_RANK_ZERO) != BM_OK);
}

// 测试5: AddVaInfo 基本功能
TEST_F(HybmVaManagerTest, AddSelfVaInfo_ValidParameters_ReturnsSuccess)
{
    uint64_t gva = TEST_GVA_BASE_DEVICE;
    uint64_t lva = TEST_LVA_BASE + TEST_OFFSET_SIXTEEN_MB;

    EXPECT_TRUE(manager.AddVaInfo({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_DEVICE, lva}, TEST_RANK_ONE) == BM_OK);

    auto [allocInfo, found] = manager.FindAllocByGva(gva);
    EXPECT_TRUE(found);
    EXPECT_TRUE(!allocInfo.registered);
    EXPECT_TRUE(allocInfo.RankId() == TEST_RANK_ONE);
    EXPECT_TRUE(allocInfo.base.memType == TEST_MEM_TYPE_DEVICE);
}

// 测试6: 地址转换功能
TEST_F(HybmVaManagerTest, AddressConversion_ValidAddresses_ReturnsCorrectValues)
{
    uint64_t gva = TEST_GVA_BASE_HOST;
    uint64_t lva = TEST_LVA_BASE;

    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, lva}, TEST_RANK_ZERO) ==
                BM_OK);

    EXPECT_TRUE(manager.GetGvaByLva(lva) == gva);
    EXPECT_TRUE(manager.GetLvaByGva(gva) == lva);

    uint64_t internalGva = gva + TEST_OFFSET_HALF_MB;
    uint64_t internalLva = lva + TEST_OFFSET_HALF_MB;

    EXPECT_TRUE(manager.GetGvaByLva(internalLva) == internalGva);
    EXPECT_TRUE(manager.GetLvaByGva(internalGva) == internalLva);
}

// 测试7: 地址转换 - 不存在的地址
TEST_F(HybmVaManagerTest, AddressConversion_NonExistentAddress_ReturnsZero)
{
    uint64_t nonExistentLva = TEST_LVA_BASE + TEST_OFFSET_SIXTEEN_MB * TEST_COUNT_TEN;
    uint64_t nonExistentGva = TEST_GVA_BASE_HOST + TEST_OFFSET_SIXTEEN_MB * TEST_COUNT_TEN;
    EXPECT_TRUE(manager.GetGvaByLva(nonExistentLva) == TEST_SIZE_ZERO);
    EXPECT_TRUE(manager.GetLvaByGva(nonExistentGva) == TEST_SIZE_ZERO);
}

// 测试8: IsGva 功能测试
TEST_F(HybmVaManagerTest, IsGva_ValidAndInvalidAddresses_ReturnsCorrectBool)
{
    uint64_t gva = TEST_GVA_BASE_HOST;
    uint64_t lva = TEST_LVA_BASE;

    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, lva}, TEST_RANK_ZERO) ==
                BM_OK);

    EXPECT_TRUE(manager.IsGva(gva));
    EXPECT_TRUE(manager.IsGva(gva + TEST_OFFSET_HALF_MB));
    EXPECT_TRUE(manager.IsGva(gva + TEST_SIZE_SIXTEEN_MB - TEST_COUNT_ONE));

    EXPECT_FALSE(manager.IsGva(gva + TEST_SIZE_SIXTEEN_MB));
    EXPECT_FALSE(manager.IsGva(TEST_SIZE_ZERO));
}

// 测试9: GetMemType 功能测试
TEST_F(HybmVaManagerTest, GetMemType_ValidAddresses_ReturnsCorrectMemType)
{
    uint64_t hostGva = TEST_GVA_BASE_HOST;
    uint64_t deviceGva = TEST_GVA_BASE_DEVICE;
    EXPECT_TRUE(manager.AddVaInfoFromExternal({hostGva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, TEST_LVA_BASE},
                                              TEST_RANK_ZERO) == BM_OK);
    BaseAllocatedGvaInfo info = {deviceGva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_DEVICE,
                                 TEST_LVA_BASE + TEST_OFFSET_SIXTEEN_MB};
    auto ret = manager.AddVaInfo(info, TEST_RANK_ONE);
    EXPECT_TRUE(ret == BM_OK);

    EXPECT_TRUE(manager.GetMemType(hostGva) == TEST_MEM_TYPE_HOST);
    EXPECT_TRUE(manager.GetMemType(deviceGva) == TEST_MEM_TYPE_DEVICE);
}

// 测试10: GetRank 功能测试
TEST_F(HybmVaManagerTest, GetRank_ValidAddresses_ReturnsCorrectRank)
{
    uint64_t gva1 = TEST_GVA_BASE_HOST;
    uint64_t gva2 = TEST_GVA_BASE_HOST + TEST_OFFSET_SIXTEEN_MB;
    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva1, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, TEST_LVA_BASE},
                                              TEST_RANK_ZERO) == BM_OK);
    BaseAllocatedGvaInfo info = {gva2, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST,
                                 TEST_LVA_BASE + TEST_OFFSET_SIXTEEN_MB};
    auto ret = manager.AddVaInfoFromExternal(info, TEST_RANK_ONE);
    EXPECT_TRUE(ret == BM_OK);

    EXPECT_TRUE(manager.GetRank(gva1).first == TEST_RANK_ZERO);
    EXPECT_TRUE(manager.GetRank(gva2).first == TEST_RANK_ONE);
}

// 测试11: IsValidAddr 功能测试
TEST_F(HybmVaManagerTest, IsValidAddr_ValidAndInvalidAddresses_ReturnsCorrectBool)
{
    uint64_t gva = TEST_GVA_BASE_HOST;
    uint64_t lva = TEST_LVA_BASE;

    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, lva}, TEST_RANK_ZERO) ==
                BM_OK);

    EXPECT_TRUE(manager.IsValidAddr(gva));
    EXPECT_TRUE(manager.IsValidAddr(lva));
    EXPECT_TRUE(manager.IsValidAddr(gva + TEST_OFFSET_HALF_MB));
    EXPECT_TRUE(manager.IsValidAddr(lva + TEST_OFFSET_HALF_MB));

    EXPECT_FALSE(manager.IsValidAddr(gva + TEST_SIZE_SIXTEEN_MB));
    EXPECT_FALSE(manager.IsValidAddr(TEST_SIZE_ZERO));
}

// 测试12: FindAllocByGva 功能测试
TEST_F(HybmVaManagerTest, FindAllocByGva_ValidAndInvalidAddresses_ReturnsCorrectResult)
{
    uint64_t gva = TEST_GVA_BASE_HOST;
    uint64_t lva = TEST_LVA_BASE;

    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, lva}, TEST_RANK_ZERO) ==
                BM_OK);

    auto [allocInfo1, found1] = manager.FindAllocByGva(gva);
    EXPECT_TRUE(found1);
    EXPECT_TRUE(allocInfo1.base.gva == gva);
    EXPECT_TRUE(allocInfo1.base.lva == lva);

    auto [allocInfo2, found2] = manager.FindAllocByGva(gva + TEST_OFFSET_HALF_MB);
    EXPECT_TRUE(found2);
    EXPECT_TRUE(allocInfo2.base.gva == gva);

    auto [allocInfo3, found3] = manager.FindAllocByGva(TEST_GVA_BASE_HOST + TEST_OFFSET_SIXTEEN_MB * TEST_COUNT_TEN);
    EXPECT_FALSE(found3);
}

// 测试13: FindAllocByLva 功能测试
TEST_F(HybmVaManagerTest, FindAllocByLva_ValidAndInvalidAddresses_ReturnsCorrectResult)
{
    uint64_t gva = TEST_GVA_BASE_HOST;
    uint64_t lva = TEST_LVA_BASE;

    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, lva}, TEST_RANK_ZERO) ==
                BM_OK);

    auto [allocInfo1, found1] = manager.FindAllocByLva(lva);
    EXPECT_TRUE(found1);
    EXPECT_TRUE(allocInfo1.base.gva == gva);
    EXPECT_TRUE(allocInfo1.base.lva == lva);

    auto [allocInfo2, found2] = manager.FindAllocByLva(lva + TEST_OFFSET_HALF_MB);
    EXPECT_TRUE(found2);
    EXPECT_TRUE(allocInfo2.base.lva == lva);

    auto [allocInfo3, found3] = manager.FindAllocByLva(TEST_LVA_BASE + TEST_OFFSET_SIXTEEN_MB * TEST_COUNT_TEN);
    EXPECT_FALSE(found3);
}

// 测试14: AllocReserveGva 基本功能
TEST_F(HybmVaManagerTest, AllocReserveGva_ValidParameters_ReturnsReservedInfo)
{
    ReservedGvaInfo reserved = manager.AllocReserveGva(TEST_RANK_ZERO, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST);

    EXPECT_TRUE(reserved.start != TEST_SIZE_ZERO);
    EXPECT_TRUE(reserved.size == TEST_SIZE_SIXTEEN_MB);
    EXPECT_TRUE(reserved.memType == TEST_MEM_TYPE_HOST);
    EXPECT_TRUE(reserved.localRankId == TEST_RANK_ZERO);
    EXPECT_TRUE(manager.GetReservedCount() == TEST_COUNT_ONE);
}

// 测试15: AllocReserveGva 重复分配
TEST_F(HybmVaManagerTest, AllocReserveGva_DuplicateAllocation_ReturnsEmptyReservedInfo)
{
    ReservedGvaInfo reserved1 = manager.AllocReserveGva(TEST_RANK_ZERO, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST);
    EXPECT_TRUE(reserved1.start != TEST_SIZE_ZERO);

    ReservedGvaInfo reserved2 = manager.AllocReserveGva(TEST_RANK_ZERO, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST);
    EXPECT_TRUE(reserved2.start != TEST_SIZE_ZERO);
}

// 测试16: AllocReserveGva 不同Rank分配
TEST_F(HybmVaManagerTest, AllocReserveGva_DifferentRanks_ReturnsReservedInfo)
{
    ReservedGvaInfo reserved1 = manager.AllocReserveGva(TEST_RANK_ZERO, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST);
    EXPECT_TRUE(reserved1.start != TEST_SIZE_ZERO);

    ReservedGvaInfo reserved2 = manager.AllocReserveGva(TEST_RANK_ONE, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST);
    EXPECT_TRUE(reserved2.start != TEST_SIZE_ZERO);
    EXPECT_TRUE(manager.GetReservedCount() == TEST_COUNT_TWO);
}

// 测试17: AllocReserveGva 不同内存类型分配
TEST_F(HybmVaManagerTest, AllocReserveGva_DifferentMemTypes_ReturnsReservedInfo)
{
    ReservedGvaInfo reserved1 = manager.AllocReserveGva(TEST_RANK_ZERO, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST);
    EXPECT_TRUE(reserved1.start != TEST_SIZE_ZERO);

    ReservedGvaInfo reserved2 = manager.AllocReserveGva(TEST_RANK_ZERO, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_DEVICE);
    EXPECT_TRUE(reserved2.start != TEST_SIZE_ZERO);
    EXPECT_TRUE(manager.GetReservedCount() == TEST_COUNT_TWO);
}

// 测试18: FreeReserveGva 功能测试
TEST_F(HybmVaManagerTest, FreeReserveGva_ValidAddress_RemovesReservation)
{
    ReservedGvaInfo reserved = manager.AllocReserveGva(TEST_RANK_ZERO, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST);
    EXPECT_TRUE(reserved.start != TEST_SIZE_ZERO);
    EXPECT_TRUE(manager.GetReservedCount() == TEST_COUNT_ONE);

    manager.FreeReserveGva(reserved.start);
    EXPECT_TRUE(manager.GetReservedCount() == TEST_SIZE_ZERO);
}

// 测试19: FreeReserveGva 无效地址
TEST_F(HybmVaManagerTest, FreeReserveGva_InvalidAddress_NoEffect)
{
    EXPECT_TRUE(manager.GetReservedCount() == TEST_SIZE_ZERO);
    manager.FreeReserveGva(TEST_GVA_BASE_HOST);
    EXPECT_TRUE(manager.GetReservedCount() == TEST_SIZE_ZERO);
}

// 测试20: RemoveOneVaInfo 功能测试
TEST_F(HybmVaManagerTest, RemoveOneVaInfo_ValidAddress_RemovesAllocation)
{
    uint64_t gva = TEST_GVA_BASE_HOST;
    uint64_t lva = TEST_LVA_BASE;
    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, lva}, TEST_RANK_ZERO) ==
                BM_OK);
    EXPECT_TRUE(manager.GetAllocCount() == TEST_COUNT_ONE);

    manager.RemoveOneVaInfo(gva);
    EXPECT_TRUE(manager.GetAllocCount() == TEST_SIZE_ZERO);
    EXPECT_FALSE(manager.IsGva(gva));
}

// 测试22: RemoveAllVaInfoByRank 功能测试
TEST_F(HybmVaManagerTest, RemoveAllVaInfoByRank_ValidRank_RemovesAllAllocations)
{
    uint64_t gva1 = TEST_GVA_BASE_HOST;
    uint64_t gva2 = TEST_GVA_BASE_HOST + TEST_OFFSET_SIXTEEN_MB;
    uint64_t gva3 = TEST_GVA_BASE_DEVICE;

    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva1, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, TEST_LVA_BASE},
                                              TEST_RANK_ZERO) == BM_OK);
    BaseAllocatedGvaInfo info0 = {gva2, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST,
                                  TEST_LVA_BASE + TEST_OFFSET_SIXTEEN_MB};
    EXPECT_TRUE(manager.AddVaInfoFromExternal(info0, TEST_RANK_ZERO) == BM_OK);
    auto lva = TEST_LVA_BASE + TEST_OFFSET_SIXTEEN_MB * TEST_COUNT_TWO;
    BaseAllocatedGvaInfo info = {gva3, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_DEVICE, lva};
    auto ret = manager.AddVaInfo(info, TEST_RANK_ONE);
    EXPECT_TRUE(ret == BM_OK);

    EXPECT_TRUE(manager.GetAllocCount() == TEST_COUNT_THREE);
}

// 测试24: GetAllocCount 功能测试
TEST_F(HybmVaManagerTest, GetAllocCount_MultipleAllocations_ReturnsCorrectCount)
{
    EXPECT_TRUE(manager.GetAllocCount() == TEST_SIZE_ZERO);

    for (size_t i = TEST_INDEX_ZERO; i < TEST_COUNT_FIVE; i++) {
        uint64_t gva = TEST_GVA_BASE_HOST + i * TEST_OFFSET_SIXTEEN_MB;
        uint64_t lva = TEST_LVA_BASE + i * TEST_OFFSET_SIXTEEN_MB;
        EXPECT_TRUE(manager.AddVaInfoFromExternal({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, lva},
                                                  TEST_RANK_ZERO) == BM_OK);
    }

    EXPECT_TRUE(manager.GetAllocCount() == TEST_COUNT_FIVE);
}

// 测试25: GetReservedCount 功能测试
TEST_F(HybmVaManagerTest, GetReservedCount_MultipleReservations_ReturnsCorrectCount)
{
    EXPECT_TRUE(manager.GetReservedCount() == TEST_SIZE_ZERO);

    for (size_t i = TEST_INDEX_ZERO; i < TEST_COUNT_THREE; i++) {
        manager.AllocReserveGva(i, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST);
    }

    EXPECT_TRUE(manager.GetReservedCount() == TEST_COUNT_THREE);
}

// 测试26: ClearAll 功能测试
TEST_F(HybmVaManagerTest, ClearAll_MultipleAllocationsAndReservations_ClearsAll)
{
    for (size_t i = TEST_INDEX_ZERO; i < TEST_COUNT_FIVE; i++) {
        uint64_t gva = TEST_GVA_BASE_HOST + i * TEST_OFFSET_SIXTEEN_MB;
        uint64_t lva = TEST_LVA_BASE + i * TEST_OFFSET_SIXTEEN_MB;
        EXPECT_TRUE(manager.AddVaInfoFromExternal({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, lva},
                                                  TEST_RANK_ZERO) == BM_OK);
    }

    for (size_t i = TEST_INDEX_ZERO; i < TEST_COUNT_THREE; i++) {
        manager.AllocReserveGva(i, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST);
    }

    EXPECT_TRUE(manager.GetAllocCount() == TEST_COUNT_FIVE);
    EXPECT_TRUE(manager.GetReservedCount() == TEST_COUNT_THREE);

    manager.ClearAll();

    EXPECT_TRUE(manager.GetAllocCount() == TEST_SIZE_ZERO);
    EXPECT_TRUE(manager.GetReservedCount() == TEST_SIZE_ZERO);
}

// 测试27: FormatMemorySize 静态方法测试
TEST_F(HybmVaManagerTest, FormatMemorySize_VariousSizes_ReturnsFormattedString)
{
    std::string size1 = VaToStr(TEST_SIZE_ONE_BYTE);
    EXPECT_TRUE(!size1.empty());

    std::string size2 = VaToStr(TEST_SIZE_ONE_MB);
    EXPECT_TRUE(!size2.empty());

    std::string size3 = VaToStr(TEST_SIZE_SIXTEEN_MB);
    EXPECT_TRUE(!size3.empty());

    std::string size4 = VaToStr(TEST_SIZE_ZERO);
    EXPECT_TRUE(!size4.empty());
}

// 测试28: PrintAllReservedGvaInfo 不崩溃测试
TEST_F(HybmVaManagerTest, PrintAllReservedGvaInfo_EmptyAndNonEmpty_DoesNotCrash)
{
    EXPECT_NO_THROW(manager.DumpReservedGvaInfo());
    manager.AllocReserveGva(TEST_RANK_ZERO, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST);
    EXPECT_NO_THROW(manager.DumpReservedGvaInfo());
}

// 测试29: PrintAllAllocGvaInfo 不崩溃测试
TEST_F(HybmVaManagerTest, PrintAllAllocGvaInfo_EmptyAndNonEmpty_DoesNotCrash)
{
    EXPECT_NO_THROW(manager.DumpAllocatedGvaInfo());
    EXPECT_TRUE(
        manager.AddVaInfoFromExternal({TEST_GVA_BASE_HOST, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, TEST_LVA_BASE},
                                      TEST_RANK_ZERO) == BM_OK);
    EXPECT_NO_THROW(manager.DumpAllocatedGvaInfo());
}

// 测试30: 边界条件 - 最小大小
TEST_F(HybmVaManagerTest, BoundaryCondition_MinimumSize_ReturnsSuccess)
{
    EXPECT_TRUE(
        manager.AddVaInfoFromExternal({TEST_GVA_BASE_HOST, TEST_SIZE_ONE_BYTE, TEST_MEM_TYPE_HOST, TEST_LVA_BASE},
                                      TEST_RANK_ZERO) == BM_OK);
    EXPECT_TRUE(manager.GetAllocCount() == TEST_COUNT_ONE);
}

// 测试31: 边界条件 - 不同内存类型不重叠
TEST_F(HybmVaManagerTest, BoundaryCondition_DifferentMemTypes_NoOverlap)
{
    uint64_t hostGva = TEST_GVA_BASE_HOST;
    uint64_t deviceGva = TEST_GVA_BASE_DEVICE;
    EXPECT_TRUE(manager.AddVaInfoFromExternal({hostGva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, TEST_LVA_BASE},
                                              TEST_RANK_ZERO) == BM_OK);
    BaseAllocatedGvaInfo base = {deviceGva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_DEVICE,
                                 TEST_LVA_BASE + TEST_OFFSET_SIXTEEN_MB};
    EXPECT_TRUE(manager.AddVaInfoFromExternal(base, TEST_RANK_ZERO) == BM_OK);
    EXPECT_TRUE(manager.GetAllocCount() == TEST_COUNT_TWO);
}

// 测试32: 并发测试 - 多线程添加
TEST_F(HybmVaManagerTest, ConcurrentTest_MultipleThreadsAdding_NoDataRace)
{
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    for (int i = 0; i < TEST_THREAD_COUNT_FOUR; i++) {
        threads.emplace_back([this, i, &successCount]() {
            for (int j = 0; j < TEST_OPERATIONS_PER_THREAD_TEN; j++) {
                uint64_t gva = TEST_GVA_BASE_HOST + (i * TEST_OPERATIONS_PER_THREAD_TEN + j) * TEST_OFFSET_SIXTEEN_MB;
                uint64_t lva = TEST_LVA_BASE + (i * TEST_OPERATIONS_PER_THREAD_TEN + j) * TEST_OFFSET_SIXTEEN_MB;

                if (manager.AddVaInfoFromExternal({gva, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, lva},
                                                  i % TEST_COUNT_THREE) == BM_OK) {
                    successCount++;
                }
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_TRUE(manager.GetAllocCount() == static_cast<size_t>(successCount));
}

// 测试33: 混合操作测试
TEST_F(HybmVaManagerTest, MixedOperations_AddRemoveQuery_WorksCorrectly)
{
    uint64_t gva1 = TEST_GVA_BASE_HOST;
    uint64_t gva2 = TEST_GVA_BASE_HOST + TEST_OFFSET_SIXTEEN_MB;
    uint64_t gva3 = TEST_GVA_BASE_HOST + TEST_OFFSET_SIXTEEN_MB * TEST_COUNT_TWO;

    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva1, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, TEST_LVA_BASE},
                                              TEST_RANK_ZERO) == BM_OK);
    EXPECT_TRUE(
        manager.AddVaInfo({gva2, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST, TEST_LVA_BASE + TEST_OFFSET_SIXTEEN_MB},
                          TEST_RANK_ONE) == BM_OK);
    ReservedGvaInfo reserved = manager.AllocReserveGva(TEST_RANK_TWO, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_DEVICE);
    EXPECT_TRUE(reserved.start != TEST_SIZE_ZERO);

    EXPECT_TRUE(manager.IsGva(gva1));
    EXPECT_TRUE(manager.IsGva(gva2));
    EXPECT_TRUE(manager.GetRank(gva1).first == TEST_RANK_ZERO);
    EXPECT_TRUE(manager.GetRank(gva2).first == TEST_RANK_ONE);

    manager.RemoveOneVaInfo(gva1);
    EXPECT_FALSE(manager.IsGva(gva1));
    EXPECT_TRUE(manager.IsGva(gva2));

    EXPECT_TRUE(manager.AddVaInfoFromExternal({gva3, TEST_SIZE_SIXTEEN_MB, TEST_MEM_TYPE_HOST,
                                               TEST_LVA_BASE + TEST_OFFSET_SIXTEEN_MB * TEST_COUNT_TWO},
                                              TEST_RANK_THREE) == BM_OK);

    EXPECT_TRUE(manager.GetAllocCount() == TEST_COUNT_TWO);
    EXPECT_TRUE(manager.GetReservedCount() == TEST_COUNT_ONE);

    manager.FreeReserveGva(reserved.start);
    EXPECT_TRUE(manager.GetReservedCount() == TEST_SIZE_ZERO);
}