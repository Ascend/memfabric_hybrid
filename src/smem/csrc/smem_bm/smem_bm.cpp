/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <algorithm>
#include "smem_common_includes.h"
#include "hybm_big_mem.h"
#include "smem_logger.h"
#include "smem_bm_entry_manager.h"
#include "smem_hybm_helper.h"
#include "mf_rwlock.h"
#include "smem_bm.h"

using namespace ock::smem;
using namespace ock::mf;
#ifdef UT_ENABLED
thread_local ReadWriteLock g_smemBmMutex_;
thread_local bool g_smemBmInited = false;
#else
ReadWriteLock g_smemBmMutex_;
bool g_smemBmInited = false;
#endif

SMEM_API int32_t smem_bm_config_init(smem_bm_config_t *config)
{
    SM_VALIDATE_RETURN(config != nullptr, "Invalid config", SM_INVALID_PARAM);
    config->initTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->createTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->controlOperationTimeout = SMEM_DEFAUT_WAIT_TIME;
    config->startConfigStoreServer = true;
    config->startConfigStoreOnly = false;
    config->dynamicWorldSize = false;
    config->unifiedAddressSpace = true;
    config->autoRanking = true;
    config->rankId = std::numeric_limits<uint16_t>::max();
    config->flags = 0;
    bzero(config->hcomUrl, sizeof(config->hcomUrl));
    bzero(&config->hcomTlsConfig, sizeof(config->hcomTlsConfig));
    bzero(&config->storeTlsConfig, sizeof(config->storeTlsConfig));
    return SM_OK;
}

