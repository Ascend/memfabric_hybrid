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
#include <cstdlib>

#include "zbal_defines.h"
#undef ALWAYS_INLINE
#define ALWAYS_INLINE inline
#include "zbal_common_includes.h"
#undef ALWAYS_INLINE
#define ALWAYS_INLINE inline __attribute__((always_inline))

#define private   public
#define protected public
#include "dl_mf_api.h"
#include "dl_cann_api.h"
#include "zbal_mem_mf_bootstrap.h"
#undef private
#undef protected

using namespace zbal;
using namespace zbal::bootstrap;
using namespace zbal::underapi;

constexpr uint16_t TEST_DEVICE_ID = 0;
constexpr uint32_t TEST_RANK_COUNT = 4;
constexpr uint32_t TEST_RANK_ID = 0;
constexpr uint64_t TEST_MEM_SIZE = 256ULL * 1024 * 1024;
constexpr uint32_t TEST_COMM_GROUP_MAX = 128;
constexpr uint32_t TEST_COMM_GROUP_ID = 5;

static int g_mockSmemInitResult = 0;
static int g_mockSmemShmConfigInitResult = 0;
static int g_mockAclrtGetDeviceResult = 0;
static int g_mockAclrtGetDeviceId = 0;
static int g_mockSmemShmInitResult = 0;
static void *g_mockSmemShmCreateReturn = nullptr;
static uint64_t g_mockSmemShmGetSymmetricSize = 0;
static int g_mockSmemSetLoggerLevelResult = 0;
static int g_mockSmemShmAtomicAllocValueResult = 0;
static uint32_t g_mockSmemShmAtomicAllocValueId = 0;
static int g_mockSmemShmAtomicReleaseValueResult = 0;
static int g_mockSmemShmSubGroupAllGatherResult = 0;
static int g_mockSmemShmSubGroupBarrierResult = 0;

static bool g_smemUnInitCalled = false;
static bool g_smemShmDestroyCalled = false;

static int MockSmemInit(uint32_t)
{
    return g_mockSmemInitResult;
}

static int MockSmemShmConfigInit(smem_shm_config_t *)
{
    return g_mockSmemShmConfigInitResult;
}

static int MockAclrtGetDevice(int32_t *deviceId)
{
    *deviceId = g_mockAclrtGetDeviceId;
    return g_mockAclrtGetDeviceResult;
}

static int MockSmemShmInit(const char *, uint32_t, uint32_t, uint16_t, smem_shm_config_t *)
{
    return g_mockSmemShmInitResult;
}

static int MockSmemSetExternLogger(void (*)(int, const char *))
{
    return 0;
}

static smem_shm_t MockSmemShmCreate(uint32_t, uint32_t, uint32_t, uint64_t, smem_shm_data_op_type, uint32_t, void **gva)
{
    if (g_mockSmemShmCreateReturn != nullptr && gva != nullptr) {
        *gva = reinterpret_cast<void *>(0x10000);
    }
    return g_mockSmemShmCreateReturn;
}

static int MockSmemShmDestroy(smem_shm_t, uint32_t)
{
    g_smemShmDestroyCalled = true;
    return 0;
}

static uint64_t MockSmemShmGetSymmetricSize(smem_shm_t)
{
    return g_mockSmemShmGetSymmetricSize;
}

static void MockSmemUnInit(void)
{
    g_smemUnInitCalled = true;
}

static int MockSmemSetLoggerLevel(int)
{
    return g_mockSmemSetLoggerLevelResult;
}

static int MockSmemShmAtomicAllocValue(smem_shm_t, uint32_t, uint32_t *retVal)
{
    if (g_mockSmemShmAtomicAllocValueResult == 0 && retVal != nullptr) {
        *retVal = g_mockSmemShmAtomicAllocValueId;
    }
    return g_mockSmemShmAtomicAllocValueResult;
}

static int MockSmemShmAtomicReleaseValue(smem_shm_t, int32_t)
{
    return g_mockSmemShmAtomicReleaseValueResult;
}

