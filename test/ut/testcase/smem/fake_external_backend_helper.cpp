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

#include "fake_external_backend_helper.h"

namespace ock {
namespace smem {
namespace test {

std::string NormalizeFakeText(const char *text)
{
    return text == nullptr ? "" : text;
}

FakeExternalHandle *AsFakeHandle(void *handle)
{
    return reinterpret_cast<FakeExternalHandle *>(handle);
}

void SetFakeHandle(void **handle, FakeExternalHandle *state)
{
    if (handle != nullptr) {
        *handle = state;
    }
}

int32_t ReturnCreateFailure(void **handle, int32_t createRet)
{
    SetFakeHandle(handle, nullptr);
    return createRet;
}

int32_t CreateFakeHandle(void **handle, FakeExternalBackendEnv &env)
{
    auto *state = new FakeExternalHandle();
    state->name = env.lastName;
    state->prefix = env.lastPrefix;
    env.lastHandle = state;
    SetFakeHandle(handle, state);
    return SMEM_STORE_BACKEND_CODE_OK;
}

std::string BuildScopedFakeName(const FakeExternalHandle &state, const std::string &name)
{
    if (state.prefix.empty()) {
        return name;
    }
    if (name.compare(0, state.prefix.size(), state.prefix) == 0) {
        return name;
    }

    std::string scopedName = state.prefix;
    if (!scopedName.empty() && scopedName.back() != '/' && !name.empty() && name.front() != '/') {
        scopedName.push_back('/');
    } else if (!scopedName.empty() && scopedName.back() == '/' && !name.empty() && name.front() == '/') {
        scopedName.pop_back();
    }
    scopedName.append(name);
    return scopedName;
}

std::vector<uint8_t> MakeFakeBytes(const void *value, uint64_t size)
{
    const auto *begin = reinterpret_cast<const uint8_t *>(value);
    return std::vector<uint8_t>(begin, begin + size);
}

void SetFakeSize(uint64_t *size, uint64_t valueSize)
{
    if (size != nullptr) {
        *size = valueSize;
    }
}

void CopyFakeBytes(const std::vector<uint8_t> &src, void *dst)
{
    if (!src.empty()) {
        std::memcpy(dst, src.data(), src.size());
    }
}

int32_t WriteFakeValue(FakeExternalHandle &state, const std::string &key, const void *value, uint64_t size)
{
    if (value == nullptr || size == 0) {
        state.kv[key].clear();
        return SMEM_STORE_BACKEND_CODE_OK;
    }
    state.kv[key] = MakeFakeBytes(value, size);
    return SMEM_STORE_BACKEND_CODE_OK;
}

int32_t ReadFakeValue(FakeExternalHandle &state, const std::string &key, void *value, uint64_t capacity,
                      uint64_t *size)
{
    const auto iter = state.kv.find(key);
    if (iter == state.kv.end()) {
        SetFakeSize(size, 0);
        return SMEM_STORE_BACKEND_CODE_NOENT;
    }
    SetFakeSize(size, iter->second.size());
    if (capacity < iter->second.size()) {
        return SMEM_STORE_BACKEND_CODE_BUFEX;
    }
    CopyFakeBytes(iter->second, value);
    return SMEM_STORE_BACKEND_CODE_OK;
}

int32_t LockFakeHandle(FakeExternalHandle &state, const std::string &name)
{
    state.locks.insert(name);
    return SMEM_STORE_BACKEND_CODE_OK;
}

int32_t TryLockFakeHandle(FakeExternalHandle &state, const std::string &name)
{
    if (state.locks.count(name) != 0) {
        return SMEM_STORE_BACKEND_CODE_LOCKED;
    }
    state.locks.insert(name);
    return SMEM_STORE_BACKEND_CODE_OK;
}

int32_t UnlockFakeHandle(FakeExternalHandle &state, const std::string &name)
{
    if (state.locks.erase(name) == 0) {
        return SMEM_STORE_BACKEND_CODE_UNLOCKED;
    }
    return SMEM_STORE_BACKEND_CODE_OK;
}

bool FakeDistributed(uint32_t flags)
{
    (void)flags;
    return GetFakeEnv().distributed;
}

int32_t FakeCreate(const char *name, const char *prefix, uint32_t flags, void **handle)
{
    (void)flags;
    auto &env = GetFakeEnv();
    env.lastName = NormalizeFakeText(name);
    env.lastPrefix = NormalizeFakeText(prefix);
    env.createCount++;
    return env.createRet == SMEM_STORE_BACKEND_CODE_OK ? CreateFakeHandle(handle, env)
                                                       : ReturnCreateFailure(handle, env.createRet);
}

void FakeDestroy(void *handle)
{
    GetFakeEnv().destroyCount++;
    delete reinterpret_cast<FakeExternalHandle *>(handle);
}

int32_t FakePut(void *handle, const char *key, const void *value, uint64_t size, uint32_t flags)
{
    (void)flags;
    auto &env = GetFakeEnv();
    env.lastPutKey = NormalizeFakeText(key);
    env.lastResolvedPutKey = BuildScopedFakeName(*AsFakeHandle(handle), env.lastPutKey);
    if (env.forcedPutRet != SMEM_STORE_BACKEND_CODE_OK) {
        return env.forcedPutRet;
    }
    return WriteFakeValue(*AsFakeHandle(handle), env.lastResolvedPutKey, value, size);
}

int32_t FakeGet(void *handle, const char *key, void *value, uint64_t capacity, uint32_t flags, uint64_t *size)
{
    (void)flags;
    auto &env = GetFakeEnv();
    env.getCount++;
    env.lastGetKey = NormalizeFakeText(key);
    env.lastResolvedGetKey = BuildScopedFakeName(*AsFakeHandle(handle), env.lastGetKey);
    if (env.forcedGetRet != SMEM_STORE_BACKEND_CODE_OK) {
        return env.forcedGetRet;
    }
    return ReadFakeValue(*AsFakeHandle(handle), env.lastResolvedGetKey, value, capacity, size);
}

int32_t FakeRemove(void *handle, const char *key, uint32_t flags)
{
    (void)flags;
    auto &env = GetFakeEnv();
    env.lastRemoveKey = NormalizeFakeText(key);
    env.lastResolvedRemoveKey = BuildScopedFakeName(*AsFakeHandle(handle), env.lastRemoveKey);
    if (env.forcedRemoveRet != SMEM_STORE_BACKEND_CODE_OK) {
        return env.forcedRemoveRet;
    }

    return AsFakeHandle(handle)->kv.erase(env.lastResolvedRemoveKey) > 0 ? SMEM_STORE_BACKEND_CODE_OK
                                                                         : SMEM_STORE_BACKEND_CODE_NOENT;
}

int32_t FakeLock(void *handle, const char *name, uint32_t flags)
{
    (void)flags;
    auto &env = GetFakeEnv();
    env.lastLockName = NormalizeFakeText(name);
    env.lastResolvedLockName = BuildScopedFakeName(*AsFakeHandle(handle), env.lastLockName);
    if (env.forcedLockRet != SMEM_STORE_BACKEND_CODE_OK) {
        return env.forcedLockRet;
    }
    return LockFakeHandle(*AsFakeHandle(handle), env.lastResolvedLockName);
}

int32_t FakeTryLock(void *handle, const char *name, uint32_t flags)
{
    (void)flags;
    auto &env = GetFakeEnv();
    env.lastTryLockName = NormalizeFakeText(name);
    env.lastResolvedTryLockName = BuildScopedFakeName(*AsFakeHandle(handle), env.lastTryLockName);
    if (env.forcedTryLockRet != SMEM_STORE_BACKEND_CODE_OK) {
        return env.forcedTryLockRet;
    }
    return TryLockFakeHandle(*AsFakeHandle(handle), env.lastResolvedTryLockName);
}

int32_t FakeUnlock(void *handle, const char *name, uint32_t flags)
{
    (void)flags;
    auto &env = GetFakeEnv();
    env.lastUnlockName = NormalizeFakeText(name);
    env.lastResolvedUnlockName = BuildScopedFakeName(*AsFakeHandle(handle), env.lastUnlockName);
    if (env.forcedUnlockRet != SMEM_STORE_BACKEND_CODE_OK) {
        return env.forcedUnlockRet;
    }
    return UnlockFakeHandle(*AsFakeHandle(handle), env.lastResolvedUnlockName);
}

int32_t FakePrefixGet(void *handle, const smem_store_prefix_get_ctx_t *ctx, uint32_t flags)
{
    (void)handle;
    (void)flags;
    (void)ctx;
    return 0;
}

smem_conf_store_backend_op_t MakeFakeBackendOp()
{
    return {
        FakeDistributed, FakeCreate, FakeDestroy, FakePut, FakeGet,
        FakePrefixGet, FakeRemove, FakeLock, FakeTryLock, FakeUnlock
    };
}

}  // namespace test
}  // namespace smem
}  // namespace ock