static int32_t SmemBmConfigCheck(const smem_bm_config_t *config)
{
    SM_VALIDATE_RETURN(config != nullptr, "config is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->unifiedAddressSpace == true, "unifiedAddressSpace must be true", SM_INVALID_PARAM);

    SM_VALIDATE_RETURN(config->initTimeout != 0, "initTimeout is zero", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->initTimeout <= SMEM_BM_TIMEOUT_MAX, "initTimeout is too large", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->createTimeout != 0, "createTimeout is zero", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->createTimeout <= SMEM_BM_TIMEOUT_MAX, "initTimeout is too large", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->controlOperationTimeout != 0, "controlOperationTimeout is zero", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(config->controlOperationTimeout <= SMEM_BM_TIMEOUT_MAX, "controlOperationTimeout is too large",
                       SM_INVALID_PARAM);

    // config->rank 在SmemBmEntryManager::PrepareStore中check
    return 0;
}

SMEM_API int32_t smem_bm_init(const char *storeURL, uint32_t worldSize, uint16_t deviceId,
                              const smem_bm_config_t *config)
{
    SM_VALIDATE_RETURN(worldSize != 0, "invalid param, worldSize is 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(worldSize <= SMEM_WORLD_SIZE_MAX, "invalid param, worldSize is too large", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(storeURL != nullptr, "invalid param, storeURL is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(SmemBmConfigCheck(config) == 0, "config is invalid", SM_INVALID_PARAM);

    WriteGuard locker(g_smemBmMutex_);
    if (g_smemBmInited) {
        SM_LOG_INFO("smem bm initialized already");
        return SM_OK;
    }

    int32_t ret = SmemBmEntryManager::Instance().Initialize(storeURL, worldSize, deviceId, *config);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("init bm entry manager failed, result: " << ret);
        return SM_ERROR;
    }

    ret = hybm_init(deviceId, config->flags);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("init hybm failed, result: " << ret << ", flags: 0x" << std::hex << config->flags);
        SmemBmEntryManager::Instance().Destroy();
        return SM_ERROR;
    }

    g_smemBmInited = true;
    SM_LOG_INFO("smem_bm_init success. "
                << " config_ip: " << storeURL);
    return SM_OK;
}

SMEM_API void smem_bm_uninit(uint32_t flags)
{
    WriteGuard locker(g_smemBmMutex_);
    if (!g_smemBmInited) {
        SM_LOG_WARN("smem bm not initialized yet");
        return;
    }

    hybm_uninit();
    SmemBmEntryManager::Instance().Destroy();
    g_smemBmInited = false;
    SM_LOG_INFO("smem_bm_uninit finished");
}

SMEM_API uint32_t smem_bm_get_rank_id()
{
    return SmemBmEntryManager::Instance().GetRankId();
}

/* return 1 means check ok */
static inline int32_t SmemBmDataOpCheck(smem_bm_data_op_type dataOpType)
{
    constexpr uint32_t dataOpTypeMask =
        SMEMB_DATA_OP_SDMA | SMEMB_DATA_OP_HOST_RDMA | SMEMB_DATA_OP_HOST_TCP | SMEMB_DATA_OP_DEVICE_RDMA;
    return (dataOpType & dataOpTypeMask) != 0;
}

SMEM_API smem_bm_t smem_bm_create(uint32_t id, uint32_t memberSize, smem_bm_data_op_type dataOpType,
                                  uint64_t localDRAMSize, uint64_t localHBMSize, uint32_t flags)
{
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", nullptr);
    SM_VALIDATE_RETURN(!(localDRAMSize == 0UL && localHBMSize == 0UL), "localMemorySize is 0", nullptr);
    SM_VALIDATE_RETURN(localDRAMSize <= SMEM_LOCAL_DRAM_SIZE_MAX, "local DRAM size exceeded", nullptr);
    SM_VALIDATE_RETURN(localHBMSize <= SMEM_LOCAL_HBM_SIZE_MAX, "local HBM size exceeded", nullptr);

    SmemBmEntryPtr entry;
    auto &manager = SmemBmEntryManager::Instance();
    SM_ASSERT_RETURN_NOLOG(SmemBmDataOpCheck(dataOpType), nullptr);
    auto ret = manager.CreateEntryById(id, entry);
    if (ret != 0 || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("create BM entity(" << id << ") failed: " << ret);
        return nullptr;
    }

    hybm_options options{};
    options.bmType = HYBM_TYPE_HOST_INITIATE;
    options.memType = SmemHybmHelper::TransHybmMemType(localDRAMSize, localHBMSize);
    options.bmDataOpType = SmemHybmHelper::TransHybmDataOpType(dataOpType);
    options.bmScope = HYBM_SCOPE_CROSS_NODE;
    options.rankCount = manager.GetWorldSize();
    options.rankId = manager.GetRankId();
    options.devId = manager.GetDeviceId();
    options.deviceVASpace = localHBMSize;
    options.hostVASpace = localDRAMSize;
    options.preferredGVA = 0;
    options.role = HYBM_ROLE_PEER;
    bzero(options.transUrl, sizeof(options.transUrl));

    smem_tls_config hcomTlsConfig = manager.GetHcomTlsOption();
    options.tlsOption.tlsEnable = hcomTlsConfig.tlsEnable;
    std::copy_n(hcomTlsConfig.caPath, SMEM_TLS_PATH_SIZE, options.tlsOption.caPath);
    std::copy_n(hcomTlsConfig.crlPath, SMEM_TLS_PATH_SIZE, options.tlsOption.crlPath);
    std::copy_n(hcomTlsConfig.certPath, SMEM_TLS_PATH_SIZE, options.tlsOption.certPath);
    std::copy_n(hcomTlsConfig.keyPath, SMEM_TLS_PATH_SIZE, options.tlsOption.keyPath);
    std::copy_n(hcomTlsConfig.keyPassPath, SMEM_TLS_PATH_SIZE, options.tlsOption.keyPassPath);
    std::copy_n(hcomTlsConfig.packagePath, SMEM_TLS_PATH_SIZE, options.tlsOption.packagePath);
    std::copy_n(hcomTlsConfig.decrypterLibPath, SMEM_TLS_PATH_SIZE, options.tlsOption.decrypterLibPath);

    SM_VALIDATE_RETURN(manager.GetHcomUrl().size() <= 64u, "url size is " << manager.GetHcomUrl().size(), nullptr);
    (void) std::copy_n(manager.GetHcomUrl().c_str(),  manager.GetHcomUrl().size(), options.transUrl);

    options.globalUniqueAddress = true;
    ret = entry->Initialize(options);
    if (ret != 0) {
        SM_LOG_AND_SET_LAST_ERROR("entry init failed, result: " << ret);
        return nullptr;
    }
    return reinterpret_cast<void *>(entry.Get());
}

SMEM_API void smem_bm_destroy(smem_bm_t handle)
{
    SM_ASSERT_RET_VOID(handle != nullptr);
    SM_ASSERT_RET_VOID(g_smemBmInited);

    auto ret = SmemBmEntryManager::Instance().RemoveEntryByPtr(reinterpret_cast<uintptr_t>(handle));
    SM_ASSERT_RET_VOID(ret == SM_OK);
}

SMEM_API int32_t smem_bm_join(smem_bm_t handle, uint32_t flags)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", SM_NOT_INITIALIZED);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }
    return entry->Join(flags);
}

