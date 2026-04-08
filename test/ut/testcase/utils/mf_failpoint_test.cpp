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

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

#include <gtest/gtest.h>

#include "mf_fault_injection_point.h"
#include "mf_fault_injection_point_registry.h"

using namespace ock::mf;

namespace {

constexpr int32_t INITIAL_RET = 0;
constexpr int32_t SUCCESS_RET = 7;
constexpr int32_t FAIL_RET = -1;
constexpr int64_t MIN_PAUSE_MS = 40;

constexpr char CALLBACK_POINT_NAME[] = "CALLBACK_POINT";
constexpr char PAUSE_POINT_NAME[] = "PAUSE_POINT";
constexpr char PENDING_POINT_NAME[] = "PENDING_POINT";
constexpr char FILE_RELOAD_POINT_NAME[] = "FILE_RELOAD_POINT";
constexpr char RELOAD_POINT_NAME[] = "RELOAD_POINT";
constexpr char CALLBACK_POINT_DESC[] = "callback point";
constexpr char PAUSE_POINT_DESC[] = "pause point";
constexpr char PENDING_POINT_DESC[] = "pending point";
constexpr char FILE_RELOAD_POINT_DESC[] = "file reload point";
constexpr char RELOAD_POINT_DESC[] = "reload point";
constexpr char ALLOC_LOCAL_MEMORY_POINT_NAME[] = "ALLOC_LOCAL_MEMORY";
constexpr char MMAP_POINT_NAME[] = "MMAP";

std::string GetConfigPath()
{
    return std::string("/tmp/mf_failpoints_") + std::to_string(getpid()) + ".conf";
}

void WriteConfig(const std::string &content)
{
    std::ofstream output(GetConfigPath(), std::ios::out | std::ios::trunc);
    ASSERT_TRUE(output.is_open());
    output << content;
    output.flush();
    ASSERT_TRUE(output.good());
}

void RemoveConfig()
{
    (void)std::remove(GetConfigPath().c_str());
}

void CallbackPoint(FaultInjectionPointParam *userParam, int32_t *result)
{
    if (result != nullptr) {
        *result = FAIL_RET;
    }
    if (userParam != nullptr) {
        std::snprintf(userParam->paramData, sizeof(userParam->paramData), "%s", "handled");
    }
}

void PendingPoint(FaultInjectionPointParam *userParam, int32_t *result)
{
    CallbackPoint(userParam, result);
}

void FileReloadPoint(FaultInjectionPointParam *userParam, int32_t *result)
{
    CallbackPoint(userParam, result);
}

void AlternativeCallbackPoint(FaultInjectionPointParam *userParam, int32_t *result)
{
    (void)userParam;
    if (result != nullptr) {
        *result = SUCCESS_RET;
    }
}

int32_t RunCallbackPoint()
{
    int32_t result = INITIAL_RET;
    FIP_START(CALLBACK_POINT, &result)
    result = SUCCESS_RET;
    FIP_END;
    return result;
}

int32_t RunPendingPoint()
{
    int32_t result = INITIAL_RET;
    FIP_START(PENDING_POINT, &result)
    result = SUCCESS_RET;
    FIP_END;
    return result;
}

int32_t RunFileReloadPoint()
{
    int32_t result = INITIAL_RET;
    FIP_START(FILE_RELOAD_POINT, &result)
    result = SUCCESS_RET;
    FIP_END;
    return result;
}

int32_t RunPausePoint(bool &bodyExecuted)
{
    int32_t result = INITIAL_RET;
    FIP_START(PAUSE_POINT)
    bodyExecuted = true;
    result = SUCCESS_RET;
    FIP_END;
    return result;
}

int32_t RunAllocLocalMemoryPoint()
{
    int32_t result = INITIAL_RET;
    FIP_START(ALLOC_LOCAL_MEMORY, &result)
    result = SUCCESS_RET;
    FIP_END;
    return result;
}

int32_t RunMmapPoint()
{
    int32_t result = INITIAL_RET;
    FIP_START(MMAP, &result)
    result = SUCCESS_RET;
    FIP_END;
    return result;
}

class MfFailpointTest : public testing::Test {
public:
    void SetUp() override
    {
        RemoveConfig();
        ASSERT_EQ(FaultInjectionPointManager::Init(), FaultInjectionPointStatus::OK);
    }