static int MockSmemShmSubGroupAllGather(smem_shm_t, const char *, uint32_t, uint32_t, const char *, uint32_t, char *,
                                        uint32_t)
{
    return g_mockSmemShmSubGroupAllGatherResult;
}

static int MockSmemShmSubGroupBarrier(smem_shm_t, const char *, uint32_t, uint32_t)
{
    return g_mockSmemShmSubGroupBarrierResult;
}

class TestZBALMemFabricBootstrap : public testing::Test {
public:
    static void SetUpTestSuite()
    {
        char cwd[4096] = {};
        (void)getcwd(cwd, sizeof(cwd));
        tmpDir_ = std::string(cwd) + "/ut_mf_tmp_" + std::to_string(rand());
        Func::MakeDir(tmpDir_, 0755);

        ascendLibDir_ = tmpDir_ + "/ascend_lib";
        Func::MakeDir(ascendLibDir_, 0755);
        Func::MakeDir(ascendLibDir_ + "/lib64", 0755);

        setenv("MEMFABRIC_HYBRID_LIBRARY_PATH", tmpDir_.c_str(), 1);
        setenv("ASCEND_HOME_PATH", ascendLibDir_.c_str(), 1);
    }

    static void TearDownTestSuite()
    {
        unsetenv("MEMFABRIC_HYBRID_LIBRARY_PATH");
        unsetenv("ASCEND_HOME_PATH");
        Func::RemoveDirRecursive(tmpDir_);
    }

    void SetUp() override
    {
        ResetMockState();
        SetupMockFunctionPointers();
        DlMfApi::gLoaded = true;
        DlCannApi::gLoaded = true;
    }

    void TearDown() override
    {
        DlMfApi::gLoaded = false;
        DlCannApi::gLoaded = false;
        ClearMockFunctionPointers();
    }

    void ResetMockState()
    {
        g_mockSmemInitResult = 0;
        g_mockSmemShmConfigInitResult = 0;
        g_mockAclrtGetDeviceResult = 0;
        g_mockAclrtGetDeviceId = 0;
        g_mockSmemShmInitResult = 0;
        g_mockSmemShmCreateReturn = reinterpret_cast<void *>(0x1);
        g_mockSmemShmGetSymmetricSize = TEST_MEM_SIZE;
        g_mockSmemSetLoggerLevelResult = 0;
        g_mockSmemShmAtomicAllocValueResult = 0;
        g_mockSmemShmAtomicAllocValueId = TEST_COMM_GROUP_ID;
        g_mockSmemShmAtomicReleaseValueResult = 0;
        g_mockSmemShmSubGroupAllGatherResult = 0;
        g_mockSmemShmSubGroupBarrierResult = 0;

        g_smemUnInitCalled = false;
        g_smemShmDestroyCalled = false;
    }