SMEM_API int32_t smem_bm_leave(smem_bm_t handle, uint32_t flags)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", SM_NOT_INITIALIZED);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    return entry->Leave(flags);
}

SMEM_API uint64_t smem_bm_get_local_mem_size_by_mem_type(smem_bm_t handle, smem_bm_mem_type memType)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", 0UL);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return 0UL;
    }
    switch (memType) {
        case SMEM_MEM_TYPE_DEVICE:
            return entry->GetCoreOptions().deviceVASpace;
        case SMEM_MEM_TYPE_HOST:
            return entry->GetCoreOptions().hostVASpace;
        default:
            SM_LOG_AND_SET_LAST_ERROR("input mem type is invalid, memType: " << memType);
            return 0UL;
    }
}

SMEM_API void *smem_bm_ptr_by_mem_type(smem_bm_t handle, smem_bm_mem_type memType, uint16_t peerRankId)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", nullptr);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", nullptr);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return nullptr;
    }

    auto &coreOption = entry->GetCoreOptions();
    SM_VALIDATE_RETURN(peerRankId < coreOption.rankCount, "invalid param, peerRankId too large", nullptr);

    void *addr = nullptr;
    switch (memType) {
        case SMEM_MEM_TYPE_DEVICE:
            addr = entry->GetDeviceGvaAddress();
            return static_cast<uint8_t *>(addr) + coreOption.deviceVASpace * peerRankId;
        case SMEM_MEM_TYPE_HOST:
            addr = entry->GetHostGvaAddress();
            return static_cast<uint8_t *>(addr) + coreOption.hostVASpace * peerRankId;
        default:
            SM_LOG_AND_SET_LAST_ERROR("input mem type is invalid, memType: " << memType);
            return nullptr;
    }
}

SMEM_API int32_t smem_bm_copy(smem_bm_t handle, smem_copy_params *params, smem_bm_copy_type t, uint32_t flags)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params != nullptr, "params is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", SM_NOT_INITIALIZED);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    return entry->DataCopy(params->src, params->dest, params->dataSize, t, flags);
}

SMEM_API int32_t smem_bm_copy_batch(smem_bm_t handle, smem_batch_copy_params *params, smem_bm_copy_type t,
                                    uint32_t flags)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(params != nullptr, "params is null", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", SM_NOT_INITIALIZED);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    return entry->DataCopyBatch(params, t, flags);
}

SMEM_API int32_t smem_bm_wait(smem_bm_t handle)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", SM_NOT_INITIALIZED);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    return entry->Wait();
}

SMEM_API uint32_t smem_bm_get_rank_id_by_gva(smem_bm_t handle, void *gva)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", SM_NOT_INITIALIZED);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    return entry->GetRankIdByGva(gva);
}

SMEM_API int32_t smem_bm_register_user_mem(smem_bm_t handle, uint64_t addr, uint64_t size)
{
    SM_VALIDATE_RETURN(handle != nullptr, "invalid param, handle is NULL", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(addr != 0, "invalid param, addr eq 0", SM_INVALID_PARAM);
    SM_VALIDATE_RETURN(g_smemBmInited, "smem bm not initialized yet", SM_NOT_INITIALIZED);

    SmemBmEntryPtr entry = nullptr;
    auto ret = SmemBmEntryManager::Instance().GetEntryByPtr(reinterpret_cast<uintptr_t>(handle), entry);
    if (ret != SM_OK || entry == nullptr) {
        SM_LOG_AND_SET_LAST_ERROR("input handle is invalid, result: " << ret);
        return SM_INVALID_PARAM;
    }

    return entry->RegisterMem(addr, size);
}
