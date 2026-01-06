/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/

#ifndef SMEM_SMEM_STORE_FACTORY_H
#define SMEM_SMEM_STORE_FACTORY_H

#include <mutex>
#include <string>
#include <unordered_map>

#include "smem_bm_def.h"
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
    static StorePtr CreateStore(const std::string &ip, uint16_t port, bool isServer, uint32_t worldSize = 0,
                                int32_t rankId = -1, int32_t connMaxRetry = -1) noexcept;

    static StorePtr CreateStoreServer(const std::string &ip, uint16_t port, uint32_t worldSize = UINT32_MAX,
                                      int32_t rankId = -1, int32_t connMaxRetry = -1) noexcept;

    static StorePtr CreateStoreClient(const std::string &ip, uint16_t port, uint32_t worldSize = 1024,
                                      int32_t rankId = -1, int32_t connMaxRetry = -1) noexcept;

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

    static void SetTlsInfo(const smem_tls_config &tlsOption) noexcept;

private:
    static std::mutex storesMutex_;
    static std::unordered_map<std::string, StorePtr> storesMap_;
    static smem_tls_config tlsOption_;
    static bool enableTls;
    static std::string tlsInfo;
    static std::string tlsPkInfo;
    static std::string tlsPkPwdInfo;
};
} // namespace smem
} // namespace ock

#endif // SMEM_SMEM_STORE_FACTORY_H
