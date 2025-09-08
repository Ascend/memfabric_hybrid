/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include <iostream>
#include <vector>
#include <sstream>
#include "smem_logger.h"
#include "smem_tcp_config_store.h"
#include "smem_prefix_config_store.h"
#include "mf_string_util.h"
#include "smem_store_factory.h"

namespace ock {
namespace smem {
static __thread int failedReason_ = 0;
static constexpr size_t MAX_TLS_INFO_LEN = 10 * 1024U;
bool StoreFactory::enableTls = true;
std::string StoreFactory::tlsInfo;
std::string StoreFactory::tlsPkInfo;
std::string StoreFactory::tlsPkPwdInfo;
std::mutex StoreFactory::storesMutex_;
std::unordered_map<std::string, StorePtr> StoreFactory::storesMap_;
AcclinkTlsOption StoreFactory::tlsOption_;
bool StoreFactory::isTlsInitialized_ = false;
std::thread StoreFactory::cleanerThread_;
std::atomic<bool> StoreFactory::timerRunning_{false};
std::condition_variable StoreFactory::cv_;
std::atomic<bool> StoreFactory::stop_{false};

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
    TlsCleanUp();
    ShutDownCleanupThread();
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
    TlsCleanUp();
    ShutDownCleanupThread();
}

void StoreFactory::TlsCleanUp() noexcept
{
    StoreFactory::tlsPkInfo = "";
    StoreFactory::tlsPkPwdInfo = "";
    tlsOption_.tlsPk = "";
    tlsOption_.tlsPkPwd = "";
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

Result ParseStr2Array(const std::string &token, char splitter, std::set<std::string> &parts)
{
    std::istringstream tokenSteam(token);
    std::string part;
    while (std::getline(tokenSteam, part, splitter)) {
        part = ock::mf::StringUtil::TrimString(part);
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
        key = ock::mf::StringUtil::TrimString(key);
        value = ock::mf::StringUtil::TrimString(value);
        if (!key.empty() && !value.empty()) {
            pair.first = key;
            pair.second = value;
            return StoreErrorCode::SUCCESS;
        }
    }

    SM_LOG_WARN("parse token to kv failed");
    return StoreErrorCode::ERROR;
}

bool SetTlsOptionValue(AcclinkTlsOption &tlsOption, const std::string &key, const std::string &value)
{
    if (key == "tlsCaPath") {
        tlsOption.tlsCaPath = value;
    } else if (key == "tlsCert") {
        tlsOption.tlsCert = value;
    } else if (key == "tlsCrlPath") {
        tlsOption.tlsCrlPath = value;
    } else if (key == "packagePath") {
        tlsOption.packagePath = value;
    } else {
        return false;
    }
    return true;
}

bool SetTlsOptionValues(AcclinkTlsOption &tlsOption, const std::string &key, std::set<std::string> &values)
{
    if (key == "tlsCrlFile") {
        tlsOption.tlsCrlFile = values;
    } else if (key == "tlsCaFile") {
        tlsOption.tlsCaFile = values;
    } else {
        return false;
    }
    return true;
}

Result ParseTlsInfo(const std::string &inputStr, AcclinkTlsOption &tlsOption)
{
    std::istringstream tokenSteam(inputStr);
    std::vector<std::string> tokens;
    std::string token;

    while (std::getline(tokenSteam, token, ';')) {
        if (!ock::mf::StringUtil::TrimString(token).empty()) {
            tokens.push_back(token);
        }
    }

    for (std::string &t : tokens) {
        std::pair<std::string, std::string> pair;
        auto ret = ParseStr2KV(t, ':', pair);
        if (ret != StoreErrorCode::SUCCESS) {
            continue;
        }

        bool res = true;
        auto key = pair.first;
        std::set<std::string> paths;
        if (pair.first == "tlsCrlFile" || pair.first == "tlsCaFile") {
            ret = ParseStr2Array(pair.second, ',', paths);
            if (ret != StoreErrorCode::SUCCESS) {
                continue;
            }

            res = SetTlsOptionValues(tlsOption, pair.first, paths);
        } else {
            res = SetTlsOptionValue(tlsOption, pair.first, pair.second);
        }

        if (!res) {
            SM_LOG_WARN("un-match tls info key " << pair.first);
        }
    }

    return StoreErrorCode::SUCCESS;
}

Result StoreFactory::InitTlsOption() noexcept
{
    tlsOption_.enableTls = StoreFactory::enableTls;

    if (!tlsOption_.enableTls) {
        SM_LOG_INFO("tls is not enabled.");
        return StoreErrorCode::SUCCESS;
    }

    tlsOption_.tlsPk = StoreFactory::tlsPkInfo;
    tlsOption_.tlsPkPwd = StoreFactory::tlsPkPwdInfo;
    if (ParseTlsInfo(StoreFactory::tlsInfo, tlsOption_) != StoreErrorCode::SUCCESS) {
        SM_LOG_ERROR("extract ssl info from input failed.");
        return StoreErrorCode::ERROR;
    }

    isTlsInitialized_ = true;
    return StoreErrorCode::SUCCESS;
}

std::function<int(const std::string&, char*, size_t&)> StoreFactory::ConvertFunc(int (*rawFunc)(const char*,
    size_t, char*, size_t&)) noexcept
{
    return [rawFunc](const std::string &cipherText, char *plainText, size_t &plainTextLen) {
        auto tmpCipherLen = cipherText.size();
        int ret = rawFunc(cipherText.c_str(), tmpCipherLen, plainText, plainTextLen);
        return ret;
    };
}

int32_t StoreFactory::SetTlsInfo(bool enable, const char *tlsData, const size_t tlsDataLen) noexcept
{
    enableTls = enable;
    if (!enable) {
        return StoreErrorCode::SUCCESS;
    }

    if (tlsData == nullptr || tlsDataLen > MAX_TLS_INFO_LEN) {
        SM_LOG_ERROR("tls info null or len invalid.");
        return StoreErrorCode::ERROR;
    }

    StoreFactory::tlsInfo = std::string(tlsData, tlsDataLen);
    return StoreErrorCode::SUCCESS;
}

int32_t StoreFactory::SetTlsPkInfo(const char *tlsPk, const uint32_t tlsPkLen, const char *tlsPkPwd,
    const uint32_t tlsPkPwLen, const smem_decrypt_handler &h) noexcept
{
    if (timerRunning_.exchange(true)) {
        SM_LOG_WARN("TLS private key has been set multiple times");
        return StoreErrorCode::SUCCESS;
    }
    if (tlsPk == nullptr || tlsPkLen > MAX_TLS_INFO_LEN) {
        SM_LOG_ERROR("tls private key is null or len invalid.");
        return StoreErrorCode::ERROR;
    }

    if (tlsPkPwd == nullptr) {
        SM_LOG_INFO("tls private key password is null.");
        StoreFactory::tlsPkPwdInfo = "";
    } else {
        if (tlsPkPwLen > MAX_TLS_INFO_LEN) {
            SM_LOG_ERROR("tls private key password len invalid.");
            return StoreErrorCode::ERROR;
        }
        StoreFactory::tlsPkPwdInfo = std::string(tlsPkPwd, tlsPkPwLen);
    }
    StoreFactory::tlsPkInfo = std::string(tlsPk, tlsPkLen);

    if (h != nullptr) {
        tlsOption_.decryptHandler_ = ConvertFunc(h);
    }

    stop_ = false;
    cleanerThread_ = std::thread([]() {
        std::unique_lock<std::mutex> lockGuard{storesMutex_};
        // after one hour of the TLS private key being set, clean up the sensitive information stored in memory.
        if (!cv_.wait_for(lockGuard, std::chrono::hours(1), [] { return stop_.load(); })) {
            TlsCleanUp();
        }
    });

    return StoreErrorCode::SUCCESS;
}

void StoreFactory::ShutDownCleanupThread() noexcept
{
    if (timerRunning_) {
        {
            std::lock_guard<std::mutex> lockGuard{storesMutex_};
            stop_ = true;
        }
        cv_.notify_one();
        if (cleanerThread_.joinable()) {
            cleanerThread_.join();
        }
        timerRunning_ = false;
    }
}
}  // namespace smem
}  // namespace ock