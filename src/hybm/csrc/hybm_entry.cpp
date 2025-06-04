/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include <cstdlib>
#include <string>
#include <thread>
#include <mutex>

#include "hybm_version.h"
#include "hybm_common_include.h"
#include "runtime_api.h"
#include "devmm_svm_gva.h"
#include "hybm_cmd.h"
#include "hybm.h"

using namespace ock::mf;

namespace {
constexpr uint16_t HYBM_INIT_MODULE_ID_MAX = 32;

int64_t initialized = 0;
uint16_t initedDeviceId = 0;
std::mutex initMutex;
}

int32_t HybmGetInitDeviceId()
{
    return static_cast<int32_t>(initedDeviceId);
}

bool HybmHasInited()
{
    return initialized > 0;
}

int32_t hybm_init(uint16_t deviceId, uint64_t flags)
{
    std::unique_lock<std::mutex> lockGuard{initMutex};
    if (initialized > 0) {
        if (initedDeviceId != deviceId) {
            BM_LOG_ERROR("this deviceId(" << deviceId << ") is not equal to the deviceId(" <<
                initedDeviceId << ") of other module!");
            return BM_ERROR;
        }

        initialized++;
        return 0;
    }

    if (drv::HalGvaPrecheck() != BM_OK) {
        BM_LOG_ERROR("the current version of ascend driver does not support global virtual address!");
        return BM_ERROR;
    }

    auto path = std::getenv("ASCEND_HOME_PATH");
    if (path == nullptr) {
        BM_LOG_ERROR("Environment ASCEND_HOME_PATH not set.");
        return BM_ERROR;
    }

    auto libPath = std::string(path).append("/lib64");
    auto ret = RuntimeApi::LoadLibrary(libPath);
    if (ret != 0) {
        BM_LOG_ERROR("load library from path : " << libPath << " failed: " << ret);
        return ret;
    }

    ret = RuntimeApi::AclrtSetDevice(deviceId);
    if (ret != BM_OK) {
        BM_LOG_ERROR("set device id to be " << deviceId << " failed: " << ret);
        return BM_ERROR;
    }

    void *globalMemoryBase = nullptr;
    size_t allocSize = HYBM_DEVICE_INFO_SIZE;  // 申请meta空间
    drv::HybmInitialize(deviceId, -1);
    ret = RuntimeApi::HalGvaReserveMemory(&globalMemoryBase, allocSize, (int32_t)deviceId, flags);
    if (ret != 0) {
        BM_LOG_ERROR("initialize mete memory with size: " << allocSize << ", flag: " << flags << " failed: " << ret);
        return -1;
    }

    ret = RuntimeApi::HalGvaAlloc((void *)HYBM_DEVICE_META_ADDR, HYBM_DEVICE_INFO_SIZE, 0);
    if (ret != BM_OK) {
        (void)RuntimeApi::HalGvaUnreserveMemory();
        BM_LOG_ERROR("HalGvaAlloc hybm meta memory failed: " << ret);
        return BM_MALLOC_FAILED;
    }

    initedDeviceId = deviceId;
    initialized = 1L;
    BM_LOG_INFO("hybm init successfully, " << LIB_VERSION);
    return 0;
}

void hybm_uninit()
{
    std::unique_lock<std::mutex> lockGuard{initMutex};
    if (initialized <= 0L) {
        BM_LOG_WARN("hybm not initialized.");
        return;
    }

    if (--initialized > 0L) {
        return;
    }

    auto ret = RuntimeApi::HalGvaUnreserveMemory();
    BM_LOG_INFO("uninitialize GVA memory return: " << ret);
    initialized = 0;
}

int32_t hybm_set_extern_logger(void (*logger)(int level, const char *msg))
{
    auto instance = HyBMOutLogger::Instance();
    if (instance == nullptr) {
        return -1;
    }

    instance->SetExternalLogFunction(logger);
    return 0;
}

int32_t hybm_set_log_level(int level)
{
    auto instance = HyBMOutLogger::Instance();
    if (instance == nullptr) {
        return -1;
    }

    if (level >= BUTT_LEVEL) {
        return -1;
    }

    instance->SetLogLevel(static_cast<LogLevel>(level));
    return 0;
}

const char *hybm_get_error_string(int32_t errCode)
{
    static thread_local std::string info;
    info = std::string("error(").append(std::to_string(errCode)).append(")");
    return info.c_str();
}