    void TearDown() override
    {
        (void)FaultInjectionPointRegistry::Unregister();
        (void)FaultInjectionPointManager::DeactivateAll();
        (void)FaultInjectionPointManager::Unregister(CALLBACK_POINT_NAME);
        (void)FaultInjectionPointManager::Unregister(PAUSE_POINT_NAME);
        (void)FaultInjectionPointManager::Unregister(PENDING_POINT_NAME);
        (void)FaultInjectionPointManager::Unregister(FILE_RELOAD_POINT_NAME);
        (void)FaultInjectionPointManager::Unregister(RELOAD_POINT_NAME);
        ASSERT_EQ(FaultInjectionPointManager::Exit(), FaultInjectionPointStatus::OK);
        RemoveConfig();
    }

protected:
};

#ifdef MF_ENABLE_TRACEPOINT

TEST_F(MfFailpointTest, InitAndExitAreReferenceCounted)
{
    EXPECT_EQ(FaultInjectionPointManager::Init(), FaultInjectionPointStatus::OK);
    EXPECT_EQ(FaultInjectionPointManager::Exit(), FaultInjectionPointStatus::OK);
    EXPECT_EQ(FaultInjectionPointManager::Exit(), FaultInjectionPointStatus::OK);

    ASSERT_EQ(FaultInjectionPointManager::Init(), FaultInjectionPointStatus::OK);
}

TEST_F(MfFailpointTest, CallbackActivationSkipsBlockAndConsumesAliveCount)
{
    ASSERT_EQ(FaultInjectionPointManager::Register(CALLBACK_POINT_NAME, CALLBACK_POINT_DESC, &CallbackPoint),
              FaultInjectionPointStatus::OK);
    ASSERT_EQ(FaultInjectionPointManager::Activate(CALLBACK_POINT_NAME, FaultInjectionPointType::CALLBACK, 1),
              FaultInjectionPointStatus::OK);

    EXPECT_EQ(RunCallbackPoint(), FAIL_RET);
    EXPECT_FALSE(FaultInjectionPointManager::IsActive(CALLBACK_POINT_NAME));
    EXPECT_EQ(RunCallbackPoint(), SUCCESS_RET);
}

TEST_F(MfFailpointTest, PauseActivationSleepsAndExecutesBlock)
{
    ASSERT_EQ(FaultInjectionPointManager::Register(PAUSE_POINT_NAME, PAUSE_POINT_DESC), FaultInjectionPointStatus::OK);
    ASSERT_EQ(FaultInjectionPointManager::Activate(PAUSE_POINT_NAME, FaultInjectionPointType::PAUSE, 1,
                                                   FaultInjectionPointManager::MakeParam("60")),
              FaultInjectionPointStatus::OK);

    bool bodyExecuted = false;
    auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(RunPausePoint(bodyExecuted), SUCCESS_RET);
    auto end = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_TRUE(bodyExecuted);
    EXPECT_GE(elapsedMs, MIN_PAUSE_MS);
    EXPECT_FALSE(FaultInjectionPointManager::IsActive(PAUSE_POINT_NAME));
}

TEST_F(MfFailpointTest, DuplicateRegisterRequiresSameCallbackSignature)
{
    ASSERT_EQ(FaultInjectionPointManager::Register(CALLBACK_POINT_NAME, CALLBACK_POINT_DESC, &CallbackPoint),
              FaultInjectionPointStatus::OK);
    EXPECT_EQ(FaultInjectionPointManager::Register(CALLBACK_POINT_NAME, CALLBACK_POINT_DESC, &CallbackPoint),
              FaultInjectionPointStatus::OK);
    EXPECT_EQ(FaultInjectionPointManager::Register(CALLBACK_POINT_NAME, CALLBACK_POINT_DESC, &AlternativeCallbackPoint),
              FaultInjectionPointStatus::ERROR);
}

TEST_F(MfFailpointTest, ReloadAppliesFileConfigToRegisteredPoint)
{
    ASSERT_EQ(FaultInjectionPointManager::Register(RELOAD_POINT_NAME, RELOAD_POINT_DESC, &CallbackPoint),
              FaultInjectionPointStatus::OK);
    WriteConfig("RELOAD_POINT callback 1 file_reload\n");

    ASSERT_EQ(FaultInjectionPointManager::Reload(), FaultInjectionPointStatus::OK);
    EXPECT_TRUE(FaultInjectionPointManager::IsActive(RELOAD_POINT_NAME));
}

TEST_F(MfFailpointTest, ReloadBeforeRegisterKeepsPendingActivation)
{
    WriteConfig("PENDING_POINT callback 1 pending_from_file\n");
    ASSERT_EQ(FaultInjectionPointManager::Reload(), FaultInjectionPointStatus::OK);

    ASSERT_EQ(FaultInjectionPointManager::Register(PENDING_POINT_NAME, PENDING_POINT_DESC, &PendingPoint),
              FaultInjectionPointStatus::OK);
    EXPECT_TRUE(FaultInjectionPointManager::IsActive(PENDING_POINT_NAME));
    EXPECT_EQ(RunPendingPoint(), FAIL_RET);
}

TEST_F(MfFailpointTest, FileChangeTriggeredReloadRemainsSupported)
{
    ASSERT_EQ(FaultInjectionPointManager::Register(FILE_RELOAD_POINT_NAME, FILE_RELOAD_POINT_DESC, &FileReloadPoint),
              FaultInjectionPointStatus::OK);
    WriteConfig("FILE_RELOAD_POINT callback 1 file_reload\n");

    EXPECT_TRUE(FaultInjectionPointManager::IsActive(FILE_RELOAD_POINT_NAME));
    EXPECT_EQ(RunFileReloadPoint(), FAIL_RET);
}

TEST_F(MfFailpointTest, ReloadWithoutConfigClearsExistingActivation)
{
    ASSERT_EQ(FaultInjectionPointManager::Register(RELOAD_POINT_NAME, RELOAD_POINT_DESC, &CallbackPoint),
              FaultInjectionPointStatus::OK);
    WriteConfig("RELOAD_POINT callback 1 file_reload\n");
    ASSERT_EQ(FaultInjectionPointManager::Reload(), FaultInjectionPointStatus::OK);
    ASSERT_TRUE(FaultInjectionPointManager::IsActive(RELOAD_POINT_NAME));

    RemoveConfig();
    ASSERT_EQ(FaultInjectionPointManager::Reload(), FaultInjectionPointStatus::OK);
    EXPECT_FALSE(FaultInjectionPointManager::IsActive(RELOAD_POINT_NAME));
}

TEST_F(MfFailpointTest, RegistrarCanRegisterAndUnregisterPoints)
{
    ASSERT_EQ(FaultInjectionPointRegistry::Register(), FaultInjectionPointStatus::OK);
    ASSERT_EQ(FaultInjectionPointManager::Activate(ALLOC_LOCAL_MEMORY_POINT_NAME, FaultInjectionPointType::CALLBACK, 1),
              FaultInjectionPointStatus::OK);
    EXPECT_EQ(RunAllocLocalMemoryPoint(), FAIL_RET);
    ASSERT_EQ(FaultInjectionPointManager::Activate(MMAP_POINT_NAME, FaultInjectionPointType::CALLBACK, 1),
              FaultInjectionPointStatus::OK);
    EXPECT_EQ(RunMmapPoint(), FAIL_RET);

    ASSERT_EQ(FaultInjectionPointRegistry::Unregister(), FaultInjectionPointStatus::OK);
    EXPECT_EQ(FaultInjectionPointManager::Activate(ALLOC_LOCAL_MEMORY_POINT_NAME, FaultInjectionPointType::CALLBACK, 1),
              FaultInjectionPointStatus::NOT_FOUND);
    EXPECT_EQ(FaultInjectionPointManager::Activate(MMAP_POINT_NAME, FaultInjectionPointType::CALLBACK, 1),
              FaultInjectionPointStatus::NOT_FOUND);
}

#else

TEST_F(MfFailpointTest, MacrosAreNoopWhenTracepointDisabled)
{
    int32_t result = INITIAL_RET;
    FIP_START(CALLBACK_POINT, &result)
    result = SUCCESS_RET;
    FIP_END;
    EXPECT_EQ(result, SUCCESS_RET);
    EXPECT_FALSE(FaultInjectionPointManager::IsActive(CALLBACK_POINT_NAME));
}

#endif

} // namespace