    void SetupMockFunctionPointers()
    {
        DlMfApi::gMfSmemInit = reinterpret_cast<mfSmemInitFunc>(MockSmemInit);
        DlMfApi::gMfSmemShmConfigInit = reinterpret_cast<mfSmemShmConfigInitFunc>(MockSmemShmConfigInit);
        DlMfApi::gMfSmemShmInit = reinterpret_cast<mfSmemShmInitFunc>(MockSmemShmInit);
        DlMfApi::gMfSmemShmCreate = reinterpret_cast<mfSmemShmCreateFunc>(MockSmemShmCreate);
        DlMfApi::gMfSmemShmDestroy = reinterpret_cast<mfSmemShmDestroyFunc>(MockSmemShmDestroy);
        DlMfApi::gMfSmemShmGetSymmetricSize =
            reinterpret_cast<mfSmemShmGetSymmetricSizeFunc>(MockSmemShmGetSymmetricSize);
        DlMfApi::gMfSmemUnInit = reinterpret_cast<mfSmemUnInitFunc>(MockSmemUnInit);
        DlMfApi::gMfSmemSetLogLevel = reinterpret_cast<mfSmemSetLogLevelFunc>(MockSmemSetLoggerLevel);
        DlMfApi::gMfSmemSetExternLogger = reinterpret_cast<mfSmemSetExternLoggerFunc>(MockSmemSetExternLogger);
        DlMfApi::gMfSmemShmAtomicAllocValue =
            reinterpret_cast<mfSmemShmAtomicAllocValueFunc>(MockSmemShmAtomicAllocValue);
        DlMfApi::gMfSmemShmAtomicReleaseValue =
            reinterpret_cast<mfSmemShmAtomicReleaseValueFunc>(MockSmemShmAtomicReleaseValue);
        DlMfApi::gMfSmemShmSubgroupAllGather =
            reinterpret_cast<mfSmemShmSubgroupAllGatherFunc>(MockSmemShmSubGroupAllGather);
        DlMfApi::gMfSmemShmSubgroupBarrier = reinterpret_cast<mfSmemShmSubgroupBarrierFunc>(MockSmemShmSubGroupBarrier);

        DlCannApi::pAclrtGetDevice = reinterpret_cast<aclrtGetDeviceFunc>(MockAclrtGetDevice);
    }

    void ClearMockFunctionPointers()
    {
        DlMfApi::gMfSmemInit = nullptr;
        DlMfApi::gMfSmemShmConfigInit = nullptr;
        DlMfApi::gMfSmemShmInit = nullptr;
        DlMfApi::gMfSmemShmCreate = nullptr;
        DlMfApi::gMfSmemShmDestroy = nullptr;
        DlMfApi::gMfSmemShmGetSymmetricSize = nullptr;
        DlMfApi::gMfSmemUnInit = nullptr;
        DlMfApi::gMfSmemSetLogLevel = nullptr;
        DlMfApi::gMfSmemSetExternLogger = nullptr;
        DlMfApi::gMfSmemShmAtomicAllocValue = nullptr;
        DlMfApi::gMfSmemShmAtomicReleaseValue = nullptr;
        DlMfApi::gMfSmemShmSubgroupAllGather = nullptr;
        DlMfApi::gMfSmemShmSubgroupBarrier = nullptr;

        DlCannApi::pAclrtGetDevice = nullptr;
    }

    MemBootstrapOptions MakeValidOptions()
    {
        MemBootstrapOptions options;
        options.boostrapType = MBT_MEMFABRIC;
        options.deviceId = TEST_DEVICE_ID;
        options.rankCount = TEST_RANK_COUNT;
        options.rankId = TEST_RANK_ID;
        options.totalMemSize = TEST_MEM_SIZE;
        options.ipPort = "127.0.0.1:12345";
        return options;
    }

    void SetupInitialized(MemFabricBoostrap &bootstrap)
    {
        bootstrap.initialized_ = true;
        bootstrap.shmHandle_ = reinterpret_cast<smem_shm_t>(0x1);
    }

    static std::string tmpDir_;
    static std::string ascendLibDir_;
};

std::string TestZBALMemFabricBootstrap::tmpDir_;
std::string TestZBALMemFabricBootstrap::ascendLibDir_;

TEST_F(TestZBALMemFabricBootstrap, InitPreCheck_SuccessAndAlreadyInit)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);

    EXPECT_EQ(bootstrap.InitPreCheck(), Z_OK);

    bootstrap.initialized_ = true;
    EXPECT_EQ(bootstrap.InitPreCheck(), Z_OK);
}

TEST_F(TestZBALMemFabricBootstrap, InitPreCheck_VerifyOptionsFails)
{
    auto options = MakeValidOptions();
    options.rankCount = 0;
    MemFabricBoostrap bootstrap(options);

    EXPECT_NE(bootstrap.InitPreCheck(), Z_OK);
}

