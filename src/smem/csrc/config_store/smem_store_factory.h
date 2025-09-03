/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef SMEM_SMEM_STORE_FACTORY_H
#define SMEM_SMEM_STORE_FACTORY_H

#include <set>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <condition_variable>
#include <atomic>
#include "smem.h"
#include "smem_config_store.h"

namespace ock {
namespace smem {

class StoreFactory {
public:
    /**
     * @brief Create a new store
     * @param ip server ip address
     * @param port server tcp port
     * @param isServer is local store server side
     * @param rankId rank id, default 0
     * @param connMaxRetry Maximum number of retry times for the client to connect to the server.
     * @return Newly created store
     */
    static StorePtr CreateStore(const std::string &ip, uint16_t port, bool isServer, int32_t rankId = 0,
        int32_t connMaxRetry = -1) noexcept;

    /**
     * @brief Destroy on exist store
     * @param ip server ip address
     * @param port server tcp port
     */
    static void DestroyStore(const std::string &ip, uint16_t port) noexcept;
    static void DestroyStoreAll(bool afterFork = false) noexcept;

    /**
     * @brief Encapsulate an existing store into a prefix store.
     * @param base existing store
     * @param prefix Prefix of keys
     * @return prefix store.
     */
    static StorePtr PrefixStore(const StorePtr &base, const std::string &prefix) noexcept;

    static int GetFailedReason() noexcept;

    /**
     * @brief Init and set tls info.
     * @param enable whether to enable tls
     * @param tlsInfo the tls config info
     * @param tlsInfoLen the length of tls config info
     * @return Returns 0 on success or an error code on failure
     */
    static int32_t SetTlsInfo(bool enable, const char *tlsInfo, const size_t tlsInfoLen) noexcept;

    /**
     * @brief Set the TLS private key and password.
     * @param enable whether to enable tls
     * @param tlsInfo the tls config info
     * @param tlsInfoLen the length of tls config info
     * @param tlsInfo the tls config info
     * @param tlsInfoLen the length of tls config info
     * @return Returns 0 on success or an error code on failure
     */
    static int32_t SetTlsPkInfo(const char *tlsPk, const uint32_t tlsPkLen, const char *tlsPkPwd,
        const uint32_t tlsPkPwLen, const smem_decrypt_handler &h) noexcept;

    static void ShutDownCleanupThread() noexcept;

private:
    static Result InitTlsOption() noexcept;
    static std::function<int(const std::string&, char*, size_t&)> ConvertFunc(int (*rawFunc)(const char*,
        size_t, char*, size_t &)) noexcept;
    static void TlsCleanUp() noexcept;
    static bool enableTls;
    static std::string tlsInfo;
    static std::string tlsPkInfo;
    static std::string tlsPkPwdInfo;

private:
    static std::mutex storesMutex_;
    static std::unordered_map<std::string, StorePtr> storesMap_;
    static AcclinkTlsOption tlsOption_;
    static bool isTlsInitialized_;
    static std::thread cleanerThread_;
    static std::atomic<bool> timerRunning_;
    static std::condition_variable cv_;
    static std::atomic<bool> stop_;
};
} // namespace smem
} // namespace ock

#endif // SMEM_SMEM_STORE_FACTORY_H
