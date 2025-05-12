/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "smem_common_includes.h"
#include "hybm_core_api.h"
#include "smem_bm.h"
#include "smem_logger.h"
#include "smem_bm_entry_manager.h"

using namespace ock::smem;
#ifdef UT_ENABLED
thread_local std::mutex g_smemBmMutex_;
thread_local bool g_smemBmInited = false;
#else
std::mutex g_smemBmMutex_;
bool g_smemBmInited = false;
#endif

SMEM_API int32_t smem_bm_config_init(smem_bm_config_t *config)
{
    SM_PARAM_VALIDATE(config == nullptr, "Invalid config", SM_INVALID_PARAM);
    config->initTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->createTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->controlOperationTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->startConfigStore = true;
    config->startConfigStoreOnly = false;
    config->dynamicWorldSize = false;
    config->unifiedAddressSpace = true;
    config->flags = 0;
    return SM_OK;
}

SMEM_API int32_t smem_bm_init(const char *configStoreIpPort, smem_bm_mem_type memType, smem_bm_data_op_type dataOpType,
                              uint32_t worldSize, uint32_t rankId, uint16_t deviceId, smem_bm_config_t *config)
{
    std::lock_guard<std::mutex> guard(g_smemBmMutex_);
    if (g_smemBmInited) {
        SM_LOG_INFO("smem bm initialized already");
        return SM_OK;
    }

    int32_t ret = SmemBmEntryManager::Instance().Initialize(configStoreIpPort, worldSize, rankId, deviceId, config);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("init bm entry manager failed, result: " << ret);
        return SM_ERROR;
    }

    ret = HybmCoreApi::HybmCoreInit(deviceId, config->flags);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("init hybm failed, result: " << ret << ", flags: 0x" << std::hex << config->flags);
        return SM_ERROR;
    }

    g_smemBmInited = true;
    SM_LOG_INFO("smem_bm_init success. world_size: " << worldSize << " config_ip: " << configStoreIpPort);
    return SM_OK;
}

SMEM_API void smem_bm_uninit(uint32_t flags)
{
    if (!g_smemBmInited) {
        SM_LOG_WARN("smem bm not initialized yet");
        return;
    }

    HybmCoreApi::HybmCoreUninit();
    SM_LOG_INFO("smem_bm_uninit finished");
}

SMEM_API smem_bm_t smem_bm_create(uint32_t id)
{
    return nullptr;
}

SMEM_API void smem_bm_destroy(smem_bm_t handle)
{
}

SMEM_API int32_t smem_bm_join(smem_bm_t handle, uint32_t flags)
{
    return SM_OK;
}

SMEM_API int32_t smem_bm_leave(smem_bm_t handle, uint32_t flags)
{
    return SM_OK;
}

SMEM_API int32_t smem_bm_copy(smem_bm_t handle, void *src, void *dest, uint64_t size, smem_bm_copy_type t,
                              uint32_t flags)
{
    return SM_OK;
}
