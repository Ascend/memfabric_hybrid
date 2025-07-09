/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "hybm.h"
#include "hybm_types.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"

using namespace ock::mf;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

const std::string ASCEND_HOME_PATH = std::string(getenv("ASCEND_HOME_PATH"));
const std::string LD_LIBRARY_PATH = std::string(getenv("LD_LIBRARY_PATH"));

class HybmEntryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

/**
* @tc.name  : hybm_init_ShouldReturnZero_WhenAllConditionsMet
* @tc.desc  : Test hybm_init function when all conditions are met
*/
TEST_F(HybmEntryTest, hybm_init_ShouldReturnZero_WhenAllConditionsMet)
{
    uint16_t deviceId = 0;
    uint64_t flags = 0;

    int32_t result = hybm_init(deviceId, flags);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(HybmHasInited(), true);
    EXPECT_EQ(HybmGetInitDeviceId(), deviceId);

    hybm_uninit();
}

/**
* @tc.name  : hybm_init_ShouldReturnBMError_WhenDeviceIdNotEqual
* @tc.desc  : Test hybm_init function when deviceId is not equal to initedDeviceId
*/
TEST_F(HybmEntryTest, hybm_init_ShouldReturnBMError_WhenDeviceIdNotEqual)
{
    uint16_t deviceId = 0;
    uint64_t flags = 0;

    int32_t result = hybm_init(deviceId, flags);
    EXPECT_EQ(result, 0);

    result = hybm_init(deviceId, flags);
    EXPECT_EQ(result, 0);
    hybm_uninit();

    deviceId = 1;
    result = hybm_init(deviceId, flags);
    EXPECT_EQ(result, BM_ERROR);

    hybm_uninit();
}

/**
* @tc.name  : hybm_init_ShouldReturnBMError_WhenHalGvaPrecheckFails
* @tc.desc  : Test hybm_init function when HalGvaPrecheck returns non-BM_OK
*/
TEST_F(HybmEntryTest, hybm_init_ShouldReturnBMError_WhenHalGvaPrecheckFails)
{
    uint16_t deviceId = 1;
    uint64_t flags = 0;

    (void) unsetenv("LD_LIBRARY_PATH");
    int32_t result = hybm_init(deviceId, flags);
    EXPECT_EQ(result, BM_ERROR);
    (void) setenv("LD_LIBRARY_PATH", LD_LIBRARY_PATH.c_str(), 1);

    hybm_uninit();
}

/**
* @tc.name  : hybm_init_ShouldReturnBMError_WhenAscendHomePathNotSet
* @tc.desc  : Test hybm_init function when ASCEND_HOME_PATH environment variable is not set
*/
TEST_F(HybmEntryTest, hybm_init_ShouldReturnBMError_WhenAscendHomePathNotSet)
{
    uint16_t deviceId = 1;
    uint64_t flags = 0;

    (void) unsetenv("ASCEND_HOME_PATH");
    int32_t result = hybm_init(deviceId, flags);
    EXPECT_EQ(result, BM_ERROR);
    (void) setenv("ASCEND_HOME_PATH", ASCEND_HOME_PATH.c_str(), 1);

    hybm_uninit();
}

/**
* @tc.name  : hybm_init_ShouldReturnBMError_WhenSetDeviceFails
* @tc.desc  : Test hybm_init function when DlAclApi::AclrtSetDevice returns non-BM_OK
*/
TEST_F(HybmEntryTest, hybm_init_ShouldReturnBMError_WhenSetDeviceFails)
{
    uint16_t deviceId = 1;
    uint64_t flags = 0;

    MOCKER_CPP(&DlAclApi::AclrtSetDevice, int32_t (*)(int32_t)).stubs().will(returnValue(-1));

    int32_t result = hybm_init(deviceId, flags);
    EXPECT_EQ(result, BM_ERROR);

    hybm_uninit();
}

/**
* @tc.name  : hybm_init_ShouldReturnMinusOne_WhenReserveMemoryFails
* @tc.desc  : Test hybm_init function when DlHalApi::HalGvaReserveMemory returns non-0
*/
TEST_F(HybmEntryTest, hybm_init_ShouldReturnMinusOne_WhenReserveMemoryFails)
{
    uint16_t deviceId = 1;
    uint64_t flags = 0;

    MOCKER_CPP(&DlHalApi::HalGvaReserveMemory, int32_t (*)(void **, size_t, int32_t, uint64_t))
        .stubs().will(returnValue(-1));
    int32_t result = hybm_init(deviceId, flags);
    EXPECT_EQ(result, BM_ERROR);

    hybm_uninit();
}

/**
* @tc.name  : hybm_init_ShouldReturnBMMallocFailed_WhenAllocMemoryFails
* @tc.desc  : Test hybm_init function when DlHalApi::HalGvaAlloc returns non-BM_OK
*/
TEST_F(HybmEntryTest, hybm_init_ShouldReturnBMMallocFailed_WhenAllocMemoryFails)
{
    uint16_t deviceId = 1;
    uint64_t flags = 0;

    MOCKER_CPP(&DlHalApi::HalGvaAlloc, int32_t (*)(void *, size_t, uint64_t))
        .stubs().will(returnValue(-1));
    int32_t result = hybm_init(deviceId, flags);
    EXPECT_EQ(result, BM_MALLOC_FAILED);

    hybm_uninit();
    // 验证未初始化分支
    hybm_uninit();
}

/**
* @tc.name  : hybm_set_log_level_ShouldReturnMinusOne_WhenInstanceIsNull
* @tc.desc  : 测试当日志实例为空时，hybm_set_log_level函数应返回-1
*/
TEST_F(HybmEntryTest, hybm_set_log_level_ShouldReturnMinusOne_WhenInstanceIsNull)
{
    // 模拟日志实例为空
    HyBMOutLogger *logger = nullptr;
    MOCKER_CPP(&HyBMOutLogger::Instance, HyBMOutLogger *(*)()).stubs().will(returnValue(logger));

    EXPECT_EQ(hybm_set_log_level(INFO_LEVEL), -1);
    EXPECT_EQ(hybm_set_extern_logger(nullptr), -1);
}

/**
* @tc.name  : hybm_set_log_level_ShouldReturnMinusOne_WhenLevelIsInvalid
* @tc.desc  : 测试当日志级别无效时，hybm_set_log_level函数应返回-1
*/
TEST_F(HybmEntryTest, hybm_set_log_level_ShouldReturnMinusOne_WhenLevelIsInvalid)
{

    EXPECT_EQ(hybm_set_log_level(BUTT_LEVEL), -1);

    EXPECT_EQ(hybm_set_log_level(INFO_LEVEL), 0);
    EXPECT_EQ(hybm_set_extern_logger(nullptr), 0);

    EXPECT_NE(hybm_get_error_string(0), nullptr);

}