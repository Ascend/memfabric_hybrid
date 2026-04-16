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
#ifndef MF_HYBRID_DLETCD_API_H
#define MF_HYBRID_DLETCD_API_H

#include "smem_common_includes.h"
#include "smem_etcd_c_define.h"

namespace ock {
namespace smem {

using EtcdNewFunc = EtcdClient *(*)(const char *endpoints, const char *username, const char *password,
                                    int timeoutSeconds);

using EtcdCloseFunc = void (*)(EtcdClient *client);

using EtcdGetLastErrorFunc = char *(*)(EtcdClient * client);

using EtcdPutFunc = int (*)(EtcdClient *client, const char *key, const void *value, size_t valueLen,
                            int64_t ttlSeconds);

using EtcdGetFunc = int (*)(EtcdClient *client, const char *key, char **outValue, size_t *outValueLen);

using EtcdPrefixGetFunc = int (*)(EtcdClient *client, const smem_store_prefix_get_ctx_t *ctx, uint32_t flags);

using EtcdFreeValueFunc = void (*)(const char *value);

using EtcdRemoveFunc = int (*)(EtcdClient *client, const char *key);

using EtcdLockFunc = int (*)(EtcdClient *client);

using EtcdLockNamedFunc = int (*)(EtcdClient *client, const char *lockName);

using EtcdUnLockFunc = int (*)(EtcdClient *client);

class EtcdApi {
public:
    [[nodiscard]] static Result LoadLibrary();
    static void CleanupLibrary() noexcept;

    [[nodiscard]] static inline EtcdClient *EtcdNew(const char *endpoints, const char *username, const char *password,
                                                    int timeoutSeconds) noexcept
    {
        SM_ASSERT_RETURN(etcdNew_ != nullptr, nullptr);
        return etcdNew_(endpoints, username, password, timeoutSeconds);
    }

    static inline void EtcdClose(EtcdClient *client) noexcept
    {
        SM_ASSERT_RET_VOID(etcdClose_ != nullptr);
        etcdClose_(client);
    }

    [[nodiscard]] static inline char *EtcdGetLastError(EtcdClient *client) noexcept
    {
        SM_ASSERT_RETURN(etcdGetLastError_ != nullptr, nullptr);
        return etcdGetLastError_(client);
    }

    [[nodiscard]] static inline int EtcdPut(EtcdClient *client, const char *key, const void *value, size_t valueLen,
                                            int64_t ttlSeconds) noexcept
    {
        SM_ASSERT_RETURN(etcdPut_ != nullptr, -1);
        return etcdPut_(client, key, value, valueLen, ttlSeconds);
    }

    [[nodiscard]] static inline int EtcdGet(EtcdClient *client, const char *key, char **outValue,
                                            size_t *outValueLen) noexcept
    {
        SM_ASSERT_RETURN(etcdGet_ != nullptr, -1);
        return etcdGet_(client, key, outValue, outValueLen);
    }

    [[nodiscard]] static inline int EtcdPrefixGet(EtcdClient *client, const smem_store_prefix_get_ctx_t *ctx,
                                                  uint32_t flags) noexcept
    {
        SM_ASSERT_RETURN(etcdPrefixGet_ != nullptr, -1);
        return etcdPrefixGet_(client, ctx, flags);
    }

    static inline void EtcdFreeValue(const char *value) noexcept
    {
        SM_ASSERT_RET_VOID(etcdFreeValue_ != nullptr);
        etcdFreeValue_(value);
    }

    [[nodiscard]] static inline int EtcdRemove(EtcdClient *client, const char *key) noexcept
    {
        SM_ASSERT_RETURN(etcdRemove_ != nullptr, -1);
        return etcdRemove_(client, key);
    }

    [[nodiscard]] static inline int EtcdLock(EtcdClient *client) noexcept
    {
        SM_ASSERT_RETURN(etcdLock_ != nullptr, -1);
        return etcdLock_(client);
    }

    [[nodiscard]] static inline int EtcdLockNamed(EtcdClient *client, const char *lockName) noexcept
    {
        SM_ASSERT_RETURN(etcdLockNamed_ != nullptr, -1);
        return etcdLockNamed_(client, lockName);
    }

    [[nodiscard]] static inline int EtcdUnLock(EtcdClient *client) noexcept
    {
        SM_ASSERT_RETURN(etcdUnLock_ != nullptr, -1);
        return etcdUnLock_(client);
    }

    [[nodiscard]] static inline bool SupportsNamedLock() noexcept
    {
        return etcdLockNamed_ != nullptr;
    }

private:
    static std::mutex mutex_;
    static bool loaded_;
    static void *libraryHandle_;
    static constexpr auto kLibraryName = "libetcd_client_v3.so";

    static EtcdNewFunc etcdNew_;
    static EtcdCloseFunc etcdClose_;
    static EtcdGetLastErrorFunc etcdGetLastError_;
    static EtcdPutFunc etcdPut_;
    static EtcdGetFunc etcdGet_;
    static EtcdPrefixGetFunc etcdPrefixGet_;
    static EtcdFreeValueFunc etcdFreeValue_;
    static EtcdRemoveFunc etcdRemove_;
    static EtcdLockFunc etcdLock_;
    static EtcdLockNamedFunc etcdLockNamed_;
    static EtcdUnLockFunc etcdUnLock_;
};

} // namespace smem
} // namespace ock

#endif // MF_HYBRID_DLETCD_API_H
