/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef SMEM_SMEM_PREFIX_CONFIG_STORE_H
#define SMEM_SMEM_PREFIX_CONFIG_STORE_H

#include "smem_config_store.h"

namespace ock {
namespace smem {

class PrefixConfigStore : public ConfigStore {
public:
    PrefixConfigStore(const StorePtr &base, std::string prefix) noexcept
        : baseStore_{base->GetCoreStore()},
          keyPrefix_{base->GetCommonPrefix().append(std::move(prefix))}
    {
    }

    ~PrefixConfigStore() override = default;

    Result Set(const std::string &key, const std::vector<uint8_t> &value) noexcept override
    {
        STORE_ASSERT_RETURN(baseStore_ != nullptr, SM_MALLOC_FAILED);
        return baseStore_->Set(std::string(keyPrefix_).append(key), value);
    }

    Result Add(const std::string &key, int64_t increment, int64_t &value) noexcept override
    {
        STORE_ASSERT_RETURN(baseStore_ != nullptr, SM_MALLOC_FAILED);
        return baseStore_->Add(std::string(keyPrefix_).append(key), increment, value);
    }

    Result Remove(const std::string &key) noexcept override
    {
        STORE_ASSERT_RETURN(baseStore_ != nullptr, SM_MALLOC_FAILED);
        return baseStore_->Remove(std::string(keyPrefix_).append(key));
    }

    Result Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept override
    {
        STORE_ASSERT_RETURN(baseStore_ != nullptr, SM_MALLOC_FAILED);
        return baseStore_->Append(std::string(keyPrefix_).append(key), value, newSize);
    }

    Result Cas(const std::string &key, const std::vector<uint8_t> &expect, const std::vector<uint8_t> &value,
               std::vector<uint8_t> &exists) noexcept override
    {
        STORE_ASSERT_RETURN(baseStore_ != nullptr, SM_MALLOC_FAILED);
        return baseStore_->Cas(std::string(keyPrefix_).append(key), expect, value, exists);
    }

    Result Watch(const std::string &key,
                 const std::function<void(int result, const std::string &, const std::vector<uint8_t> &)> &notify,
                 uint32_t &wid) noexcept override
    {
        STORE_ASSERT_RETURN(baseStore_ != nullptr, SM_MALLOC_FAILED);
        return baseStore_->Watch(
            std::string(keyPrefix_).append(key),
            [key, notify](int result, const std::string &, const std::vector<uint8_t> &value) {
                notify(result, key, value);
            },
            wid);
    }

    Result Watch(WatchRankType type, const std::function<void(WatchRankType, uint32_t)> &notify,
                 uint32_t &wid) noexcept override
    {
        STORE_ASSERT_RETURN(baseStore_ != nullptr, SM_MALLOC_FAILED);
        return baseStore_->Watch(type, notify, wid);
    }

    Result Unwatch(uint32_t wid) noexcept override
    {
        STORE_ASSERT_RETURN(baseStore_ != nullptr, SM_MALLOC_FAILED);
        return baseStore_->Unwatch(wid);
    }

    std::string GetCompleteKey(const std::string &key) noexcept override
    {
        return std::string(keyPrefix_).append(key);
    }

    std::string GetCommonPrefix() noexcept override
    {
        STORE_ASSERT_RETURN(baseStore_ != nullptr, "");
        return keyPrefix_ + baseStore_->GetCommonPrefix();
    }

    StorePtr GetCoreStore() noexcept override
    {
        return baseStore_;
    }

    void RegisterReconnectHandler(ConfigStoreReconnectHandler callback) noexcept override
    {
        if (baseStore_ == nullptr) {
            return;
        }
        return baseStore_->RegisterReconnectHandler(callback);
    }

protected:
    Result GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept override
    {
        STORE_ASSERT_RETURN(baseStore_ != nullptr, SM_MALLOC_FAILED);
        return baseStore_->Get(std::string(keyPrefix_).append(key), value, timeoutMs);
    }

private:
    const StorePtr baseStore_;
    const std::string keyPrefix_;
};
}  // namespace smem
}  // namespace ock

#endif  // SMEM_SMEM_PREFIX_CONFIG_STORE_H