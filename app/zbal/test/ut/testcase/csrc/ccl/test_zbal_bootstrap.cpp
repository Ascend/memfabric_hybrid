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

#include "test_zbal_def.h"
#include "zbal_bootstrap_types.h"

#define private public
#include "zbal_bootstrap_default.h"
#undef private

using namespace zbal;
using namespace zbal::bootstrap;

constexpr uint16_t ZBAL_TEST_DEVICE_ID = 0;
constexpr uint16_t ZBAL_TEST_WORLD_SIZE = 4;
constexpr uint16_t ZBAL_TEST_RANK_ID = 0;
constexpr uint64_t ZBAL_TEST_MEM_SIZE = 256ULL * 1024 * 1024;
constexpr uint16_t ZBAL_TEST_COMM_GROUP_CAP = 16;
constexpr uint16_t ZBAL_TEST_COMM_META_SPACE_SIZE = 1024;
constexpr uint32_t ZBAL_TEST_COMM_GROUP_ID = 5;
constexpr uint32_t ZBAL_TEST_COMM_GROUP_MAX = 128;

class MockMemBootstrap : public MemBootstrap {
public:
    explicit MockMemBootstrap(const MemBootstrapOptions &options) : MemBootstrap(options) {}

    ZResult Initialize() noexcept override
    {
        initCalled_ = true;
        return initResult_;
    }

    void UnInitialize() noexcept override
    {
        uninitCalled_ = true;
    }

    ZResult AcquireCommGroupId(uint32_t max, uint32_t &uniqueId) noexcept override
    {
        acqCalled_ = true;
        acqMax_ = max;
        if (acqResult_ == Z_OK) {
            uniqueId = acqReturnId_;
        }
        return acqResult_;
    }

    ZResult ReleaseCommGroupId(uint32_t uniqueId) noexcept override
    {
        relCalled_ = true;
        relId_ = uniqueId;
        return relResult_;
    }

    ZResult SubGroupAllGather(const std::string &key, uint32_t rankSize, uint32_t rankId, const char *sendBuf,
                              uint32_t sendSize, char *recvBuf, uint32_t recvSize) noexcept override
    {
        agCalled_ = true;
        agKey_ = key;
        agRankSize_ = rankSize;
        agRankId_ = rankId;
        return agResult_;
    }

    ZResult SubGroupBarrier(const std::string &key, uint32_t rankSize, uint32_t rankId) noexcept override
    {
        barrierCalled_ = true;
        barrierKey_ = key;
        barrierRankSize_ = rankSize;
        barrierRankId_ = rankId;
        return barrierResult_;
    }

    ZResult SetLoggerLevel(int level) noexcept override
    {
        logCalled_ = true;
        logLevel_ = level;
        return logResult_;
    }

    void SetOutput(const MemBootstrapOutput &out)
    {
        output_ = out;
    }

    ZResult initResult_ = Z_OK;
    ZResult acqResult_ = Z_OK;
    uint32_t acqReturnId_ = 0;
    ZResult relResult_ = Z_OK;
    ZResult agResult_ = Z_OK;
    ZResult barrierResult_ = Z_OK;
    ZResult logResult_ = Z_OK;

    bool initCalled_ = false;
    bool uninitCalled_ = false;
    bool acqCalled_ = false;
    uint32_t acqMax_ = 0;
    bool relCalled_ = false;
    uint32_t relId_ = 0;
    bool agCalled_ = false;
    std::string agKey_;
    uint32_t agRankSize_ = 0;
    uint32_t agRankId_ = 0;
    bool barrierCalled_ = false;
    std::string barrierKey_;
    uint32_t barrierRankSize_ = 0;
    uint32_t barrierRankId_ = 0;
    bool logCalled_ = false;
    int logLevel_ = 0;
};

class TestableMemBootstrap : public MemBootstrap {
public:
    explicit TestableMemBootstrap(const MemBootstrapOptions &options) : MemBootstrap(options) {}

    using MemBootstrap::VerifyOptions;

    ZResult Initialize() noexcept override
    {
        return Z_OK;
    }
    void UnInitialize() noexcept override {}
    ZResult AcquireCommGroupId(uint32_t, uint32_t &) noexcept override
    {
        return Z_OK;
    }
    ZResult ReleaseCommGroupId(uint32_t) noexcept override
    {
        return Z_OK;
    }
    ZResult SubGroupAllGather(const std::string &, uint32_t, uint32_t, const char *, uint32_t, char *,
                              uint32_t) noexcept override
    {
        return Z_OK;
    }
    ZResult SubGroupBarrier(const std::string &, uint32_t, uint32_t) noexcept override
    {
        return Z_OK;
    }
    ZResult SetLoggerLevel(int) noexcept override
    {
        return Z_OK;
    }
};

