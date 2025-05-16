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
          keyPrefix_{std::move(prefix).append(base->GetCommonPrefix())}
    {
    }

    ~PrefixConfigStore() override = default;

    Result Set(const std::string &key, const std::vector<uint8_t> &value) noexcept override
    {
        return baseStore_->Set(std::string(keyPrefix_).append(key), value);
    }

    Result Add(const std::string &key, int64_t increment, int64_t &value) noexcept override
    {
        return baseStore_->Add(std::string(keyPrefix_).append(key), increment, value);
    }

    Result Remove(const std::string &key) noexcept override
    {
        return baseStore_->Remove(std::string(keyPrefix_).append(key));
    }

    Result Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept override
    {
        return baseStore_->Append(std::string(keyPrefix_).append(key), value, newSize);
    }

    Result Cas(const std::string &key, const std::vector<uint8_t> &expect, const std::vector<uint8_t> &value,
               std::vector<uint8_t> &exists) noexcept override
    {
        return baseStore_->Cas(std::string(keyPrefix_).append(key), expect, value, exists);
    }

    std::string GetCompleteKey(const std::string &key) noexcept override
    {
        return std::string(keyPrefix_).append(key);
    }

    std::string GetCommonPrefix() noexcept override
    {
        return keyPrefix_ + baseStore_->GetCommonPrefix();
    }

    StorePtr GetCoreStore() noexcept override
    {
        return baseStore_;
    }

protected:
    Result GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept override
    {
        return baseStore_->Get(std::string(keyPrefix_).append(key), value, timeoutMs);
    }

private:
    const StorePtr baseStore_;
    const std::string keyPrefix_;
};
}  // namespace smem
}  // namespace ock

#endif  // SMEM_SMEM_PREFIX_CONFIG_STORE_H
