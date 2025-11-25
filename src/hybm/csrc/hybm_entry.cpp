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

#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <regex>
#include "devmm_svm_gva.h"
#include "hybm.h"
#include "hybm_ptracer.h"
#include "hybm_common_include.h"
#include "hybm_gva.h"
#include "hybm_gva_version.h"
#include "hybm_version.h"
#include "mf_file_util.h"
#include "under_api/dl_api.h"

using namespace ock::mf;

namespace {

uint64_t g_baseAddr = 0ULL;
int64_t initialized = 0;
int32_t initedDeviceId = -1;

std::mutex initMutex;
} // namespace

int32_t HybmGetInitDeviceId()
{
    return initedDeviceId;
}

bool HybmHasInited()
{
    return initialized > 0;
}

static inline int hybm_load_library()
{
    std::string libPath;
#ifdef USE_CANN
    char *path = std::getenv("ASCEND_HOME_PATH");
    BM_VALIDATE_RETURN(path != nullptr, "Environment ASCEND_HOME_PATH not set.", BM_ERROR);
    libPath = std::string(path).append("/lib64");
    if (!ock::mf::FileUtil::Realpath(libPath) || !ock::mf::FileUtil::IsDir(libPath)) {
        BM_LOG_ERROR("Environment ASCEND_HOME_PATH check failed.");
        return BM_ERROR;
    }
#endif

    auto ret = DlApi::LoadLibrary(libPath, HybmGetGvaVersion());
    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "load library from path failed: " << ret);
    return 0;
}

HYBM_API int32_t hybm_init(uint16_t deviceId, uint64_t flags)
{
    std::unique_lock<std::mutex> lockGuard{initMutex};
    if (initialized > 0) {
        if (initedDeviceId != deviceId) {
            BM_LOG_ERROR("this deviceId(" << deviceId << ") is not equal to the deviceId(" << initedDeviceId
                                          << ") of other module!");
            return BM_ERROR;
        }

        /*
         * hybm_init will be accessed multiple times when bm/shm/trans init
         * incremental loading is required here.
         */
        BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(hybm_load_library(), "load library failed");

        initialized++;
        return 0;
    }

    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(HalGvaPrecheck(), "the current version of ascend driver does not support mf!");
    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(hybm_load_library(), "load library failed");
    ptracer_config_t config{.tracerType = 1, .dumpFilePath = "/var/log/memfabric_hybrid"};
    auto ret = ptracer_init(&config);
    if (ret != BM_OK) {
        DlApi::CleanupLibrary();
        BM_LOG_ERROR("init ptracer module failed, result: " << ret);
        return ret;
    }

    ret = hybm_init_hbm_gva(deviceId, flags, g_baseAddr);
    if (ret != BM_OK) {
        ptracer_uninit();
        DlApi::CleanupLibrary();
        BM_LOG_ERROR("set device id to be " << deviceId << " failed: " << ret);
        return BM_ERROR;
    }

    initedDeviceId = deviceId;
    initialized = 1L;
    BM_LOG_INFO("hybm init successfully, " << LIB_VERSION << ", deviceId: " << deviceId);
    return 0;
}

HYBM_API void hybm_uninit()
{
    std::unique_lock<std::mutex> lockGuard{initMutex};
    if (initialized <= 0L) {
        BM_LOG_WARN("hybm not initialized.");
        return;
    }

    ptracer_uninit();

    if (--initialized > 0L) {
        return;
    }

    auto ret = drv::HalGvaUnreserveMemory(g_baseAddr);
    BM_LOG_INFO("uninitialize GVA memory return: " << ret);
    g_baseAddr = 0ULL;
    DlApi::CleanupLibrary();
    initialized = 0;
}

HYBM_API void hybm_set_extern_logger(void (*logger)(int level, const char *msg))
{
    if (logger == nullptr) {
        return;
    }
    if (ock::mf::OutLogger::Instance().GetLogExtraFunc() != nullptr) {
        BM_LOG_WARN("logFunc will be rewriting");
    }
    ock::mf::OutLogger::Instance().SetExternalLogFunction(logger, true);
}

HYBM_API int32_t hybm_set_log_level(int level)
{
    BM_VALIDATE_RETURN(ock::mf::OutLogger::ValidateLevel(level),
                       "set log level failed, invalid param, level should be 0~3", -1);
    ock::mf::OutLogger::Instance().SetLogLevel(static_cast<ock::mf::LogLevel>(level));
    return 0;
}

HYBM_API const char *hybm_get_error_string(int32_t errCode)
{
    static thread_local std::string info = std::string("error(").append(std::to_string(errCode)).append(")");
    return info.c_str();
}