class TestZBALBootstrap : public testing::Test {
public:
    void SetUp() override
    {
        Bootstrap::Destroy();
    }

    void TearDown() override
    {
        Bootstrap::Destroy();
    }

    void InitValidOptions(zbal_bootstrap_options_t &options)
    {
        bzero(&options, sizeof(zbal_bootstrap_options_t));
        options.btType = BOOT_BY_MEMFABRIC;
        options.worldSize = ZBAL_TEST_WORLD_SIZE;
        options.rankId = ZBAL_TEST_RANK_ID;
        options.deviceId = ZBAL_TEST_DEVICE_ID;
        options.deviceMemorySize = ZBAL_TEST_MEM_SIZE;
        options.commGroupCap = ZBAL_TEST_COMM_GROUP_CAP;
        options.commMetaSpaceSize = ZBAL_TEST_COMM_META_SPACE_SIZE;
    }

    void InitMemBootstrapOptions(MemBootstrapOptions &options)
    {
        options.boostrapType = MBT_MEMFABRIC;
        options.deviceId = ZBAL_TEST_DEVICE_ID;
        options.rankCount = ZBAL_TEST_WORLD_SIZE;
        options.rankId = ZBAL_TEST_RANK_ID;
        options.totalMemSize = ZBAL_TEST_MEM_SIZE;
        options.ipPort = "127.0.0.1:12345";
    }

    std::shared_ptr<MockMemBootstrap> CreateReadyMock()
    {
        auto mock = std::make_shared<MockMemBootstrap>(MemBootstrapOptions{});
        mock->initResult_ = Z_OK;

        MemBootstrapOutput mockOutput;
        mockOutput.gvaDevice = reinterpret_cast<void *>(0x10000);
        mockOutput.myGvaDevice = reinterpret_cast<void *>(0x20000);
        mockOutput.memorySizeDevice = ZBAL_TEST_MEM_SIZE;
        mockOutput.memorySpaceSizeDevice = ZBAL_TEST_MEM_SIZE * ZBAL_TEST_NUMBER_TWO;
        mock->SetOutput(mockOutput);

        return mock;
    }

    void InjectMockIntoBootstrap(Bootstrap *bootstrap, MockMemBootstrap *mock)
    {
        bootstrap->memBootstrap_ = mock;
        bootstrap->inited_ = true;

        auto &memOutput = mock->GetOutput();
        bootstrap->output_.deviceGva = memOutput.gvaDevice;
        bootstrap->output_.myDeviceGva = memOutput.myGvaDevice;
        bootstrap->output_.createdDeviceMemorySpaceSize = memOutput.memorySpaceSizeDevice;
        bootstrap->output_.allocatedDeviceMemorySize = memOutput.memorySizeDevice;
        bootstrap->output_.myCommMetaDeviceGva = memOutput.myGvaDevice;
        bootstrap->output_.metaSizeOfDevice = bootstrap->options_.commMetaSpaceSize;
        bootstrap->output_.metaSizeOfDevice =
            bootstrap->output_.metaSizeOfDevice * ZBAL_TEST_SIZE_1KB * bootstrap->options_.commGroupCap;
        bootstrap->output_.mySMAGva = reinterpret_cast<void *>(
            reinterpret_cast<uintptr_t>(bootstrap->output_.myDeviceGva) + bootstrap->output_.metaSizeOfDevice);
        bootstrap->output_.smaSizeOfDevice =
            bootstrap->output_.allocatedDeviceMemorySize - bootstrap->output_.metaSizeOfDevice;
    }
};

TEST_F(TestZBALBootstrap, LifecycleCreateGetDestroy)
{
    EXPECT_TRUE(Bootstrap::Get() == nullptr);

    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    EXPECT_TRUE(Bootstrap::Create(options) == nullptr);

    Bootstrap::Destroy();
    Bootstrap::Destroy();
}

