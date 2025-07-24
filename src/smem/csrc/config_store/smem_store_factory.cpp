/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include <nlohmann/json.hpp>
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
acclinkTlsOption StoreFactory::tlsOption_;
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
        SM_LOG_INFO("Startup for store(url=" << ip << ":" << port << ", isSever=" << isServer << ", rank=" << rankId
                                             << ") address in use");
        failedReason_ = SM_RESOURCE_IN_USE;
        return nullptr;
    }
    if (ret != 0) {
        SM_LOG_ERROR("Startup for store(url=" << ip << ":" << port << ", isSever=" << isServer << ", rank=" << rankId
                                              << ") failed:" << ret);
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
        if (j.contains(#key) && j[#key].is_string()) {          \
            std::string path = j[#key];                         \
            if (!CommonFunc::Realpath(path)) {                  \
                return StoreErrorCode::ERROR;                   \
            }                                                   \
            tlsOption.key = path;                               \
        }                                                       \
    } while (0)

#define GET_ARRAY_FILE(key, topPath)                                                                \
    do {                                                                                            \
        if (j.contains(#key) && j[#key].is_array()) {                                               \
            auto files = j[#key].get<std::vector<std::string>>();                                   \
            std::set<std::string> tlsFileSet = std::set<std::string>(files.begin(), files.end());   \
            for (const std::string &file : tlsFileSet) {                                            \
                std::string filePath = (topPath) + (file);                                          \
                std::string filePathCheck = filePath;                                               \
                if (!CommonFunc::Realpath(filePath) || filePath != filePathCheck) {                 \
                    SM_LOG_ERROR("Failed to check tlsFiles");                                       \
                    return StoreErrorCode::ERROR;                                                   \
                }                                                                                   \
            }                                                                                       \
            tlsOption.key = tlsFileSet;                                                             \
        }                                                                                           \
    } while (0)

Result ParseSSLFromJson(const char* tlsJsonInfo, acclinkTlsOption &tlsOption)
{
    using json = nlohmann::json;

    try {
        json j = json::parse(tlsJsonInfo);
        GET_STRING_PATH(tlsCert);
        GET_STRING_PATH(tlsCrlPath);
        GET_STRING_PATH(tlsCaPath);
        GET_STRING_PATH(tlsPk);
        GET_STRING_PATH(tlsPkPwd);
        GET_STRING_PATH(packagePath);
        GET_ARRAY_FILE(tlsCaFile, tlsOption.tlsCaPath);
        GET_ARRAY_FILE(tlsCrlFile, tlsOption.tlsCrlPath);
        return StoreErrorCode::SUCCESS;
    } catch (const json::parse_error& e) {
        SM_LOG_ERROR("Failed to parse TLS config JSON: " << e.what());
        return StoreErrorCode::ERROR;
    }
}

Result StoreFactory::InitTlsOption() noexcept
{
    bool isTlsEnable = true;
    auto tlsEnableEnv = std::getenv("MEMFABRIC_HYBRID_TLS_ENABLE");
    if (tlsEnableEnv != nullptr) {
        std::string tlsEnableStr = tlsEnableEnv;
        if (tlsEnableStr == "0") {
            isTlsEnable = false;
        } else if (tlsEnableStr == "1") {
            isTlsEnable = true;
        } else {
            SM_LOG_INFO("Environment variable MEMFABRIC_HYBRID_TLS_ENABLE must be 0 or 1.");
            return StoreErrorCode::ERROR;
        }
    }
    tlsOption_.enableTls = isTlsEnable;

    if (!tlsOption_.enableTls) {
        SM_LOG_INFO("tls is not enabled.");
        return StoreErrorCode::SUCCESS;
    }

    const char* tlsJsonInfo = std::getenv("MEMFABRIC_HYBRID_TLS_INFO");
    if (tlsJsonInfo == nullptr) {
        SM_LOG_ERROR("set ssl option failed, environment MEMFABRIC_HYBRID_TLS_INFO not set. ");
        return StoreErrorCode::ERROR;
    }

    size_t jsonLen = std::strlen(tlsJsonInfo);
    if (jsonLen > MAX_TLS_JSON_LEN) {
        SM_LOG_ERROR("MEMFABRIC_HYBRID_TLS_INFO is too long (" << jsonLen << " bytes)");
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