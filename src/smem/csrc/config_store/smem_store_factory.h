/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef SMEM_SMEM_STORE_FACTORY_H
#define SMEM_SMEM_STORE_FACTORY_H

#include <set>
#include <mutex>
#include <string>
#include <unordered_map>
#include <functional>
#include "smem_config_store.h"

namespace ock {
namespace smem {

class StoreFactory {
public:
    /**
     * @brief create a new store
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
     * @brief destroy on exist store
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

    static void RegisterDecryptHandler(const smem_decrypt_handler &h) noexcept;

private:
    static Result InitTlsOption() noexcept;
    static std::function<int(const std::string&, char*, int&)> ConvertFunc(int (*rawFunc)(const char*, int*, char*, int*)) noexcept;

private:
    static std::mutex storesMutex_;
    static std::unordered_map<std::string, StorePtr> storesMap_;
    static acclinkTlsOption tlsOption_;
    static bool isTlsInitialized_;
};
} // namespace smem
} // namespace ock

#endif // SMEM_SMEM_STORE_FACTORY_H
