/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include <iostream>
#include <thread>
#include <vector>
#include "gtest/gtest.h"
#include "mmc_read_write_lock.h"
#include "config/mmc_functions.h"
#include "mmc_ref.h"

using namespace testing;
using namespace std;
using namespace ock::mmc;

class TestFunctions : public testing::Test {
public:
    TestFunctions();

    void SetUp() override;

    void TearDown() override;

protected:
};
TestFunctions::TestFunctions() {}

void TestFunctions::SetUp() {}

void TestFunctions::TearDown() {}

TEST_F(TestFunctions, test_str_utils)
{
    uint64_t uint64{0};
    float aFloat{0};
    bool aBool{true};
    aBool = OckStoULL("1", uint64);
    ASSERT_TRUE(uint64 == 1u);
    aBool = OckStoULL("1a", uint64);
    ASSERT_TRUE(!aBool);
    aBool = OckStof("aa", aFloat);
    ASSERT_TRUE(!aBool);
    aBool = OckStof("1.1", aFloat);
    ASSERT_TRUE(aBool);
    std::string path = "tmp";
    ASSERT_TRUE(GetRealPath(path) == false);
    path = "/tmp";
    ASSERT_TRUE(GetRealPath(path) == true);

    std::set<std::string> set;
    std::vector<std::string> vector;
    SplitStr("a,b", ",", set);
    SplitStr("a,b", ",", vector);
    ASSERT_TRUE(set.size() == 2u);
    ASSERT_TRUE(vector.size() == 2u);
}