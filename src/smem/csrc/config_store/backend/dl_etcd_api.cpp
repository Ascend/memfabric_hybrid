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
#include "dl_etcd_api.h"
#include <dlfcn.h>

namespace ock {
namespace smem {
bool EtcdApi::loaded_ = false;
std::mutex EtcdApi::mutex_;
void *EtcdApi::libraryHandle_ = nullptr;

EtcdNewFunc EtcdApi::etcdNew_ = nullptr;
EtcdCloseFunc EtcdApi::etcdClose_ = nullptr;
EtcdGetLastErrorFunc EtcdApi::etcdGetLastError_ = nullptr;
EtcdPutFunc EtcdApi::etcdPut_ = nullptr;
EtcdGetFunc EtcdApi::etcdGet_ = nullptr;
EtcdFreeValueFunc EtcdApi::etcdFreeValue_ = nullptr;
EtcdRemoveFunc EtcdApi::etcdRemove_ = nullptr;
EtcdLockFunc EtcdApi::etcdLock_ = nullptr;
EtcdUnLockFunc EtcdApi::etcdUnLock_ = nullptr;

namespace {

template<typename FuncType>
bool LoadSymbol(void *handle, const char *symbolName, FuncType &funcPtr)
{
    funcPtr = reinterpret_cast<FuncType>(dlsym(handle, symbolName));
    if (funcPtr == nullptr) {
        SM_LOG_ERROR("Failed to load symbol " << symbolName << ", error: " << dlerror());
        return false;
    }
    return true;
}

} // namespace

Result EtcdApi::LoadLibrary()
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (loaded_) {
        return SM_OK;
    }
    libraryHandle_ = dlopen(kLibraryName, RTLD_NOW | RTLD_NODELETE);
    if (libraryHandle_ == nullptr) {
        SM_LOG_ERROR("Failed to open library [" << kLibraryName << "], error: " << dlerror());
        return SM_ERROR;
    }
    bool success = true;
    success = success && LoadSymbol(libraryHandle_, "Etcd_New", etcdNew_);
    success = success && LoadSymbol(libraryHandle_, "Etcd_Close", etcdClose_);
    success = success && LoadSymbol(libraryHandle_, "Etcd_GetLastError", etcdGetLastError_);
    success = success && LoadSymbol(libraryHandle_, "Etcd_Put", etcdPut_);
    success = success && LoadSymbol(libraryHandle_, "Etcd_Get", etcdGet_);
    success = success && LoadSymbol(libraryHandle_, "Etcd_FreeValue", etcdFreeValue_);
    success = success && LoadSymbol(libraryHandle_, "Etcd_Remove", etcdRemove_);
    success = success && LoadSymbol(libraryHandle_, "Etcd_Lock", etcdLock_);
    success = success && LoadSymbol(libraryHandle_, "Etcd_UnLock", etcdUnLock_);
    if (!success) {
        dlclose(libraryHandle_);
        libraryHandle_ = nullptr;
        etcdNew_ = nullptr;
        etcdClose_ = nullptr;
        etcdGetLastError_ = nullptr;
        etcdPut_ = nullptr;
        etcdGet_ = nullptr;
        etcdFreeValue_ = nullptr;
        etcdRemove_ = nullptr;
        etcdLock_ = nullptr;
        etcdUnLock_ = nullptr;
        return SM_ERROR;
    }
    loaded_ = true;
    return SM_OK;
}

void EtcdApi::CleanupLibrary() noexcept
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!loaded_) {
        return;
    }
    etcdNew_ = nullptr;
    etcdClose_ = nullptr;
    etcdGetLastError_ = nullptr;
    etcdPut_ = nullptr;
    etcdGet_ = nullptr;
    etcdFreeValue_ = nullptr;
    etcdRemove_ = nullptr;
    etcdLock_ = nullptr;
    etcdUnLock_ = nullptr;
    if (libraryHandle_ != nullptr) {
        dlclose(libraryHandle_);
        libraryHandle_ = nullptr;
    }
    loaded_ = false;
}

} // namespace smem
} // namespace ock