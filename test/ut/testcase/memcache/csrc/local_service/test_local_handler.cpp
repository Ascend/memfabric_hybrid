/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "mmc_meta_manager.h"
#include "mmc_ref.h"
#include "gtest/gtest.h"
#include <iostream>

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestMmcMetaManager1 : public testing::Test {
public:
    TestMmcMetaManager1();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestMmcMetaManager1::TestMmcMetaManager1() {}

void TestMmcMetaManager1::SetUp()
{
    cout << "this is Meta Manger TEST_F setup:" << std::endl;
}

void TestMmcMetaManager1::TearDown()
{
    cout << "this is Meta Manager TEST_F teardown" << std::endl;
}

TEST_F(TestMmcMetaManager1, Init)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{100, 1000};

    uint64_t defaultTtl = 2000;

    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);
    ASSERT_TRUE(metaMng != nullptr);
}

TEST_F(TestMmcMetaManager1, Alloc)
{
    MmcLocation loc{0, MEDIA_DRAM};
    MmcLocalMemlInitInfo locInfo{0, 1000000};
    uint64_t defaultTtl = 2000;
    MmcRef<MmcMetaManager> metaMng = MmcMakeRef<MmcMetaManager>(defaultTtl);
    metaMng->Mount(loc, locInfo);

    AllocOptions allocReq{SIZE_32K, 1, 0, 0, 0};
    MmcMemObjMetaPtr objMeta;
    Result ret = metaMng->Alloc("test_string", allocReq, 1, objMeta);
    ASSERT_TRUE(ret == MMC_OK);
    ASSERT_TRUE(objMeta->NumBlobs() == 1);
    ASSERT_TRUE(objMeta->Size() == SIZE_32K);
}
