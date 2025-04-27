/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef SMEM_SMEM_CONFIG_STORE_H
#define SMEM_SMEM_CONFIG_STORE_H

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

#include "smem_common_includes.h"

namespace ock {
namespace smem {
enum StoreErrorCode : int16_t {
    SUCCESS = SM_OK,
    ERROR = SM_ERROR,
    INVALID_MESSAGE = -400,
    INVALID_KEY = -401,
    NOT_EXIST = -404,
    TIMEOUT = -601,
    IO_ERROR = -602
};

class ConfigStore : public SmReferable {
public:
    ~ConfigStore() override = default;

public:
    /**
     * @brief Set string value
     * @param key          [in] key to be set
     * @param value        [in] value to be set
     * @return 0 if successfully done
     */
    Result Set(const std::string &key, const std::string &value) noexcept;

    /**
     * @brief Get string value with key
     *
     * @param key          [in] key to be got
     * @param value        [out] value to be got
     * @param timeoutMs    [in] timeout
     * @return 0 if successfully done
     */
    Result Get(const std::string &key, std::string &value, int64_t timeoutMs = -1) noexcept;

    /**
     * @brief Get vector value with key
     *
     * @param key          [in] key to be got
     * @param value        [out] value to be got
     * @param timeoutMs    [in] timeout
     * @return 0 if successfully done
     */
    Result Get(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs = -1) noexcept;

    /**
     * @brief Set vector value
     *
     * @param key          [in] key to be set
     * @param value        [in] value to be set
     * @return 0 if successfully done
     */
    virtual Result Set(const std::string &key, const std::vector<uint8_t> &value) noexcept = 0;

    /**
     * @brief Add integer value
     *
     * @param key          [in] key to be increased
     * @param increment    [in] value to be increased
     * @param value        [out] value after increased
     * @return 0 if successfully done
     */
    virtual Result Add(const std::string &key, int64_t increment, int64_t &value) noexcept = 0;

    /**
     * @brief Remove a key
     *
     * @param key          [in] key to be removed
     * @return 0 if successfully done
     */
    virtual Result Remove(const std::string &key) noexcept = 0;

    /**
     * @brief Append string to a key with string value
     *
     * @param key          [in] key to be appended
     * @param value        [in] value to be appended
     * @param newSize      [out] new size of value after appended
     * @return 0 if successfully done
     */
    Result Append(const std::string &key, const std::string &value, uint64_t &newSize) noexcept;

    /**
     * @brief Append char/int8 vector to a key with char/int8 value
     *
     * @param key          [in] key to be appended
     * @param value        [in] value to be appended
     * @param newSize      [out] new size of value after appended
     * @return 0 if successfully done
     */
    virtual Result Append(const std::string &key, const std::vector<uint8_t> &value, uint64_t &newSize) noexcept = 0;

    /**
     * @brief Get error string by code
     *
     * @param errCode      [in] error cde
     * @return error string
     */
    static const char *ErrStr(int16_t errCode);

protected:
    virtual Result GetReal(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept = 0;
    static constexpr uint32_t MAX_KEY_LEN_CLIENT = 1024U;
};
using StorePtr = SmRef<ConfigStore>;

inline Result ConfigStore::Set(const std::string &key, const std::string &value) noexcept
{
    return Set(key, std::vector<uint8_t>(value.begin(), value.end()));
}

inline Result ConfigStore::Get(const std::string &key, std::string &value, int64_t timeoutMs) noexcept
{
    std::vector<uint8_t> u8val;
    auto ret = GetReal(key, u8val, timeoutMs);
    if (ret != 0) {
        return ret;
    }

    value = std::string(u8val.begin(), u8val.end());
    return 0;
}

inline Result ConfigStore::Get(const std::string &key, std::vector<uint8_t> &value, int64_t timeoutMs) noexcept
{
    return GetReal(key, value, timeoutMs);
}

inline Result ConfigStore::Append(const std::string &key, const std::string &value, uint64_t &newSize) noexcept
{
    std::vector<uint8_t> u8val(value.begin(), value.end());
    return Append(key, u8val, newSize);
}

inline const char *ConfigStore::ErrStr(int16_t errCode)
{
    switch (errCode) {
        case SUCCESS:
            return "success";
        case ERROR:
            return "error";
        case INVALID_MESSAGE:
            return "invalid message";
        case INVALID_KEY:
            return "invalid key";
        case NOT_EXIST:
            return "key not exists";
        case TIMEOUT:
            return "timeout";
        case IO_ERROR:
            return "socket error";
        default:
            return "unknown error";
    }
}

}  // namespace smem
}  // namespace ock

#endif  // SMEM_SMEM_CONFIG_STORE_H
