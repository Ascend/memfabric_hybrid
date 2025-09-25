/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef SMEM_SMEM_STORE_FACTORY_H
#define SMEM_SMEM_STORE_FACTORY_H

#include <mutex>
#include <string>
#include <unordered_map>
#include "smem_config_store.h"
#include "mf_tls_def.h"

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
    static StorePtr CreateStore(const std::string &ip, uint16_t port, bool isServer,
                                uint32_t worldSize = 0, int32_t rankId = -1, int32_t connMaxRetry = -1) noexcept;

    static StorePtr CreateStoreServer(const std::string &ip, uint16_t port, uint32_t worldSize = 0,
                                      int32_t rankId = -1, int32_t connMaxRetry = -1) noexcept;

    static StorePtr CreateStoreClient(const std::string &ip, uint16_t port, uint32_t worldSize = 0,
                                      int32_t rankId = -1, int32_t connMaxRetry = -1) noexcept;

    /**
     * @brief destroy on exist store
     * @param ip server ip address
     * @param port server tcp port
     */
    static void DestroyStore(const std::string &ip, uint16_t port) noexcept;

    /**
     * @brief Encapsulate an existing store into a prefix store.
     * @param base existing store
     * @param prefix Prefix of keys
     * @return prefix store.
     */
    static StorePtr PrefixStore(const StorePtr &base, const std::string &prefix) noexcept;

    static int GetFailedReason() noexcept;

    static void SetExternalLogFunction(void (* func)(int, const char *)) noexcept;

    static void SetLogLevel(int level) noexcept;

    static void SetTlsInfo(const tls_config& tlsOption) noexcept;

private:
    static std::mutex storesMutex_;
    static std::unordered_map<std::string, StorePtr> storesMap_;
    static tls_config tlsOption_;
};
} // namespace smem
} // namespace ock

#endif // SMEM_SMEM_STORE_FACTORY_H
