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
#include "hybm_version.h"
#include "hybm_stream_manager.h"
#include "hybm_va_manager.h"
#include "dl_api.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"

using namespace ock::mf;

namespace {

uint64_t g_baseAddr = 0ULL;
int64_t initialized = 0;
int32_t initedDeviceId = -1;
void *g_allocHandle = nullptr;

std::mutex initMutex;
} // namespace

int32_t HybmGetInitDeviceId()
{
    return initedDeviceId; // userDeviceId
}

bool HybmHasInited()
{
    return initialized > 0;
}

static inline Result hybm_load_library()
{
    std::string libPath;
#if defined(ASCEND_NPU)
    char *path = std::getenv("ASCEND_HOME_PATH");
    BM_VALIDATE_RETURN(path != nullptr, "Environment ASCEND_HOME_PATH is not set.", BM_ERROR);
    libPath = std::string(path).append("/lib64");
    if (!ock::mf::FileUtil::Realpath(libPath) || !ock::mf::FileUtil::IsDir(libPath)) {
        BM_LOG_ERROR("Environment ASCEND_HOME_PATH check failed.");
        return BM_ERROR;
    }
#elif defined(NVIDIA_GPU)
    char *path = std::getenv("CUDA_HOME");
    BM_VALIDATE_RETURN(path != nullptr, "Environment CUDA_HOME is not set.", BM_ERROR);
    libPath = std::string(path).append("/lib64");
    if (!ock::mf::FileUtil::Realpath(libPath) || !ock::mf::FileUtil::IsDir(libPath)) {
        BM_LOG_ERROR("Environment CUDA_HOME check failed.");
        return BM_ERROR;
    }
#endif

    auto ret = DlApi::LoadLibrary(libPath, HybmGetGvaVersion());
    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "load library from path failed: " << ret << ", path:" << libPath);
    return BM_OK;
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
        BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(hybm_load_library(), "load library failed");
        if (g_baseAddr == 0) {
            auto ret = hybm_init_hbm_gva(deviceId, flags, g_baseAddr,
                                         DlAclApi::GetAscendSocType(), &g_allocHandle);
            BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(ret, "init shm meta failed");
        }

        initialized++;
        return 0;
    }

    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(HalGvaPrecheck(), "the current version of ascend driver does not support mf!");
    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(hybm_load_library(), "load library failed");
    ptracer_config_t config{.tracerType = 1, .dumpFilePath = "/var/log/memfabric_hybrid"};
    auto ret = ptracer_init(&config);
    if (ret != BM_OK) {
        BM_LOG_WARN("init ptracer module failed, result: " << ret << ", error msg: " << ptracer_get_last_err_msg());
    }

    ret = hybm_init_hbm_gva(deviceId, flags, g_baseAddr, DlAclApi::GetAscendSocType(), &g_allocHandle);
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

    if (--initialized > 0L) {
        return;
    }

    ptracer_uninit();
    auto socType = DlAclApi::GetAscendSocType();
    if ((socType == AscendSocType::ASCEND_950) || (HybmGetGvaVersion() == HYBM_GVA_V4)) {
        if (g_baseAddr != 0) {
            auto ret = DlHalApi::HalMemUnmap(reinterpret_cast<void *>(g_baseAddr));
            BM_LOG_INFO("unmap meta info res: " << ret);
            if (g_allocHandle != nullptr) {
                ret = DlHalApi::HalMemRelease((drv_mem_handle_t *)g_allocHandle);
                g_allocHandle = nullptr;
                BM_LOG_INFO("release meta memory handle res: " << ret);
            }
            ret = DlHalApi::HalMemAddressFree(reinterpret_cast<void *>(g_baseAddr));
            BM_LOG_INFO("free meta memory res: " << ret);
        }
    } else {
        if (g_baseAddr != 0) {
            drv::HalGvaFree(HYBM_DEVICE_META_ADDR, HYBM_DEVICE_INFO_SIZE);
            auto ret = drv::HalGvaUnreserveMemory(g_baseAddr);
            BM_LOG_INFO("uninitialize GVA memory return: " << ret);
            g_baseAddr = 0ULL;
        }
    }

    HybmStreamManager::DestroyAllThreadHybmStream();
    DlApi::CleanupLibrary();
    initialized = 0;
}

HYBM_API void hybm_set_extern_logger(void (*logger)(int level, const char *msg))
{
    if (logger == nullptr) {
        return;
    }

    if (ock::mf::OutLogger::Instance().GetExternalLogFunction() != nullptr) {
        BM_LOG_WARN("External log function has already been set, which will be override with new log function");
    }
    ock::mf::OutLogger::Instance().SetExternalLogFunction(logger, true);
}

HYBM_API void hybm_set_extern_alarm(void (*alarm)(uint16_t code, const char *msg), void (*resume)(uint16_t code))
{
    if (alarm == nullptr) {
        return;
    }

    if (ock::mf::OutLogger::Instance().GetAlarmLogFunction() != nullptr) {
        BM_LOG_WARN("Alarm log function has already been set, which will be override with new log function");
    }
    ock::mf::OutLogger::Instance().SetAlarmLogFunction(alarm, resume, true);
}

HYBM_API int32_t hybm_set_log_level(int level)
{
    BM_VALIDATE_RETURN(ock::mf::OutLogger::ValidateLevel(level),
                       "set log level failed, invalid param, level should be 0~4", -1);
    ock::mf::OutLogger::Instance().SetLogLevel(static_cast<ock::mf::LogLevel>(level));
    return 0;
}

HYBM_API const char *hybm_get_error_string(int32_t errCode)
{
    static thread_local std::string info = std::string("error(").append(std::to_string(errCode)).append(")");
    return info.c_str();
}

HYBM_API int32_t hybm_gva_to_va(uint64_t gva, hybm_mem_type vaMemType, uint64_t *va)
{
    if (va == nullptr) {
        BM_LOG_ERROR("input va is null.");
        return BM_ERROR;
    }

    if (!HybmHasInited()) {
        BM_LOG_ERROR("hybm not initialized.");
        return BM_ERROR;
    }

    // Validate memory type
    if (vaMemType >= HYBM_MEM_TYPE_BUTT) {
        BM_LOG_ERROR("invalid memory type: " << vaMemType);
        return BM_ERROR;
    }
    // Determine output VA type based on memory type
    uint32_t outputType = vaMemType == HYBM_MEM_TYPE_DEVICE ? HVM_DVA : HVM_HVA;
    uint64_t convertedVa = HybmVaManager::GetInstance().TransformVa(gva, HVM_GVA, outputType);
    if (convertedVa == 0) {
        BM_LOG_ERROR("GVA to VA conversion failed for address: 0x" << std::hex << gva);
        return BM_ERROR;
    }

    *va = convertedVa;
    return BM_OK;
}