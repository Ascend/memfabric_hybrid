/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef SMEM_SMEM_CONFIG_STORE_H
#define SMEM_SMEM_CONFIG_STORE_H

#include <cstdint>
#include <vector>
#include <chrono>
#include <string>
#include <memory>

namespace ock {
namespace smem {

enum ErrorCode : int16_t {
    SUCCESS = 0,
    ERROR = -1,
    INVALID_MESSAGE = -400,
    INVALID_KEY = -401,
    NOT_EXIST = -404,
    TIMEOUT = -601,
    IO_ERROR = -602
};

class ConfigStore {
public:
    virtual ~ConfigStore() = default;

public:
    int Set(const std::string &key, const std::string &value) noexcept
    {
        return Set(key, std::vector<uint8_t>(value.begin(), value.end()));
    }

    virtual int Set(const std::string &key, const std::vector<uint8_t> &value) noexcept = 0;

    int Get(const std::string &key, std::string &value, int64_t timeoutMs = -1) noexcept
    {
        std::vector<uint8_t> u8val;
        auto ret = GetReal(key, u8val, timeoutMs);
        if (ret != 0) {
            return ret;
        }

        value = std::string(u8val.begin(), u8val.end());
        return 0;
    }

    int Get(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs = -1) noexcept
    {
        return GetReal(key, value, timeoutMs);
    }

    virtual int Add(const std::string &key, int64_t increment, int64_t &value) noexcept = 0;
    virtual int Remove(const std::string &key) noexcept = 0;

    int Append(const std::string &key, const std::string &value, uint64_t &newSize) noexcept
    {
        std::vector<uint8_t> u8val(value.begin(), value.end());
        return Append(key, u8val, newSize);
    }

    virtual int Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept = 0;

protected:
    virtual int GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs = -1) noexcept = 0;
    static constexpr uint32_t MAX_KEY_LEN_CLIENT = 1024U;
};

using StorePtr = std::shared_ptr<ConfigStore>;
}
}

#endif  // SMEM_SMEM_CONFIG_STORE_H
