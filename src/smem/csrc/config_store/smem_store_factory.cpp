/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include <iostream>
#include <vector>
#include <sstream>
#include "smem_logger.h"
#include "smem_tcp_config_store.h"
#include "smem_prefix_config_store.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {
static __thread int failedReason_ = 0;
static constexpr size_t MAX_TLS_INFO_LEN = 10 * 1024U;
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

std::string TrimString(const std::string &input)
{
    auto start = input.begin();
    while (start != input.end() && std::isspace(*start)) {
        start++;
    }

    auto end = input.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));

    return std::string(start, end + 1);
}

Result ParseStr2Array(const std::string &token, char splitter, std::set<std::string> &parts)
{
    std::istringstream tokenSteam(token);
    std::string part;
    while (std::getline(tokenSteam, part, splitter)) {
        part = TrimString(part);
        if (!part.empty()) {
            parts.insert(part);
        }
    }

    if (parts.empty()) {
        SM_LOG_WARN("parse token to array failed");
        return StoreErrorCode::ERROR;
    }
    return StoreErrorCode::SUCCESS;
}

Result ParseStr2KV(const std::string &token, char splitter, std::pair<std::string, std::string> &pair)
{
    std::istringstream stm(token);
    std::string key;
    std::string value;
    if (std::getline(stm, key, splitter) && std::getline(stm, value, splitter)) {
        key = TrimString(key);
        value = TrimString(value);
        if (!key.empty() && !value.empty()) {
            pair.first = key;
            pair.second = value;
            return StoreErrorCode::SUCCESS;
        }
    }

    SM_LOG_WARN("parse token to kv failed");
    return StoreErrorCode::ERROR;
}

void SetTlsOptionValue(AcclinkTlsOption &tlsOption, const std::string &key, const std::string &value)
{
    if (key == "tlsCaPath") {
        tlsOption.tlsCaPath = value;
    } else if (key == "tlsCert") {
        tlsOption.tlsCert = value;
    } else if (key == "tlsPk") {
        tlsOption.tlsPk = value;
    } else if (key == "tlsPkPwd") {
        tlsOption.tlsPkPwd = value;
    } else if (key == "tlsCrlPath") {
        tlsOption.tlsCrlPath = value;
    } else if (key == "packagePath") {
        tlsOption.packagePath = value;
    }
}

void SetTlsOptionValues(AcclinkTlsOption &tlsOption, const std::string &key, std::set<std::string> &values)
{
    if (key == "tlsCrlFile") {
        tlsOption.tlsCrlFile = values;
    } else if (key == "tlsCaFile") {
        tlsOption.tlsCaFile = values;
    }
}

Result ParseTlsInfo(const char* tlsInput, AcclinkTlsOption &tlsOption)
{
    std::string inputStr(tlsInput);
    std::istringstream tokenSteam(inputStr);
    std::vector<std::string> tokens;
    std::string token;

    while (std::getline(tokenSteam, token, ';')) {
        if (!TrimString(token).empty()) {
            tokens.push_back(token);
        }
    }

    for (std::string &t : tokens) {
        std::pair<std::string, std::string> pair;
        auto ret = ParseStr2KV(t, ':', pair);
        if (ret != StoreErrorCode::SUCCESS) {
            continue;
        }

        auto key = pair.first;
        std::set<std::string> paths;
        if (pair.first == "tlsCrlFile" || pair.first == "tlsCaFile") {
            ret = ParseStr2Array(pair.second, ',', paths);
            if (ret != StoreErrorCode::SUCCESS) {
                continue;
            }

            SetTlsOptionValues(tlsOption, pair.first, paths);
        } else {
            SetTlsOptionValue(tlsOption, pair.first, pair.second);
        }
    }

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

    const char* tlsInfo = std::getenv("SMEM_CONF_STORE_TLS_INFO");
    if (tlsInfo == nullptr) {
        SM_LOG_ERROR("set ssl option failed, environment SMEM_CONF_STORE_TLS_INFO not set. ");
        return StoreErrorCode::ERROR;
    }

    size_t infoLen = std::strlen(tlsInfo);
    if (infoLen > MAX_TLS_INFO_LEN) {
        SM_LOG_ERROR("SMEM_CONF_STORE_TLS_INFO is too long (" << infoLen << " bytes)");
        return StoreErrorCode::ERROR;
    }

    if (ParseTlsInfo(tlsInfo, tlsOption_) != StoreErrorCode::SUCCESS) {
        SM_LOG_ERROR("extract ssl info from input failed.");
        return StoreErrorCode::ERROR;
    }

    isTlsInitialized_ = true;
    return StoreErrorCode::SUCCESS;
}

std::function<int(const std::string&, char*, int&)> StoreFactory::ConvertFunc(int (*rawFunc)(const char*, int*, char*, int*)) noexcept
{
    return [rawFunc](const std::string &cipherText, char *plainText, int32_t &plainTextLen) {
        int tmpCipherLen = static_cast<int>(cipherText.size());
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