/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "smem.h"
#include "smem_common_includes.h"
#include "smem_version.h"
#include "smem_net_common.h"
#include "hybm.h"
#include "acc_links/net/acc_log.h"
#include "smem_store_factory.h"

namespace {
bool g_smemInited = false;
}

SMEM_API int32_t smem_init(uint32_t flags)
{
    using namespace ock::smem;

    /* create logger instance */
    SMOutLogger::Instance();

    g_smemInited = true;

    // 未init时不能给hybm更新log配置,所以在init后延迟更新
    auto func = SMOutLogger::Instance().GetLogExtraFunc();
    if (func != nullptr) {
        (void)smem_set_extern_logger(func);
    }
    (void)smem_set_log_level(SMOutLogger::Instance().GetLogLevel());

    SM_LOG_INFO("smem init successfully, " << LIB_VERSION);
	
    return SM_OK;
}

SMEM_API int32_t smem_create_config_store(const char *storeUrl)
{
    static std::atomic<uint32_t> callNum = { 0 };

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

    auto store = ock::smem::StoreFactory::CreateStore(extraction.ip, extraction.port, true);
    if (store == nullptr) {
        SM_LOG_ERROR("create store server failed with URL.");
        return ock::smem::SM_ERROR;
    }

    if (callNum.fetch_add(1U) == 0) {
        pthread_atfork(
            []() {},  // 父进程 fork 前：释放锁等资源
            []() {},  // 父进程 fork 后：无特殊操作
            []() { ock::smem::StoreFactory::DestroyStoreAll(true); }
        );
    }

    return ock::smem::SM_OK;
}

SMEM_API void smem_uninit()
{
    g_smemInited = false;
    return;
}

SMEM_API int32_t smem_set_extern_logger(void (*fun)(int, const char *))
{
    using namespace ock::smem;

    SM_VALIDATE_RETURN(fun != nullptr, "set extern logger failed, invalid func which is NULL", SM_INVALID_PARAM);

    /* set my out logger */
    SMOutLogger::Instance().SetExternalLogFunction(fun, true);

    /* set dependent acc_links log function */
    auto result = AccSetExternalLog(fun);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("set acc_links log function failed, result: " << result);
        return result;
    }

    /* set dependent hybm core log function */
    if (g_smemInited) {
        result = hybm_set_extern_logger(fun);
        if (result != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("set hybm core log function failed, result: " << result);
            return result;
        }
    }

    return SM_OK;
}

SMEM_API int32_t smem_set_log_level(int level)
{
    using namespace ock::smem;

    SM_VALIDATE_RETURN((level >= 0 && level < LogLevel::BUTT_LEVEL),
                       "set log level failed, invalid param, level should be 0~3", SM_INVALID_PARAM);

    /* set my logger's level */
    SMOutLogger::Instance().SetLogLevel(static_cast<LogLevel>(level));

    /* set acc_links log level */
    auto result = AccSetLogLevel(level);
    if (result != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("set acc_links log level failed, result: " << result);
        return result;
    }

    /* set hybm core log level */
    if (g_smemInited) {
        result = hybm_set_log_level(level);
        if (result != SM_OK) {
            SM_LOG_AND_SET_LAST_ERROR("set hybm core log level failed, result: " << result);
            return result;
        }
    }

    return SM_OK;
}

SMEM_API const char *smem_get_last_err_msg()
{
    return ock::smem::SmLastError::GetAndClear(false);
}

SMEM_API const char *smem_get_and_clear_last_err_msg()
{
    return ock::smem::SmLastError::GetAndClear(true);
}

SMEM_API int32_t smem_register_decrypt_handler(const smem_decrypt_handler h)
{
    if (h == nullptr) {
        SM_LOG_ERROR("decrypt handler is nullptr");
        return ock::smem::SM_ERROR;
    }
    ock::smem::StoreFactory::RegisterDecryptHandler(h);
    return ock::smem::SM_OK;
}
