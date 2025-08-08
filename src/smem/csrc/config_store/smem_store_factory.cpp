/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include "smem_logger.h"
#include "smem_tcp_config_store.h"
#include "smem_prefix_config_store.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {
static __thread int failedReason_ = 0;
static constexpr size_t MAX_TLS_JSON_LEN = 10 * 1024U;
std::mutex StoreFactory::storesMutex_;
std::unordered_map<std::string, StorePtr> StoreFactory::storesMap_;
AcclinkTlsOption StoreFactory::tlsOption_;
bool StoreFactory::isTlsInitialized_ = false;

StorePtr StoreFactory::CreateStore(const std::string &ip, uint16_t port, bool isServer, int32_t rankId,
                                   int32_t connMaxRetry) noexcept
{
    std::string storeKey = std::string(ip).append(":").append(std::to_string(port));

    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    auto pos = storesMap_.find(storeKey);
    if (pos != storesMap_.end()) {
        return pos->second;
    }

    auto store = SmMakeRef<TcpConfigStore>(ip, port, isServer, rankId);
    SM_ASSERT_RETURN(store != nullptr, nullptr);

    if (!isTlsInitialized_ && InitTlsOption() != StoreErrorCode::SUCCESS) {
        SM_LOG_ERROR("init tls option failed. ");
        return nullptr;
    }

    auto ret = store->Startup(tlsOption_, connMaxRetry);
    if (ret == SM_RESOURCE_IN_USE) {
        SM_LOG_INFO("Startup for store(isSever=" << isServer << ", rank=" << rankId << ") address in use");
        failedReason_ = SM_RESOURCE_IN_USE;
        return nullptr;
    }
    if (ret != 0) {
        SM_LOG_ERROR("Startup for store(isSever=" << isServer << ", rank=" << rankId << ") failed:" << ret);
        failedReason_ = ret;
        return nullptr;
    }

    storesMap_.emplace(storeKey, store.Get());
    lockGuard.unlock();

    return store.Get();
}

void StoreFactory::DestroyStore(const std::string &ip, uint16_t port) noexcept
{
    std::string storeKey = std::string(ip).append(":").append(std::to_string(port));
    std::unique_lock<std::mutex> lockGuard{storesMutex_};
    storesMap_.erase(storeKey);
}

void StoreFactory::DestroyStoreAll(bool afterFork) noexcept
{
    if (afterFork) {
        for (auto &e : storesMap_) {
            Convert<ConfigStore, TcpConfigStore>(e.second)->Shutdown(afterFork);
        }
    } else {
        std::unique_lock<std::mutex> lockGuard{storesMutex_};
        for (auto &e: storesMap_) {
            Convert<ConfigStore, TcpConfigStore>(e.second)->Shutdown(afterFork);
        }
    }
    storesMap_.clear();
}

StorePtr StoreFactory::PrefixStore(const ock::smem::StorePtr &base, const std::string &prefix) noexcept
{
    SM_VALIDATE_RETURN(base != nullptr, "invalid param, base is nullptr", nullptr);

    auto store = SmMakeRef<PrefixConfigStore>(base, prefix);
    SM_ASSERT_RETURN(store != nullptr, nullptr);

    return store.Get();
}

int StoreFactory::GetFailedReason() noexcept
{
    return failedReason_;
}