TEST_F(TestZBALBootstrap, VerifyOptionsAllBranches)
{
    zbal_bootstrap_options_t options;

    InitValidOptions(options);
    options.btType = BOOT_BY_BUTT;
    EXPECT_TRUE(Bootstrap::Create(options) == nullptr);

    InitValidOptions(options);
    options.ipPort[0] = '\0';
    EXPECT_TRUE(Bootstrap::Create(options) == nullptr);

    InitValidOptions(options);
    options.worldSize = ZBAL_RANK_COUNT_MAX_LIMIT + 1;
    EXPECT_TRUE(Bootstrap::Create(options) == nullptr);

    InitValidOptions(options);
    options.rankId = options.worldSize;
    EXPECT_TRUE(Bootstrap::Create(options) == nullptr);

    InitValidOptions(options);
    options.deviceId = ZBAL_DEVICE_COUNT_MAX_LIMIT;
    EXPECT_TRUE(Bootstrap::Create(options) == nullptr);

    InitValidOptions(options);
    options.deviceMemorySize = ZBAL_MEMORY_SIZE_CAP;
    EXPECT_TRUE(Bootstrap::Create(options) == nullptr);

    InitValidOptions(options);
    options.commGroupCap = COMM_GROUP_COUNT_CAP_MAX;
    EXPECT_TRUE(Bootstrap::Create(options) == nullptr);

    InitValidOptions(options);
    options.commGroupCap = ZBAL_TEST_COMM_GROUP_CAP;
    options.commMetaSpaceSize = static_cast<uint16_t>(ZBAL_TEST_MEM_SIZE / ZBAL_TEST_SIZE_1KB);
    EXPECT_TRUE(Bootstrap::Create(options) == nullptr);

    InitValidOptions(options);
    options.commMetaSpaceSize = static_cast<uint16_t>(ZBAL_OPERATE_PARAM_SIZE / ZBAL_TEST_SIZE_1KB);
    EXPECT_TRUE(Bootstrap::Create(options) == nullptr);
}

TEST_F(TestZBALBootstrap, CreateFailsWhenNoMemFabric)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);

    EXPECT_TRUE(Bootstrap::Create(options) == nullptr);
    EXPECT_TRUE(Bootstrap::Get() == nullptr);
}

TEST_F(TestZBALBootstrap, DelegationWhenNotBootstrapped)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);

    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    uint32_t uniqueId = 0;
    EXPECT_TRUE(bootstrap->AcquireCommGroupId(ZBAL_TEST_NUMBER_TEN, uniqueId) == Z_NOT_BOOTSTRAPPED);
    EXPECT_TRUE(bootstrap->ReleaseCommGroupId(0) == Z_NOT_BOOTSTRAPPED);

    char sendBuf[16] = "hello";
    char recvBuf[64] = {};
    EXPECT_TRUE(bootstrap->SubGroupAllGather("test_key", ZBAL_TEST_NUMBER_TWO, 0, sendBuf, ZBAL_TEST_NUMBER_FIVE,
                                             recvBuf, ZBAL_TEST_NUMBER_SIXTYFOUR) == Z_NOT_BOOTSTRAPPED);
    EXPECT_TRUE(bootstrap->SubGroupBarrier("test_key", ZBAL_TEST_NUMBER_TWO, 0) == Z_NOT_BOOTSTRAPPED);
    EXPECT_TRUE(bootstrap->SetLoggerLevel(0) == Z_NOT_BOOTSTRAPPED);

    EXPECT_TRUE(bootstrap->GetOutput().deviceGva == nullptr);

    EXPECT_NO_THROW(bootstrap->UnInitialize());
}

TEST_F(TestZBALBootstrap, MemBootstrapCreateTypeValidation)
{
    MemBootstrapOptions options;
    InitMemBootstrapOptions(options);

    options.boostrapType = MBT_BUTT;
    EXPECT_TRUE(MemBootstrap::Create(options) == nullptr);

    options.boostrapType = MBT_MEMFABRIC;
    EXPECT_TRUE(MemBootstrap::Create(options) != nullptr);
}

TEST_F(TestZBALBootstrap, MemBootstrapVerifyOptionsAllBranches)
{
    MemBootstrapOptions options;
    InitMemBootstrapOptions(options);

    {
        TestableMemBootstrap mb(options);
        EXPECT_TRUE(mb.VerifyOptions() == Z_OK);
    }
    {
        options.rankCount = 0;
        TestableMemBootstrap mb(options);
        EXPECT_TRUE(mb.VerifyOptions() == Z_INVALID_PARAM);
    }
    {
        options.rankCount = ZBAL_TEST_WORLD_SIZE;
        options.rankId = ZBAL_TEST_WORLD_SIZE;
        TestableMemBootstrap mb(options);
        EXPECT_TRUE(mb.VerifyOptions() == Z_INVALID_PARAM);
    }
    {
        options.rankId = ZBAL_TEST_RANK_ID;
        options.deviceId = ZBAL_DEVICE_COUNT_MAX_LIMIT;
        TestableMemBootstrap mb(options);
        EXPECT_TRUE(mb.VerifyOptions() == Z_INVALID_PARAM);
    }
    {
        options.deviceId = ZBAL_TEST_DEVICE_ID;
        options.totalMemSize = ZBAL_MEMORY_SIZE_CAP;
        TestableMemBootstrap mb(options);
        EXPECT_TRUE(mb.VerifyOptions() == Z_INVALID_PARAM);
    }
    {
        options.totalMemSize = ZBAL_TEST_MEM_SIZE;
        options.ipPort = "";
        TestableMemBootstrap mb(options);
        EXPECT_TRUE(mb.VerifyOptions() == Z_INVALID_PARAM);
    }
}