TEST_F(TestZBALMemFabricBootstrap, InitPreCheck_MemFabricLibFails)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);

    unsetenv("MEMFABRIC_HYBRID_LIBRARY_PATH");
    EXPECT_NE(bootstrap.InitPreCheck(), Z_OK);
    setenv("MEMFABRIC_HYBRID_LIBRARY_PATH", tmpDir_.c_str(), 1);

    DlMfApi::gLoaded = false;
    EXPECT_NE(bootstrap.InitPreCheck(), Z_OK);
}

TEST_F(TestZBALMemFabricBootstrap, InitPreCheck_AscendLibFails)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);

    unsetenv("ASCEND_HOME_PATH");
    EXPECT_NE(bootstrap.InitPreCheck(), Z_OK);
    setenv("ASCEND_HOME_PATH", ascendLibDir_.c_str(), 1);

    DlCannApi::gLoaded = false;
    EXPECT_NE(bootstrap.InitPreCheck(), Z_OK);
}

TEST_F(TestZBALMemFabricBootstrap, CreateSHMSpace_SimpleApiFails)
{
    auto options = MakeValidOptions();

    g_mockSmemInitResult = -1;
    MemFabricBoostrap b1(options);
    EXPECT_NE(b1.CreateSHMSpace(), Z_OK);

    ResetMockState();
    g_mockSmemShmConfigInitResult = -1;
    MemFabricBoostrap b2(options);
    EXPECT_NE(b2.CreateSHMSpace(), Z_OK);

    ResetMockState();
    g_mockAclrtGetDeviceResult = -1;
    MemFabricBoostrap b3(options);
    EXPECT_NE(b3.CreateSHMSpace(), Z_OK);

    ResetMockState();
    g_mockAclrtGetDeviceId = -1;
    MemFabricBoostrap b4(options);
    EXPECT_NE(b4.CreateSHMSpace(), Z_OK);

    ResetMockState();
    g_mockSmemShmInitResult = -1;
    MemFabricBoostrap b5(options);
    EXPECT_NE(b5.CreateSHMSpace(), Z_OK);
}

TEST_F(TestZBALMemFabricBootstrap, CreateSHMSpace_SmemShmCreateReturnsNull)
{
    g_mockSmemShmCreateReturn = nullptr;
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);

    EXPECT_NE(bootstrap.CreateSHMSpace(), Z_OK);
    EXPECT_TRUE(g_smemUnInitCalled);
}

TEST_F(TestZBALMemFabricBootstrap, CreateSHMSpace_GetSymmetricSizeFails)
{
    g_mockSmemShmGetSymmetricSize = 0;
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);

    EXPECT_NE(bootstrap.CreateSHMSpace(), Z_OK);
    EXPECT_TRUE(g_smemShmDestroyCalled);
    EXPECT_TRUE(g_smemUnInitCalled);
}

TEST_F(TestZBALMemFabricBootstrap, CreateSHMSpace_TotalMemExceedsSpaceSize)
{
    g_mockSmemShmGetSymmetricSize = TEST_MEM_SIZE - 1;
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);

    EXPECT_NE(bootstrap.CreateSHMSpace(), Z_OK);
}

TEST_F(TestZBALMemFabricBootstrap, CreateSHMSpace_Success)
{
    auto options = MakeValidOptions();

    g_mockSmemSetLoggerLevelResult = -1;
    MemFabricBoostrap b1(options);
    EXPECT_EQ(b1.CreateSHMSpace(), Z_OK);
    EXPECT_TRUE(b1.initialized_);

    ResetMockState();
    MemFabricBoostrap b2(options);
    auto result = b2.CreateSHMSpace();
    EXPECT_EQ(result, Z_OK);
    EXPECT_TRUE(b2.initialized_);
    EXPECT_NE(b2.shmHandle_, nullptr);
    EXPECT_NE(b2.output_.gvaDevice, nullptr);
    EXPECT_NE(b2.output_.myGvaDevice, nullptr);
    EXPECT_EQ(b2.output_.memorySizeDevice, options.totalMemSize);
    EXPECT_EQ(b2.output_.memorySpaceSizeDevice, g_mockSmemShmGetSymmetricSize);
}

