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

#include "mf_file_util.h"

using namespace ock::mf;

class MFFileUtilTest : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase()
    {
        GlobalMockObject::reset();
    }

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(MFFileUtilTest, GetFileSize_1)
{
    std::string path1 = "/etc/group";
    size_t size1 = FileUtil::GetFileSize(path1);
    EXPECT_TRUE(size1 >= 0);

    std::string path2 = "/etc/group111222";
    size_t size2 = FileUtil::GetFileSize(path2);
    EXPECT_EQ(size2, 0);

    MOCKER(fseek).stubs().will(returnValue(-1));
    size_t size3 = FileUtil::GetFileSize(path1);
    EXPECT_TRUE(size3 == 0);

    MOCKER(fopen).stubs().will(returnValue(static_cast<FILE *>(nullptr)));
    size_t size4 = FileUtil::GetFileSize(path1);
    EXPECT_TRUE(size4 == 0);
}

TEST_F(MFFileUtilTest, IsFile_1)
{
    std::string path1 = "/etc/group";
    EXPECT_TRUE(ock::mf::FileUtil::IsFile(path1));

    std::string path2 = "/etc/group111222";
    EXPECT_FALSE(ock::mf::FileUtil::IsFile(path2));
}

TEST_F(MFFileUtilTest, IsDir_1)
{
    std::string path1 = "/etc/";
    EXPECT_TRUE(ock::mf::FileUtil::IsDir(path1));

    std::string path2 = "/etc/group111222";
    EXPECT_FALSE(ock::mf::FileUtil::IsDir(path2));
}

TEST_F(MFFileUtilTest, CheckFileSize_1)
{
    std::string path1 = "/etc/group";
    const uint32_t max_size = 10 * 1024 * 1024;
    EXPECT_TRUE(ock::mf::FileUtil::CheckFileSize(path1, max_size));

    std::string path2 = "/etc/group111222";
    EXPECT_FALSE(ock::mf::FileUtil::CheckFileSize(path2, max_size));
}