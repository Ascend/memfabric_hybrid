/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#include <cstdlib>
#include <string>
#include <thread>
#include <mutex>
#include <fstream>
#include <limits.h>

#include "hybm_version.h"
#include "hybm_common_include.h"
#include "under_api/dl_api.h"
#include "under_api/dl_acl_api.h"
#include "under_api/dl_hal_api.h"
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

HYBM_API int32_t hybm_init(uint16_t deviceId, uint64_t flags)
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

    auto path = std::getenv("ASCEND_HOME_PATH");
    if (path == nullptr) {
        BM_LOG_ERROR("Environment ASCEND_HOME_PATH not set.");
        return BM_ERROR;
    }

    auto libPath = std::string(path).append("/lib64");
    auto ret = DlApi::LoadLibrary(libPath);
    BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "load library from path: " << libPath << " failed: " << ret, ret);

    ret = DlAclApi::AclrtSetDevice(deviceId);
    if (ret != BM_OK) {
        DlApi::CleanupLibrary();
        BM_LOG_ERROR("set device id to be " << deviceId << " failed: " << ret);
        return BM_ERROR;
    }

    initedDeviceId = deviceId;
    initialized = 1L;
    BM_LOG_INFO("hybm init successfully, " << LIB_VERSION);
    return 0;
}

HYBM_API void hybm_uninit()
{
    std::unique_lock<std::mutex> lockGuard{initMutex};
    if (initialized <= 0L) {
        BM_LOG_WARN("hybm not initialized.");
        return;
    }

    if (--initialized > 0L) {
        return;
    }

    DlApi::CleanupLibrary();
    initialized = 0;
}

HYBM_API int32_t hybm_set_extern_logger(void (*logger)(int level, const char *msg))
{
    auto instance = HyBMOutLogger::Instance();
    if (instance == nullptr) {
        return -1;
    }

    instance->SetExternalLogFunction(logger);
    return 0;
}

HYBM_API int32_t hybm_set_log_level(int level)
{
    auto instance = HyBMOutLogger::Instance();
    if (instance == nullptr) {
        return -1;
    }

    if (level < 0 || level >= BUTT_LEVEL) {
        BM_LOG_ERROR("Set log level error, invalid param level: " << level);
        return -1;
    }

    instance->SetLogLevel(static_cast<LogLevel>(level));
    return 0;
}

HYBM_API const char *hybm_get_error_string(int32_t errCode)
{
    static thread_local std::string info;
    info = std::string("error(").append(std::to_string(errCode)).append(")");
    return info.c_str();
}