TEST_F(TestZBALMemFabricBootstrap, Initialize_FailurePaths)
{
    {
        auto options = MakeValidOptions();
        options.rankCount = 0;
        MemFabricBoostrap bootstrap(options);
        EXPECT_NE(bootstrap.Initialize(), Z_OK);
    }
    {
        g_mockSmemInitResult = -1;
        auto options = MakeValidOptions();
        MemFabricBoostrap bootstrap(options);
        EXPECT_NE(bootstrap.Initialize(), Z_OK);
    }
}

TEST_F(TestZBALMemFabricBootstrap, Initialize_Success)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);

    EXPECT_EQ(bootstrap.Initialize(), Z_OK);
    EXPECT_TRUE(bootstrap.initialized_);
}

TEST_F(TestZBALMemFabricBootstrap, UnInitialize_NotInitAndNoHandle)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);

    EXPECT_NO_THROW(bootstrap.UnInitialize());
    EXPECT_FALSE(g_smemShmDestroyCalled);
    EXPECT_FALSE(g_smemUnInitCalled);

    bootstrap.initialized_ = true;
    g_smemUnInitCalled = false;
    EXPECT_NO_THROW(bootstrap.UnInitialize());
    EXPECT_FALSE(g_smemShmDestroyCalled);
    EXPECT_TRUE(g_smemUnInitCalled);
    EXPECT_FALSE(bootstrap.initialized_);
}

TEST_F(TestZBALMemFabricBootstrap, UnInitialize_WithShmHandle)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);
    bootstrap.initialized_ = true;
    bootstrap.shmHandle_ = reinterpret_cast<smem_shm_t>(0x1);

    EXPECT_NO_THROW(bootstrap.UnInitialize());
    EXPECT_TRUE(g_smemShmDestroyCalled);
    EXPECT_TRUE(g_smemUnInitCalled);
    EXPECT_FALSE(bootstrap.initialized_);
    EXPECT_TRUE(bootstrap.shmHandle_ == nullptr);
}

TEST_F(TestZBALMemFabricBootstrap, CommGroupId_NotInitialized)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);

    uint32_t uniqueId = 0;
    EXPECT_EQ(bootstrap.AcquireCommGroupId(TEST_COMM_GROUP_MAX, uniqueId), Z_MEM_NOT_BOOTSTRAP);
    EXPECT_EQ(bootstrap.ReleaseCommGroupId(TEST_COMM_GROUP_ID), Z_MEM_NOT_BOOTSTRAP);
}

TEST_F(TestZBALMemFabricBootstrap, CommGroupId_Success)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);
    SetupInitialized(bootstrap);

    uint32_t uniqueId = 0;
    EXPECT_EQ(bootstrap.AcquireCommGroupId(TEST_COMM_GROUP_MAX, uniqueId), Z_OK);
    EXPECT_EQ(uniqueId, TEST_COMM_GROUP_ID);

    EXPECT_EQ(bootstrap.ReleaseCommGroupId(TEST_COMM_GROUP_ID), Z_OK);
}

TEST_F(TestZBALMemFabricBootstrap, CommGroupId_Failure)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);
    SetupInitialized(bootstrap);

    g_mockSmemShmAtomicAllocValueResult = -1;
    uint32_t uniqueId = 0;
    EXPECT_NE(bootstrap.AcquireCommGroupId(TEST_COMM_GROUP_MAX, uniqueId), Z_OK);

    g_mockSmemShmAtomicReleaseValueResult = -1;
    EXPECT_NE(bootstrap.ReleaseCommGroupId(TEST_COMM_GROUP_ID), Z_OK);
}

