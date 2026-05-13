/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <cstring>

#include "zbal_defines.h"

#undef ALWAYS_INLINE
#define ALWAYS_INLINE
#include "zbal_functions.h"

using namespace zbal;

class TestZBALFunctions : public testing::Test {
public:
    void SetUp() override
    {
        testDir_ = "/tmp/zbal_ut_" + std::to_string(::getpid());
        Func::MakeDir(testDir_, 0755);
    }

    void TearDown() override
    {
        Func::RemoveDirRecursive(testDir_);
    }

    std::string testDir_;
};

/* ================================================================
 * GetEnv
 * ================================================================ */

TEST_F(TestZBALFunctions, GetEnvIntExisting)
{
    ::setenv("ZBAL_UT_INT_VAL", "789", 1);
    EXPECT_EQ(Func::GetEnv<uint32_t>("ZBAL_UT_INT_VAL", 0), 789);
    ::unsetenv("ZBAL_UT_INT_VAL");
}

TEST_F(TestZBALFunctions, GetEnvIntFallback)
{
    EXPECT_EQ(Func::GetEnv<uint32_t>("ZBAL_UT_NONEXIST_VAR", 42), 42);
}

TEST_F(TestZBALFunctions, GetEnvIntInvalidStringReturnsDefault)
{
    ::setenv("ZBAL_UT_INT_INVALID", "not_a_number", 1);
    EXPECT_EQ(Func::GetEnv<uint32_t>("ZBAL_UT_INT_INVALID", 99), 99);
    ::unsetenv("ZBAL_UT_INT_INVALID");
}

TEST_F(TestZBALFunctions, GetEnvIntOutOfRangeReturnsDefault)
{
    ::setenv("ZBAL_UT_INT_OVERFLOW", "99999999999999999999", 1);
    EXPECT_EQ(Func::GetEnv<uint32_t>("ZBAL_UT_INT_OVERFLOW", 77), 77);
    ::unsetenv("ZBAL_UT_INT_OVERFLOW");
}

TEST_F(TestZBALFunctions, GetEnvStringExisting)
{
    ::setenv("ZBAL_UT_STR_VAL", "my_config_value", 1);
    EXPECT_EQ(Func::GetEnv<std::string>("ZBAL_UT_STR_VAL", "default"), "my_config_value");
    ::unsetenv("ZBAL_UT_STR_VAL");
}

TEST_F(TestZBALFunctions, GetEnvStringFallback)
{
    EXPECT_EQ(Func::GetEnv<std::string>("ZBAL_UT_NONEXIST_STR", "fallback"), "fallback");
}

TEST_F(TestZBALFunctions, GetEnvStringEmpty)
{
    ::setenv("ZBAL_UT_STR_EMPTY", "", 1);
    EXPECT_EQ(Func::GetEnv<std::string>("ZBAL_UT_STR_EMPTY", "default"), "");
    ::unsetenv("ZBAL_UT_STR_EMPTY");
}

/* ================================================================
 * Exist / Readable / Writable
 * ================================================================ */

TEST_F(TestZBALFunctions, ExistOnDirectory)
{
    EXPECT_TRUE(Func::Exist("/tmp"));
    EXPECT_FALSE(Func::Exist("/tmp/zbal_nonexistent_dir_12345"));
}

TEST_F(TestZBALFunctions, ExistOnCreatedFile)
{
    std::string fpath = testDir_ + "/exist_test.txt";
    std::ofstream ofs(fpath);
    ofs << "data";
    ofs.close();
    EXPECT_TRUE(Func::Exist(fpath));
}

TEST_F(TestZBALFunctions, ReadableAndWritable)
{
    EXPECT_TRUE(Func::Readable("/tmp"));
    EXPECT_TRUE(Func::Writable("/tmp"));
    EXPECT_TRUE(Func::ReadAndWritable("/tmp"));
    EXPECT_FALSE(Func::Readable("/tmp/zbal_noexist_abcde"));
}

/* ================================================================
 * MakeDir / MakeDirRecursive
 * ================================================================ */

TEST_F(TestZBALFunctions, MakeDirEmptyPathFails)
{
    EXPECT_FALSE(Func::MakeDir("", 0755));
}

TEST_F(TestZBALFunctions, MakeDirSingleLevel)
{
    std::string path = testDir_ + "/single_level";
    EXPECT_TRUE(Func::MakeDir(path, 0755));
    EXPECT_TRUE(Func::IsDir(path));
}

TEST_F(TestZBALFunctions, MakeDirRecursiveDeep)
{
    std::string deep = testDir_ + "/a/b/c/d/e";
    EXPECT_TRUE(Func::MakeDirRecursive(deep, 0755));
    EXPECT_TRUE(Func::Exist(deep));
    EXPECT_TRUE(Func::Exist(testDir_ + "/a/b/c"));
}

TEST_F(TestZBALFunctions, MakeDirRecursiveAlreadyExists)
{
    EXPECT_TRUE(Func::MakeDirRecursive("/tmp", 0755));
}

TEST_F(TestZBALFunctions, MakeDirRecursiveEmptyPathFails)
{
    EXPECT_FALSE(Func::MakeDirRecursive("", 0755));
}

/* ================================================================
 * Remove / RemoveDirRecursive
 * ================================================================ */

