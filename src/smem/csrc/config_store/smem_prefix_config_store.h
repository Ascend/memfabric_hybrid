/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef SMEM_SMEM_PREFIX_CONFIG_STORE_H
#define SMEM_SMEM_PREFIX_CONFIG_STORE_H

#include <memory>
#include "smem_config_store.h"

namespace ock {
namespace smem {

class PrefixConfigStore : public ConfigStore {
public:
    PrefixConfigStore(std::shared_ptr<ConfigStore> base, std::string prefix) noexcept
        : baseStore_{std::move(base)},
          keyPrefix_{std::move(prefix)}
    {
    }

    int Set(const std::string &key, const std::vector<uint8_t> &value) noexcept override
    {
        return baseStore_->Set(std::string(keyPrefix_).append(key), value);
    }

    int Add(const std::string &key, int64_t increment, int64_t &value) noexcept override
    {
        return baseStore_->Add(std::string(keyPrefix_).append(key), increment, value);
    }

    int Remove(const std::string &key) noexcept override
    {
        return baseStore_->Remove(std::string(keyPrefix_).append(key));
    }

    int Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept override
    {
        return baseStore_->Append(std::string(keyPrefix_).append(key), value, newSize);
    }

protected:
    int GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept override
    {
        return baseStore_->Get(std::string(keyPrefix_).append(key), value, timeoutMs);
    }

private:
    const std::shared_ptr<ConfigStore> baseStore_;
    const std::string keyPrefix_;
};
}
}

#endif  // SMEM_SMEM_PREFIX_CONFIG_STORE_H
