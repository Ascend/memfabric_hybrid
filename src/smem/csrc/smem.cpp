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
#include <atomic>
#include "smem_common_includes.h"
#include "smem_version.h"
#include "smem_net_common.h"
#include "hybm.h"
#include "acc_log.h"
#include "smem_store_factory.h"
#include "smem_last_error.h"
#include "smem_external_backend_registry.h"
#include "smem.h"

namespace {
bool g_smemInited = false;

bool IsValidBackendOp(const smem_conf_store_backend_op_t &backendOp)
{
    return backendOp.distributed != nullptr && backendOp.create != nullptr && backendOp.destroy != nullptr &&
           backendOp.put != nullptr && backendOp.get != nullptr && backendOp.remove != nullptr &&
           backendOp.lock != nullptr && backendOp.try_lock != nullptr && backendOp.unlock != nullptr;
}
}

SMEM_API int32_t smem_init(uint32_t flags)
{
    using namespace ock::smem;
    g_smemInited = true;
    SM_LOG_INFO("smem init successfully, " << LIB_VERSION);
    return SM_OK;
}

SMEM_API int32_t smem_create_config_store(const char *storeUrl, uint64_t flags)
{
    static std::atomic<uint32_t> callNum = {0};

    if (storeUrl == nullptr) {
        SM_LOG_ERROR("input store URL is null.");
        return ock::smem::SM_INVALID_PARAM;
    }

    ock::smem::UrlExtraction extraction;
    auto ret = extraction.ExtractIpPortFromUrl(storeUrl);
    if (ret != 0) {
        SM_LOG_ERROR("input store URL invalid.");
        return ock::smem::SM_INVALID_PARAM;
    }
    auto store = ock::smem::StoreFactory::CreateStoreByUrl(storeUrl, true);
    if (store == nullptr) {
        SM_LOG_ERROR("create store server failed with URL.");
        return ock::smem::SM_ERROR;
    }

    if (callNum.fetch_add(1U) == 0) {
        pthread_atfork([]() {}, // 父进程 fork 前：释放锁等资源
                       []() {}, // 父进程 fork 后：无特殊操作
                       []() { ock::smem::StoreFactory::DestroyStoreAll(true); });
    }

    return ock::smem::SM_OK;
}

SMEM_API void smem_destroy_config_store(const char *storeUrl)
{
    if (storeUrl == nullptr) {
        SM_LOG_WARN("input store URL is null.");
        return;
    }
    ock::smem::StoreFactory::DestroyStore(storeUrl);
}

SMEM_API int32_t smem_config_store_set_backend_op(const smem_conf_store_backend_op_t *backendOp)
{
    if (backendOp == nullptr) {
        SM_LOG_ERROR("backend operation set is null.");
        return ock::smem::SM_INVALID_PARAM;
    }
    if (!IsValidBackendOp(*backendOp)) {
        SM_LOG_ERROR("backend operation set is invalid.");
        return ock::smem::SM_INVALID_PARAM;
    }

    ock::smem::SmemExternalBackendRegistry::SetExternalBackendOp(*backendOp);
    return ock::smem::SM_OK;
}

SMEM_API void smem_uninit()
{
    g_smemInited = false;
}

SMEM_API int32_t smem_set_extern_logger(void (*func)(int, const char *))
{
    SM_VALIDATE_RETURN(func != nullptr, "set extern logger failed, invalid func which is NULL",
                       ock::smem::SM_INVALID_PARAM);
    ock::mf::OutLogger::Instance().SetExternalLogFunction(func, true);
    hybm_set_extern_logger(func);
    return ock::smem::SM_OK;
}

SMEM_API int32_t smem_set_extern_alarm(void (*alarm)(uint16_t code, const char *msg), void (*resume)(uint16_t code))
{
    SM_VALIDATE_RETURN(alarm != nullptr, "set extern logger failed, invalid func which is NULL",
                       ock::smem::SM_INVALID_PARAM);
    ock::mf::OutLogger::Instance().SetAlarmLogFunction(alarm, resume, true);
    hybm_set_extern_alarm(alarm, resume);
    return ock::smem::SM_OK;
}

SMEM_API int32_t smem_set_log_level(int level)
{
    return hybm_set_log_level(level);
}

SMEM_API int32_t smem_set_conf_store_tls(bool enable, const char *tls_info, const uint32_t tls_info_len)
{
    return ock::smem::SM_OK;
}

SMEM_API const char *smem_get_last_err_msg()
{
    return ock::smem::SmLastError::GetAndClear(false);
}

SMEM_API const char *smem_get_and_clear_last_err_msg()
{
    return ock::smem::SmLastError::GetAndClear(true);
}

SMEM_API int32_t smem_set_config_store_tls_key(const char *tls_pk, const uint32_t tls_pk_len, const char *tls_pk_pw,
                                               const uint32_t tls_pk_pw_len, const smem_decrypt_handler h)
{
    return ock::smem::SM_OK;
}