TEST_F(TestZBALBootstrap, AcquireCommGroupIdWithMock)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    mock->acqReturnId_ = ZBAL_TEST_COMM_GROUP_ID;
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    uint32_t uniqueId = 0;
    EXPECT_TRUE(bootstrap->AcquireCommGroupId(ZBAL_TEST_COMM_GROUP_MAX, uniqueId) == Z_OK);
    EXPECT_TRUE(uniqueId == ZBAL_TEST_COMM_GROUP_ID);
    EXPECT_TRUE(mock->acqMax_ == ZBAL_TEST_COMM_GROUP_MAX);

    mock->acqResult_ = Z_ERROR;
    EXPECT_TRUE(bootstrap->AcquireCommGroupId(ZBAL_TEST_NUMBER_TEN, uniqueId) == Z_ERROR);
}

TEST_F(TestZBALBootstrap, AcquireCommGroupIdMaxZero)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    mock->acqReturnId_ = 0;
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    uint32_t uniqueId = 0;
    EXPECT_TRUE(bootstrap->AcquireCommGroupId(0, uniqueId) == Z_OK);
    EXPECT_TRUE(mock->acqMax_ == 0);
}

TEST_F(TestZBALBootstrap, ReleaseCommGroupIdWithMock)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    EXPECT_TRUE(bootstrap->ReleaseCommGroupId(ZBAL_TEST_COMM_GROUP_ID) == Z_OK);
    EXPECT_TRUE(mock->relId_ == ZBAL_TEST_COMM_GROUP_ID);
}

TEST_F(TestZBALBootstrap, SubGroupAllGatherWithMock)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    char sendBuf[16] = "hello";
    char recvBuf[64] = {};
    EXPECT_TRUE(bootstrap->SubGroupAllGather("test_key", ZBAL_TEST_NUMBER_TWO, 0, sendBuf, ZBAL_TEST_NUMBER_FIVE,
                                             recvBuf, ZBAL_TEST_NUMBER_SIXTYFOUR) == Z_OK);
    EXPECT_TRUE(mock->agKey_ == "test_key");
    EXPECT_TRUE(mock->agRankSize_ == ZBAL_TEST_NUMBER_TWO);
}

TEST_F(TestZBALBootstrap, SubGroupBarrierWithMock)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    EXPECT_TRUE(bootstrap->SubGroupBarrier("barrier_key", ZBAL_TEST_NUMBER_FOUR, 1) == Z_OK);
    EXPECT_TRUE(mock->barrierKey_ == "barrier_key");
}

TEST_F(TestZBALBootstrap, SetLoggerLevelWithMock)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    EXPECT_TRUE(bootstrap->SetLoggerLevel(ZBAL_TEST_NUMBER_THREE) == Z_OK);
    EXPECT_TRUE(mock->logLevel_ == ZBAL_TEST_NUMBER_THREE);
}

TEST_F(TestZBALBootstrap, GetOutputAndFieldTranslation)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    const auto &output = bootstrap->GetOutput();
    EXPECT_TRUE(output.deviceGva == reinterpret_cast<void *>(0x10000));
    EXPECT_TRUE(output.myDeviceGva == reinterpret_cast<void *>(0x20000));
    EXPECT_TRUE(output.createdDeviceMemorySpaceSize == ZBAL_TEST_MEM_SIZE * ZBAL_TEST_NUMBER_TWO);
    EXPECT_TRUE(output.allocatedDeviceMemorySize == ZBAL_TEST_MEM_SIZE);
    EXPECT_TRUE(output.myCommMetaDeviceGva == reinterpret_cast<void *>(0x20000));

    uint64_t expectedMetaSize = static_cast<uint64_t>(options.commMetaSpaceSize) * 1024 * options.commGroupCap;
    EXPECT_TRUE(output.metaSizeOfDevice == expectedMetaSize);

    void *expectedSMAGva =
        reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(output.myDeviceGva) + output.metaSizeOfDevice);
    EXPECT_TRUE(output.mySMAGva == expectedSMAGva);
    EXPECT_TRUE(output.smaSizeOfDevice == output.allocatedDeviceMemorySize - output.metaSizeOfDevice);
}