TEST_F(TestZBALMemFabricBootstrap, SubGroupAllGather_InvalidParams)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);
    SetupInitialized(bootstrap);

    char sendBuf[16] = "hello";
    char recvBuf[64] = {};

    EXPECT_EQ(bootstrap.SubGroupAllGather("", 2, 0, sendBuf, 5, recvBuf, 64), Z_INVALID_PARAM);
    EXPECT_EQ(bootstrap.SubGroupAllGather("key", 0, 0, sendBuf, 5, recvBuf, 64), Z_INVALID_PARAM);
    EXPECT_EQ(bootstrap.SubGroupAllGather("key", 2, 2, sendBuf, 5, recvBuf, 64), Z_INVALID_PARAM);
    EXPECT_EQ(bootstrap.SubGroupAllGather("key", 2, 0, nullptr, 5, recvBuf, 64), Z_INVALID_PARAM);
    EXPECT_EQ(bootstrap.SubGroupAllGather("key", 2, 0, sendBuf, 0, recvBuf, 64), Z_INVALID_PARAM);
    EXPECT_EQ(bootstrap.SubGroupAllGather("key", 2, 0, sendBuf, 5, nullptr, 64), Z_INVALID_PARAM);
    EXPECT_EQ(bootstrap.SubGroupAllGather("key", 2, 0, sendBuf, 5, recvBuf, 0), Z_INVALID_PARAM);
}

TEST_F(TestZBALMemFabricBootstrap, SubGroupAllGather_NotInitialized)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);

    char sendBuf[16] = "hello";
    char recvBuf[64] = {};
    EXPECT_EQ(bootstrap.SubGroupAllGather("key", 2, 0, sendBuf, 5, recvBuf, 64), Z_MEM_NOT_BOOTSTRAP);
}

TEST_F(TestZBALMemFabricBootstrap, SubGroupAllGather_SuccessAndFailure)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);
    SetupInitialized(bootstrap);

    char sendBuf[16] = "hello";
    char recvBuf[64] = {};
    EXPECT_EQ(bootstrap.SubGroupAllGather("test_key", 2, 0, sendBuf, 5, recvBuf, 64), Z_OK);

    g_mockSmemShmSubGroupAllGatherResult = -1;
    EXPECT_NE(bootstrap.SubGroupAllGather("test_key", 2, 0, sendBuf, 5, recvBuf, 64), Z_OK);
}

TEST_F(TestZBALMemFabricBootstrap, SubGroupBarrier_InvalidParamsAndNotInit)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);
    SetupInitialized(bootstrap);

    EXPECT_EQ(bootstrap.SubGroupBarrier("", 2, 0), Z_INVALID_PARAM);
    EXPECT_EQ(bootstrap.SubGroupBarrier("key", 0, 0), Z_INVALID_PARAM);
    EXPECT_EQ(bootstrap.SubGroupBarrier("key", 2, 2), Z_INVALID_PARAM);

    MemFabricBoostrap bootstrap2(options);
    EXPECT_EQ(bootstrap2.SubGroupBarrier("key", 2, 0), Z_MEM_NOT_BOOTSTRAP);
}

TEST_F(TestZBALMemFabricBootstrap, SubGroupBarrier_SuccessAndFailure)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);
    SetupInitialized(bootstrap);

    EXPECT_EQ(bootstrap.SubGroupBarrier("barrier_key", 4, 1), Z_OK);

    g_mockSmemShmSubGroupBarrierResult = -1;
    EXPECT_NE(bootstrap.SubGroupBarrier("barrier_key", 4, 1), Z_OK);
}

TEST_F(TestZBALMemFabricBootstrap, SetLoggerLevel)
{
    auto options = MakeValidOptions();
    MemFabricBoostrap bootstrap(options);

    EXPECT_EQ(bootstrap.SetLoggerLevel(3), Z_MEM_NOT_BOOTSTRAP);

    SetupInitialized(bootstrap);
    EXPECT_EQ(bootstrap.SetLoggerLevel(3), Z_OK);

    g_mockSmemSetLoggerLevelResult = -1;
    EXPECT_NE(bootstrap.SetLoggerLevel(3), Z_OK);
}