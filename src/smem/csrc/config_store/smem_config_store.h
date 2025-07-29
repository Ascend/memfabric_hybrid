/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef SMEM_SMEM_CONFIG_STORE_H
#define SMEM_SMEM_CONFIG_STORE_H

#include <cstdint>
#include <vector>
#include <string>
#include <functional>

#include "acc_links/net/acc_def.h"
#include "acc_links/net/acc_tcp_server.h"
#include "smem_security.h"
#include "smem_common_includes.h"

struct AcclinkTlsOption {
    bool enableTls = true;
    std::string tlsTopPath = "";                  /* root path of certifications */
    std::string tlsCert;                          /* certification of server */
    std::string tlsCrlPath;                       /* optional, crl file path */
    std::string tlsCaPath;                        /* ca file path */
    std::set<std::string> tlsCaFile;              /* paths of ca */
    std::set<std::string> tlsCrlFile;             /* path of crl file */
    std::string tlsPk;                            /* private key */
    std::string tlsPkPwd;                         /* private key加密文件->可选传入 */
    ock::acc::AccDecryptHandler decryptHandler_;  /* private key decryptor */
    std::string packagePath;                      /* lib库路径 */
};


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
     * @brief Perform an atomic compare and swap for string type. That is, if the current value for <i>key</i> equals
     *        <i>expect</i>, then set the value of <i>key</i> to be <i>value</i>.
     * @param key          [in] key for performed
     * @param expect       [in] expected value for old, empty string equals non-exist
     * @param value        [in] value for set if expected matches
     * @param exists       [out] old value of the key before this operation
     * @return If the communication with the store server is successful, 0 is returned. Otherwise, non-zero is returned.
     *         Returning 0 does not indicate successful CAS. To determine whether the CAS is successful, compare
     *         <i>exists</i> and <i>expect</i>.
     */
    Result Cas(const std::string &key, const std::string &expect, const std::string &value,
               std::string &exists) noexcept;

    /**
     * @brief Perform an atomic compare and swap for uint8 vector. That is, if the current value for <i>key</i> equals
     *        <i>expect</i>, then set the value of <i>key</i> to be <i>value</i>.
     * @param key          [in] key for performed
     * @param expect       [in] expected value for old, empty vector equals non-exist
     * @param value        [in] value for set if expected matches
     * @param exists       [out] old value of the key before this operation
     * @return If the communication with the store server is successful, 0 is returned. Otherwise, non-zero is returned.
     *         Returning 0 does not indicate successful CAS. To determine whether the CAS is successful, compare
     *         <i>exists</i> and <i>expect</i>.
     */
    virtual Result Cas(const std::string &key, const std::vector<uint8_t> &expect, const std::vector<uint8_t> &value,
                       std::vector<uint8_t> &exists) noexcept = 0;

    /**
     * @brief Watch the specified non-existent key. When the key is created, the specified notify function is invoked.
     * @param key          [in] key to be watched
     * @param notify       [in] notify function when key is created.
     * @param wid          [out] Unique ID of the watch event.
     * @return 0 if successfully done
     */
    Result Watch(const std::string &key,
                 const std::function<void(int result, const std::string &, const std::string &)> &notify,
                 uint32_t &wid) noexcept;

    /**
     * @brief Watch the specified non-existent key. When the key is created, the specified notify function is invoked.
     * @param key          [in] key to be watched
     * @param notify       [in] notify function when key is created.
     * @param wid          [out] Unique ID of the watch event.
     * @return 0 if successfully done
     */
    virtual Result Watch(
        const std::string &key,
        const std::function<void(int result, const std::string &, const std::vector<uint8_t> &)> &notify,
        uint32_t &wid) noexcept = 0;

    /**
     * @brief Cancel an existed watcher.
     * @param wid          [in] Unique ID of the watch event.
     * @return 0 if successfully done
     */
    virtual Result Unwatch(uint32_t wid) noexcept = 0;

    /**
     * @brief Get error string by code
     *
     * @param errCode      [in] error cde
     * @return error string
     */
    static const char *ErrStr(int16_t errCode);

    virtual std::string GetCompleteKey(const std::string &key) noexcept = 0;

    virtual std::string GetCommonPrefix() noexcept = 0;

    virtual SmRef<ConfigStore> GetCoreStore() noexcept = 0;

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

inline Result ConfigStore::Cas(const std::string &key, const std::string &expect, const std::string &value,
                               std::string &exists) noexcept
{
    std::vector<uint8_t> u8expect{expect.begin(), expect.end()};
    std::vector<uint8_t> u8value{value.begin(), value.end()};
    std::vector<uint8_t> u8exists;
    auto ret = Cas(key, u8expect, u8value, u8exists);
    if (ret != SM_OK) {
        return ret;
    }

    exists = std::string{u8exists.begin(), u8exists.end()};
    return SM_OK;
}

inline Result ConfigStore::Watch(
    const std::string &key, const std::function<void(int result, const std::string &, const std::string &)> &notify,
    uint32_t &wid) noexcept
{
    return Watch(
        key,
        [notify](int res, const std::string &k, const std::vector<uint8_t> &v) {
            notify(res, k, std::string{v.begin(), v.end()});
        },
        wid);
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

inline ock::acc::AccTlsOption ConvertTlsOption(const AcclinkTlsOption &opt)
{
    ock::acc::AccTlsOption tlsOption;
    tlsOption.enableTls = opt.enableTls;
    tlsOption.tlsTopPath = opt.tlsTopPath;
    tlsOption.tlsCert = opt.tlsCert;
    tlsOption.tlsCaPath = opt.tlsCaPath;
    tlsOption.tlsCrlPath = opt.tlsCrlPath;
    tlsOption.tlsCaFile = opt.tlsCaFile;
    tlsOption.tlsCrlFile = opt.tlsCrlFile;
    tlsOption.tlsPk = opt.tlsPk;
    tlsOption.tlsPkPwd = opt.tlsPkPwd;
    return tlsOption;
}

}  // namespace smem
}  // namespace ock

#endif  // SMEM_SMEM_CONFIG_STORE_H
