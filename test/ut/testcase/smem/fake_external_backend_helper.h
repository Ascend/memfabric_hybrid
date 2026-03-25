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

#ifndef TEST_UT_TESTCASE_SMEM_FAKE_EXTERNAL_BACKEND_HELPER_H_
#define TEST_UT_TESTCASE_SMEM_FAKE_EXTERNAL_BACKEND_HELPER_H_

#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "smem_def.h"
#include "smem_external_backend_registry.h"

namespace ock {
namespace smem {
namespace test {

// Fake handle for tracking per-backend state
struct FakeExternalHandle {
    std::string name;
    std::string prefix;
    std::unordered_map<std::string, std::vector<uint8_t>> kv;
    std::unordered_set<std::string> locks;
};

// Environment state for controlling fake backend behavior
struct FakeExternalBackendEnv {
    bool distributed = true;
    int32_t createRet = SMEM_STORE_BACKEND_CODE_OK;
    int32_t forcedPutRet = SMEM_STORE_BACKEND_CODE_OK;
    int32_t forcedGetRet = SMEM_STORE_BACKEND_CODE_OK;
    int32_t forcedRemoveRet = SMEM_STORE_BACKEND_CODE_OK;
    int32_t forcedLockRet = SMEM_STORE_BACKEND_CODE_OK;
    int32_t forcedTryLockRet = SMEM_STORE_BACKEND_CODE_OK;
    int32_t forcedUnlockRet = SMEM_STORE_BACKEND_CODE_OK;
    int createCount = 0;
    int destroyCount = 0;
    int getCount = 0;
    std::string lastName;
    std::string lastPrefix;
    std::string lastPutKey;
    std::string lastGetKey;
    std::string lastRemoveKey;
    std::string lastLockName;
    std::string lastTryLockName;
    std::string lastUnlockName;
    std::string lastResolvedPutKey;
    std::string lastResolvedGetKey;
    std::string lastResolvedRemoveKey;
    std::string lastResolvedLockName;
    std::string lastResolvedTryLockName;
    std::string lastResolvedUnlockName;
    FakeExternalHandle *lastHandle = nullptr;
};

// Global fake environment, reset between tests
inline FakeExternalBackendEnv &GetFakeEnv()
{
    static FakeExternalBackendEnv env;
    return env;
}

inline void ResetFakeExternalEnv()
{
    GetFakeEnv() = {};
    SmemExternalBackendRegistry::ResetExternalBackendOp();
}

std::string NormalizeFakeText(const char *text);

FakeExternalHandle *AsFakeHandle(void *handle);

void SetFakeHandle(void **handle, FakeExternalHandle *state);

int32_t ReturnCreateFailure(void **handle, int32_t createRet);

int32_t CreateFakeHandle(void **handle, FakeExternalBackendEnv &env);

std::string BuildScopedFakeName(const FakeExternalHandle &state, const std::string &name);

std::vector<uint8_t> MakeFakeBytes(const void *value, uint64_t size);

void SetFakeSize(uint64_t *size, uint64_t valueSize);

void CopyFakeBytes(const std::vector<uint8_t> &src, void *dst);

int32_t WriteFakeValue(FakeExternalHandle &state, const std::string &key, const void *value, uint64_t size);

int32_t ReadFakeValue(FakeExternalHandle &state, const std::string &key, void *value, uint64_t capacity,
                      uint64_t *size);

int32_t LockFakeHandle(FakeExternalHandle &state, const std::string &name);

int32_t TryLockFakeHandle(FakeExternalHandle &state, const std::string &name);

int32_t UnlockFakeHandle(FakeExternalHandle &state, const std::string &name);

bool FakeDistributed(uint32_t flags);

int32_t FakeCreate(const char *name, const char *prefix, uint32_t flags, void **handle);

void FakeDestroy(void *handle);

int32_t FakePut(void *handle, const char *key, const void *value, uint64_t size, uint32_t flags);

int32_t FakeGet(void *handle, const char *key, void *value, uint64_t capacity, uint32_t flags, uint64_t *size);

int32_t FakeRemove(void *handle, const char *key, uint32_t flags);

int32_t FakeLock(void *handle, const char *name, uint32_t flags);

int32_t FakeTryLock(void *handle, const char *name, uint32_t flags);

int32_t FakeUnlock(void *handle, const char *name, uint32_t flags);

smem_conf_store_backend_op_t MakeFakeBackendOp();

}  // namespace test
}  // namespace smem
}  // namespace ock

#endif  // TEST_UT_TESTCASE_SMEM_FAKE_EXTERNAL_BACKEND_HELPER_H_