#define GET_STRING_PATH(key)                                    \
    do {                                                        \
        if (doc.HasMember(#key) && doc[#key].IsString()) {      \
            std::string path = doc[#key].GetString();           \
            tlsOption.key = path;                               \
        }                                                       \
    } while (0)

#define GET_ARRAY_FILE(key)                                                       \
    do {                                                                          \
        if (doc.HasMember(#key) && doc[#key].IsArray()) {                         \
            std::set<std::string> tlsFileSet;                                     \
            for (rapidjson::SizeType i = 0; i < doc[#key].Size(); i++) {          \
                if (!doc[#key].IsString()) {                                      \
                    SM_LOG_ERROR("tlsFiles array contains non-stirng element");   \
                    return StoreErrorCode::ERROR;                                 \
                }                                                                 \
                tlsFileSet.insert(doc[#key][i].GetString());                      \
            }                                                                     \
            tlsOption.key = tlsFileSet;                                           \
        }                                                                         \
    } while (0)

Result ParseSSLFromJson(const char* tlsJsonInfo, AcclinkTlsOption &tlsOption)
{
    rapidjson::Document doc;
    if (doc.Parse(tlsJsonInfo).HasParseError()) {
        rapidjson::ParseErrorCode errorCode = doc.GetParseError();
        size_t errorOffset = doc.GetErrorOffset();
        const char *errorMsg = rapidjson::GetParseError_En(errorCode);
        SM_LOG_ERROR("Failed to parse TLS config JSON with error: " << errorMsg << " at offset " << errorOffset);
        return StoreErrorCode::ERROR;
    }
    GET_STRING_PATH(tlsCert);
    GET_STRING_PATH(tlsCrlPath);
    GET_STRING_PATH(tlsCaPath);
    GET_STRING_PATH(tlsPk);
    GET_STRING_PATH(tlsPkPwd);
    GET_STRING_PATH(packagePath);
    GET_ARRAY_FILE(tlsCaFile);
    GET_ARRAY_FILE(tlsCrlFile);
    return StoreErrorCode::SUCCESS;
}

Result StoreFactory::InitTlsOption() noexcept
{
    bool isTlsEnable = true;
    auto tlsEnableEnv = std::getenv("SMEM_CONF_STORE_TLS_ENABLE");
    if (tlsEnableEnv != nullptr) {
        std::string tlsEnableStr = tlsEnableEnv;
        if (tlsEnableStr == "0") {
            isTlsEnable = false;
        } else if (tlsEnableStr == "1") {
            isTlsEnable = true;
        } else {
            SM_LOG_INFO("Environment variable SMEM_CONF_STORE_TLS_ENABLE must be 0 or 1.");
            return StoreErrorCode::ERROR;
        }
    }
    tlsOption_.enableTls = isTlsEnable;

    if (!tlsOption_.enableTls) {
        SM_LOG_INFO("tls is not enabled.");
        return StoreErrorCode::SUCCESS;
    }

    const char* tlsJsonInfo = std::getenv("SMEM_CONF_STORE_TLS_INFO");
    if (tlsJsonInfo == nullptr) {
        SM_LOG_ERROR("set ssl option failed, environment SMEM_CONF_STORE_TLS_INFO not set. ");
        return StoreErrorCode::ERROR;
    }

    size_t jsonLen = std::strlen(tlsJsonInfo);
    if (jsonLen > MAX_TLS_JSON_LEN) {
        SM_LOG_ERROR("SMEM_CONF_STORE_TLS_INFO is too long (" << jsonLen << " bytes)");
        return StoreErrorCode::ERROR;
    }

    if (ParseSSLFromJson(tlsJsonInfo, tlsOption_) != StoreErrorCode::SUCCESS) {
        SM_LOG_ERROR("extract ssl info from json failed.");
        return StoreErrorCode::ERROR;
    }

    isTlsInitialized_ = true;
    return StoreErrorCode::SUCCESS;
}

std::function<int(const std::string&, char*, int&)> StoreFactory::ConvertFunc(int (*rawFunc)(const char*, int*, char*, int*)) noexcept
{
    return [rawFunc](const std::string &cipherText, char *plainText, int32_t &plainTextLen) {
        int tmpCipherLen = cipherText.size();
        int tmpPlainLen = plainTextLen;
        int ret = rawFunc(cipherText.c_str(), &tmpCipherLen, plainText, &tmpPlainLen);
        plainTextLen = tmpPlainLen;
        return ret;
    };
}

void StoreFactory::RegisterDecryptHandler(const smem_decrypt_handler &h) noexcept
{
    tlsOption_.decryptHandler_ = ConvertFunc(h);
}
}  // namespace smem
}  // namespace ock