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

#ifndef MF_HYBM_CORE_DL_CUDA_API_H
#define MF_HYBM_CORE_DL_CUDA_API_H
#include <cstddef>
#include <mutex>
#include "hybm_functions.h"

namespace ock {
namespace mf {
using cudaSetDeviceFunc = int (*)(int device);
using cudaGetDeviceFunc = int (*)(int *device);
using cudaDeviceEnablePeerAccessFunc = int (*)(int peerDevice, unsigned int flags);
using cudaStreamCreateFunc = int (*)(void **pStream);
using cudaStreamCreateWithFlagsFunc = int (*)(void **pStream, unsigned int flags);
using cudaStreamCreateWithPriorityFunc = int (*)(void **pStream, unsigned int flags, int priority);
using cudaStreamDestroyFunc = int (*)(void *stream);
using cudaStreamSynchronizeFunc = int (*)(void *stream);
using cudaMallocFunc = int (*)(void **devPtr, size_t size);
using cudaFreeFunc = int (*)(void *devPtr);
using cudaMallocHostFunc = int (*)(void **pHost, size_t size);
using cudaFreeHostFunc = int (*)(void *pHost);
using cudaMemcpyFunc = int (*)(void *dst, const void *src, size_t count, int kind);
using cudaMemcpyAsyncFunc = int (*)(void *dst, const void *src, size_t count, int kind, void *stream);
using cudaMemcpy2DFunc = int (*)(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height,
                                 int kind);
using cudaMemcpy2DAsyncFunc = int (*)(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width,
                                      size_t height, int kind, void *stream);
using cudaMemsetFunc = int (*)(void *devPtr, int value, size_t count);

class DlCudaApi {
public:
    static Result LoadLibrary(const std::string &libDirPath);
    static void CleanupLibrary();

    static inline Result CudaSetDevice(int deviceId, bool force = false)
    {
        if (pCudaSetDevice == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        if (force) {
            return static_cast<Result>(pCudaSetDevice(deviceId));
        }
        int nowDeviceId = -1;
        if (CudaGetDevice(&nowDeviceId) == BM_OK && nowDeviceId == deviceId) {
            return BM_OK;
        } else {
            return static_cast<Result>(pCudaSetDevice(deviceId));
        }
    }

    static inline Result CudaGetDevice(int *deviceId)
    {
        if (pCudaGetDevice == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaGetDevice(deviceId));
    }

    static inline Result CudaDeviceEnablePeerAccess(int peerDeviceId, unsigned int flags)
    {
        if (pCudaDeviceEnablePeerAccess == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaDeviceEnablePeerAccess(peerDeviceId, flags));
    }

    static inline Result CudaStreamCreate(void **stream)
    {
        if (pCudaStreamCreate == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaStreamCreate(stream));
    }

    static inline Result CudaStreamCreateWithFlags(void **stream, unsigned int flags)
    {
        if (pCudaStreamCreateWithFlags == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaStreamCreateWithFlags(stream, flags));
    }

    static inline Result CudaStreamCreateWithPriority(void **stream, unsigned int flags, int priority)
    {
        if (pCudaStreamCreateWithPriority == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaStreamCreateWithPriority(stream, flags, priority));
    }

    static inline Result CudaStreamDestroy(void *stream)
    {
        if (pCudaStreamDestroy == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaStreamDestroy(stream));
    }

    static inline Result CudaStreamSynchronize(void *stream)
    {
        if (pCudaStreamSynchronize == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaStreamSynchronize(stream));
    }

    static inline Result CudaMalloc(void **ptr, size_t size)
    {
        if (pCudaMalloc == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaMalloc(ptr, size));
    }

    static inline Result CudaFree(void *ptr)
    {
        if (pCudaFree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaFree(ptr));
    }

    static inline Result CudaMallocHost(void **ptr, size_t size)
    {
        if (pCudaMallocHost == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaMallocHost(ptr, size));
    }

    static inline Result CudaFreeHost(void *ptr)
    {
        if (pCudaFreeHost == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaFreeHost(ptr));
    }

    static inline Result CudaMemcpy(void *dst, const void *src, size_t count, int kind)
    {
        if (pCudaMemcpy == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }

        return static_cast<Result>(pCudaMemcpy(dst, src, count, kind));
    }

    static inline Result CudaMemcpyAsync(void *dst, const void *src, size_t count, int kind, void *stream)
    {
        if (pCudaMemcpyAsync == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }

        return static_cast<Result>(pCudaMemcpyAsync(dst, src, count, kind, stream));
    }

    static inline Result CudaMemcpy2D(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width,
                                      size_t height, int kind)
    {
        if (pCudaMemcpy2D == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaMemcpy2D(dst, dpitch, src, spitch, width, height, kind));
    }

    static inline Result CudaMemcpy2DAsync(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width,
                                           size_t height, int kind, void *stream)
    {
        if (pCudaMemcpy2DAsync == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaMemcpy2DAsync(dst, dpitch, src, spitch, width, height, kind, stream));
    }

    static inline Result CudaMemset(void *devPtr, int value, size_t count)
    {
        if (pCudaMemset == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return static_cast<Result>(pCudaMemset(devPtr, value, count));
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *rtHandle;
    static const char *gCudaRuntimeLibName;

    static cudaSetDeviceFunc pCudaSetDevice;
    static cudaGetDeviceFunc pCudaGetDevice;
    static cudaDeviceEnablePeerAccessFunc pCudaDeviceEnablePeerAccess;
    static cudaStreamCreateFunc pCudaStreamCreate;
    static cudaStreamCreateWithFlagsFunc pCudaStreamCreateWithFlags;
    static cudaStreamCreateWithPriorityFunc pCudaStreamCreateWithPriority;
    static cudaStreamDestroyFunc pCudaStreamDestroy;
    static cudaStreamSynchronizeFunc pCudaStreamSynchronize;
    static cudaMallocFunc pCudaMalloc;
    static cudaFreeFunc pCudaFree;
    static cudaMallocHostFunc pCudaMallocHost;
    static cudaFreeHostFunc pCudaFreeHost;
    static cudaMemcpyFunc pCudaMemcpy;
    static cudaMemcpyAsyncFunc pCudaMemcpyAsync;
    static cudaMemcpy2DFunc pCudaMemcpy2D;
    static cudaMemcpy2DAsyncFunc pCudaMemcpy2DAsync;
    static cudaMemsetFunc pCudaMemset;
};

} // namespace mf
} // namespace ock

#endif // MF_HYBM_CORE_DL_CUDA_API_H