TEST_F(TestZBALFunctions, RemoveSingleFile)
{
    std::string fpath = testDir_ + "/rm_test.txt";
    {
        std::ofstream ofs(fpath);
        ofs << "hello";
    }
    EXPECT_TRUE(Func::Remove(fpath, false));
    EXPECT_FALSE(Func::Exist(fpath));
}

TEST_F(TestZBALFunctions, RemoveEmptyPathFails)
{
    EXPECT_FALSE(Func::Remove(""));
}

TEST_F(TestZBALFunctions, RemoveDirRecursiveNested)
{
    std::string subdir = testDir_ + "/nested/sub";
    EXPECT_TRUE(Func::MakeDirRecursive(subdir, 0755));
    std::string fpath = subdir + "/data.txt";
    {
        std::ofstream ofs(fpath);
        ofs << "data";
    }

    EXPECT_TRUE(Func::RemoveDirRecursive(testDir_ + "/nested"));
    EXPECT_FALSE(Func::Exist(testDir_ + "/nested"));
}

TEST_F(TestZBALFunctions, RemoveDirRecursiveNonExistent)
{
    EXPECT_FALSE(Func::RemoveDirRecursive("/tmp/zbal_noexist_rmdir_12345"));
}

/* ================================================================
 * IsDir
 * ================================================================ */

TEST_F(TestZBALFunctions, IsDirDistinguishesFileFromDir)
{
    std::string fpath = testDir_ + "/reg_file.txt";
    {
        std::ofstream ofs(fpath);
        ofs << "x";
    }
    EXPECT_FALSE(Func::IsDir(fpath));
    EXPECT_TRUE(Func::IsDir(testDir_));
}

/* ================================================================
 * GetCurrentDateTime
 * ================================================================ */

TEST_F(TestZBALFunctions, GetCurrentDateTimeFormat)
{
    auto dt = Func::GetCurrentDateTime();
    EXPECT_EQ(dt.length(), 19);
    EXPECT_EQ(dt[4], '_');
    EXPECT_EQ(dt[7], '_');
    EXPECT_EQ(dt[10], '_');
    EXPECT_EQ(dt[13], '_');
    EXPECT_EQ(dt[16], '_');
}

/* ================================================================
 * GetEnvSplitByComma
 * ================================================================ */

TEST_F(TestZBALFunctions, GetEnvSplitByCommaEmpty)
{
    EXPECT_TRUE(Func::GetEnvSplitByComma("ZBAL_UT_NOEXIST_COMMA").empty());
}

TEST_F(TestZBALFunctions, GetEnvSplitByCommaWithWhitespace)
{
    ::setenv("ZBAL_UT_COMMA_WS", "  alpha  , beta , gamma ,,delta  ", 1);
    auto result = Func::GetEnvSplitByComma("ZBAL_UT_COMMA_WS");
    EXPECT_EQ(result.size(), 4);
    EXPECT_TRUE(result.count("alpha") > 0);
    EXPECT_TRUE(result.count("beta") > 0);
    EXPECT_TRUE(result.count("gamma") > 0);
    EXPECT_TRUE(result.count("delta") > 0);
    ::unsetenv("ZBAL_UT_COMMA_WS");
}

TEST_F(TestZBALFunctions, GetEnvSplitByCommaAllWhitespace)
{
    ::setenv("ZBAL_UT_COMMA_WS2", "  ,  ,  ", 1);
    auto result = Func::GetEnvSplitByComma("ZBAL_UT_COMMA_WS2");
    EXPECT_TRUE(result.empty());
    ::unsetenv("ZBAL_UT_COMMA_WS2");
}

/* ================================================================
 * Realpath
 * ================================================================ */

TEST_F(TestZBALFunctions, RealpathValidDirectory)
{
    std::string path = "/tmp";
    EXPECT_TRUE(Func::Realpath(path));
    EXPECT_FALSE(path.empty());
}

TEST_F(TestZBALFunctions, RealpathEmptyPath)
{
    std::string path;
    EXPECT_FALSE(Func::Realpath(path));
}

TEST_F(TestZBALFunctions, RealpathTooLong)
{
    std::string path(ZBAL_PATH_MAX_LIMIT + 1, 'x');
    EXPECT_FALSE(Func::Realpath(path));
}

TEST_F(TestZBALFunctions, RealpathNonExistent)
{
    std::string path = "/tmp/zbal_ut_noexist_path_xyz";
    EXPECT_FALSE(Func::Realpath(path));
}

/* ================================================================
 * LibraryRealPath
 * ================================================================ */

TEST_F(TestZBALFunctions, LibraryRealPathDirNotFound)
{
    std::string realPath;
    EXPECT_FALSE(Func::LibraryRealPath("/zbal_nonexistent_lib_dir", "libtest.so", realPath));
}

TEST_F(TestZBALFunctions, LibraryRealPathLibNotInDir)
{
    std::string realPath;
    EXPECT_FALSE(Func::LibraryRealPath("/tmp", "libzbal_nonexist.so", realPath));
}

TEST_F(TestZBALFunctions, LibraryRealPathCreatesCorrectPath)
{
    std::string soPath = testDir_ + "/libdummy.so";
    {
        std::ofstream ofs(soPath);
        ofs << "dummy";
    }

    std::string realPath;
    EXPECT_TRUE(Func::LibraryRealPath(testDir_, "libdummy.so", realPath));
    EXPECT_NE(realPath.find("libdummy.so"), std::string::npos);
}