TEST_F(TestZBALBootstrap, UnInitializeWithMock)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    EXPECT_NO_THROW(bootstrap->UnInitialize());
    EXPECT_TRUE(mock->uninitCalled_);
}

TEST_F(TestZBALBootstrap, UnInitializeTwice)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    bootstrap->UnInitialize();
    EXPECT_TRUE(mock->uninitCalled_);

    mock->uninitCalled_ = false;
    EXPECT_NO_THROW(bootstrap->UnInitialize());
    EXPECT_FALSE(mock->uninitCalled_);
}

TEST_F(TestZBALBootstrap, DestroyMemoryBootstrapSwapPattern)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    EXPECT_TRUE(bootstrap->memBootstrap_ != nullptr);
    bootstrap->UnInitialize();
    EXPECT_TRUE(bootstrap->memBootstrap_ == nullptr);
    EXPECT_FALSE(bootstrap->inited_);
}

TEST_F(TestZBALBootstrap, InitializeWhenAlreadyInited)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    EXPECT_TRUE(bootstrap->Initialize() == Z_OK);
    EXPECT_FALSE(mock->initCalled_);
}

TEST_F(TestZBALBootstrap, DestructorTriggersUnInitialize)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);

    auto mock = CreateReadyMock();

    {
        Bootstrap bootstrap(options);
        InjectMockIntoBootstrap(&bootstrap, mock.get());
        EXPECT_FALSE(mock->uninitCalled_);
    }

    EXPECT_TRUE(mock->uninitCalled_);
}

TEST_F(TestZBALBootstrap, CreateWhenSingletonExists)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);

    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    Bootstrap::gBootstrap = bootstrap;

    auto result = Bootstrap::Create(options);
    EXPECT_TRUE(result == bootstrap);

    Bootstrap::gBootstrap = nullptr;
}

TEST_F(TestZBALBootstrap, DestroyWhenSingletonNull)
{
    Bootstrap::gBootstrap = nullptr;
    EXPECT_NO_THROW(Bootstrap::Destroy());
}

TEST_F(TestZBALBootstrap, GetWhenSingletonExists)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    Bootstrap::gBootstrap = bootstrap;
    EXPECT_TRUE(Bootstrap::Get() == bootstrap);

    Bootstrap::gBootstrap = nullptr;
}

TEST_F(TestZBALBootstrap, CreateMemBootstrapOptionsTranslation)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    options.flags = 0xABCD;
    options.dataOperationType = 0x1234;

    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    EXPECT_TRUE(bootstrap->options_.flags == 0xABCD);
    EXPECT_TRUE(bootstrap->options_.dataOperationType == 0x1234);
    EXPECT_TRUE(bootstrap->options_.btType == BOOT_BY_MEMFABRIC);
    EXPECT_TRUE(bootstrap->options_.deviceId == ZBAL_TEST_DEVICE_ID);
    EXPECT_TRUE(bootstrap->options_.worldSize == ZBAL_TEST_WORLD_SIZE);
    EXPECT_TRUE(bootstrap->options_.rankId == ZBAL_TEST_RANK_ID);
    EXPECT_TRUE(bootstrap->options_.deviceMemorySize == ZBAL_TEST_MEM_SIZE);
}

TEST_F(TestZBALBootstrap, FullDelegationFlow)
{
    zbal_bootstrap_options_t options;
    InitValidOptions(options);
    auto bootstrap = ZMakeRef<Bootstrap>(options);
    ASSERT_TRUE(bootstrap != nullptr);

    auto mock = CreateReadyMock();
    mock->acqReturnId_ = ZBAL_TEST_NUMBER_SIXTYFOUR;
    InjectMockIntoBootstrap(bootstrap.Get(), mock.get());

    uint32_t uniqueId = 0;
    EXPECT_TRUE(bootstrap->AcquireCommGroupId(ZBAL_TEST_NUMBER_ONE_HUNDRED, uniqueId) == Z_OK);
    EXPECT_TRUE(uniqueId == ZBAL_TEST_NUMBER_SIXTYFOUR);

    EXPECT_TRUE(bootstrap->ReleaseCommGroupId(ZBAL_TEST_NUMBER_SIXTYFOUR) == Z_OK);
    EXPECT_TRUE(bootstrap->SubGroupBarrier("sync", ZBAL_TEST_NUMBER_TWO, 0) == Z_OK);
    EXPECT_TRUE(bootstrap->SetLoggerLevel(ZBAL_TEST_NUMBER_TWO) == Z_OK);

    EXPECT_NO_THROW(bootstrap->UnInitialize());
    EXPECT_TRUE(mock->uninitCalled_);
}