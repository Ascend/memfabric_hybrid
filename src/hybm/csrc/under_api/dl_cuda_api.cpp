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

#if defined(NVIDIA_GPU)
#include <dlfcn.h>
#include "dl_cuda_api.h"

namespace ock {
namespace mf {

bool DlCudaApi::gLoaded = false;
std::mutex DlCudaApi::gMutex;
void *DlCudaApi::rtHandle = nullptr;

cudaSetDeviceFunc DlCudaApi::pCudaSetDevice = nullptr;
cudaGetDeviceFunc DlCudaApi::pCudaGetDevice = nullptr;
cudaDeviceEnablePeerAccessFunc DlCudaApi::pCudaDeviceEnablePeerAccess = nullptr;
cudaStreamCreateFunc DlCudaApi::pCudaStreamCreate = nullptr;
cudaStreamCreateWithFlagsFunc DlCudaApi::pCudaStreamCreateWithFlags = nullptr;
cudaStreamCreateWithPriorityFunc DlCudaApi::pCudaStreamCreateWithPriority = nullptr;
cudaStreamDestroyFunc DlCudaApi::pCudaStreamDestroy = nullptr;
cudaStreamSynchronizeFunc DlCudaApi::pCudaStreamSynchronize = nullptr;
cudaMallocFunc DlCudaApi::pCudaMalloc = nullptr;
cudaFreeFunc DlCudaApi::pCudaFree = nullptr;
cudaMallocHostFunc DlCudaApi::pCudaMallocHost = nullptr;
cudaFreeHostFunc DlCudaApi::pCudaFreeHost = nullptr;
cudaMemcpyFunc DlCudaApi::pCudaMemcpy = nullptr;
cudaMemcpyAsyncFunc DlCudaApi::pCudaMemcpyAsync = nullptr;
cudaMemcpy2DFunc DlCudaApi::pCudaMemcpy2D = nullptr;
cudaMemcpy2DAsyncFunc DlCudaApi::pCudaMemcpy2DAsync = nullptr;
cudaMemsetFunc DlCudaApi::pCudaMemset = nullptr;

Result DlCudaApi::LoadLibrary(const std::string &libDirPath)
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }

    std::string realPath;
    if (!FileUtil::LibraryRealPath(libDirPath, std::string("libcudart.so"), realPath)) {
        BM_LOG_ERROR(libDirPath << " get lib path failed");
        return BM_ERROR;
    }

    rtHandle = dlopen(realPath.c_str(), RTLD_NOW);
    if (rtHandle == nullptr) {
        BM_LOG_ERROR("Failed to open library [" << realPath << "], error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    DL_LOAD_SYM(pCudaSetDevice, cudaSetDeviceFunc, rtHandle, "cudaSetDevice");
    DL_LOAD_SYM(pCudaGetDevice, cudaGetDeviceFunc, rtHandle, "cudaGetDevice");
    DL_LOAD_SYM(pCudaDeviceEnablePeerAccess, cudaDeviceEnablePeerAccessFunc, rtHandle, "cudaDeviceEnablePeerAccess");

    DL_LOAD_SYM(pCudaStreamCreate, cudaStreamCreateFunc, rtHandle, "cudaStreamCreate");
    DL_LOAD_SYM(pCudaStreamCreateWithFlags, cudaStreamCreateWithFlagsFunc, rtHandle, "cudaStreamCreateWithFlags");
    DL_LOAD_SYM(pCudaStreamCreateWithPriority, cudaStreamCreateWithPriorityFunc, rtHandle,
                "cudaStreamCreateWithPriority");
    DL_LOAD_SYM(pCudaStreamDestroy, cudaStreamDestroyFunc, rtHandle, "cudaStreamDestroy");
    DL_LOAD_SYM(pCudaStreamSynchronize, cudaStreamSynchronizeFunc, rtHandle, "cudaStreamSynchronize");

    DL_LOAD_SYM(pCudaMalloc, cudaMallocFunc, rtHandle, "cudaMalloc");
    DL_LOAD_SYM(pCudaFree, cudaFreeFunc, rtHandle, "cudaFree");
    DL_LOAD_SYM(pCudaMallocHost, cudaMallocHostFunc, rtHandle, "cudaMallocHost");
    DL_LOAD_SYM(pCudaFreeHost, cudaFreeHostFunc, rtHandle, "cudaFreeHost");

    DL_LOAD_SYM(pCudaMemcpy, cudaMemcpyFunc, rtHandle, "cudaMemcpy");
    DL_LOAD_SYM(pCudaMemcpyAsync, cudaMemcpyAsyncFunc, rtHandle, "cudaMemcpyAsync");
    DL_LOAD_SYM(pCudaMemcpy2D, cudaMemcpy2DFunc, rtHandle, "cudaMemcpy2D");
    DL_LOAD_SYM(pCudaMemcpy2DAsync, cudaMemcpy2DAsyncFunc, rtHandle, "cudaMemcpy2DAsync");

    DL_LOAD_SYM(pCudaMemset, cudaMemsetFunc, rtHandle, "cudaMemset");

    gLoaded = true;
    return BM_OK;
}

void DlCudaApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

    // Reset function pointers
    pCudaSetDevice = nullptr;
    pCudaGetDevice = nullptr;
    pCudaDeviceEnablePeerAccess = nullptr;

    pCudaStreamCreate = nullptr;
    pCudaStreamCreateWithFlags = nullptr;
    pCudaStreamCreateWithPriority = nullptr;
    pCudaStreamDestroy = nullptr;
    pCudaStreamSynchronize = nullptr;

    pCudaMalloc = nullptr;
    pCudaFree = nullptr;
    pCudaMallocHost = nullptr;
    pCudaFreeHost = nullptr;

    pCudaMemcpy = nullptr;
    pCudaMemcpyAsync = nullptr;
    pCudaMemcpy2D = nullptr;
    pCudaMemcpy2DAsync = nullptr;

    pCudaMemset = nullptr;

    if (rtHandle != nullptr) {
        dlclose(rtHandle);
        rtHandle = nullptr;
    }
    gLoaded = false;
}

} // namespace mf
} // namespace ock
#endif