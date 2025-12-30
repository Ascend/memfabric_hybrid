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

#ifndef DL_HYBRID_API_H
#define DL_HYBRID_API_H

#include <cstddef>
#include <string>
#include <mutex>
#include "dl_acl_api.h"

#if defined(ASCEND_NPU)
#include "dl_acl_api.h"
#elif defined(NVIDIA_GPU)
#include "dl_cuda_api.h"
#endif

namespace ock {
namespace mf {
class DlHybridApi {
public:
    static Result LoadLibrary(const std::string &libDirPath = "")
    {
#if defined(ASCEND_NPU)
        return DlAclApi::LoadLibrary(libDirPath);
#elif defined(NVIDIA_GPU)
        return DlCudaApi::LoadLibrary(libDirPath);
#endif
        return BM_OK;
    }

    static void CleanupLibrary()
    {
#if defined(ASCEND_NPU)
        return DlAclApi::CleanupLibrary();
#elif defined(NVIDIA_GPU)
        return DlCudaApi::CleanupLibrary();
#endif
    }

    static inline Result SetDevice(int device)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtSetDevice(device);
#elif defined(NVIDIA_GPU)
        return DlCudaApi::CudaSetDevice(device);
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result GetDevice(int *device)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtGetDevice(device);
#elif defined(NVIDIA_GPU)
        return DlCudaApi::CudaGetDevice(device);
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result StreamCreate(void **stream)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtCreateStream(stream);
#elif defined(NVIDIA_GPU)
        return DlCudaApi::CudaStreamCreate(stream);
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result StreamDestroy(void *stream)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtDestroyStream(stream);
#elif defined(NVIDIA_GPU)
        return DlCudaApi::CudaStreamDestroy(stream);
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result StreamSynchronize(void *stream)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtSynchronizeStream(stream);
#elif defined(NVIDIA_GPU)
        return DlCudaApi::CudaStreamSynchronize(stream);
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result Malloc(void **ptr, size_t size, uint32_t type)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtMalloc(ptr, size, type);
#elif defined(NVIDIA_GPU)
        (void)type;
        return DlCudaApi::CudaMalloc(ptr, size);
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result MallocHost(void **ptr, size_t size)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtMallocHost(ptr, size);
#elif defined(NVIDIA_GPU)
        return DlCudaApi::CudaMallocHost(ptr, size);
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result Free(void *ptr)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtFree(ptr);
#elif defined(NVIDIA_GPU)
        return DlCudaApi::CudaFree(ptr);
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result FreeHost(void *ptr)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtFreeHost(ptr);
#elif defined(NVIDIA_GPU)
        return DlCudaApi::CudaFreeHost(ptr);
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result Memcpy(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtMemcpy(dst, destMax, src, count, kind);
#elif defined(NVIDIA_GPU)
        (void)destMax;
        return DlCudaApi::CudaMemcpy(dst, src, count, static_cast<int>(kind));
#else
        if (kind != ACL_MEMCPY_HOST_TO_HOST) {
            return BM_INVALID_PARAM;
        }
        if (count > destMax) {
            return BM_INVALID_PARAM;
        }
        // 将 void* 转为 char*
        char *dst_char = static_cast<char *>(dst);
        const char *src_char = static_cast<const char *>(src);
        std::copy_n(src_char, count, dst_char);
        return BM_OK;
#endif
    }

    static inline Result MemcpyBatch(void **dsts, size_t *destMax, void **srcs, size_t *sizes, size_t numBatches,
                                     aclrtMemcpyBatchAttr *attrs, size_t *attrsIndexes, size_t numAttrs,
                                     size_t *failIndex)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtMemcpyBatch(dsts, destMax, srcs, sizes, numBatches, attrs, attrsIndexes, numAttrs,
                                          failIndex);
#elif defined(NVIDIA_GPU)
        return BM_NOT_SUPPORTED;
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result MemcpyAsync(void *dst, size_t destMax, const void *src, size_t count, uint32_t kind,
                                     void *stream)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtMemcpyAsync(dst, destMax, src, count, kind, stream);
#elif defined(NVIDIA_GPU)
        (void)destMax;
        return DlCudaApi::CudaMemcpyAsync(dst, src, count, static_cast<int>(kind), reinterpret_cast<void *>(stream));
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result DeviceEnablePeerAccess(int32_t peerDeviceId, uint32_t flags)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtDeviceEnablePeerAccess(peerDeviceId, flags);
#elif defined(NVIDIA_GPU)
        return DlCudaApi::CudaDeviceEnablePeerAccess(peerDeviceId, flags);
#else
        return BM_NOT_SUPPORTED;
#endif
    }

    static inline Result SynchronizeStream(void *stream)
    {
#if defined(ASCEND_NPU)
        return DlAclApi::AclrtSynchronizeStream(stream);
#elif defined(NVIDIA_GPU)
        return DlCudaApi::CudaStreamSynchronize(reinterpret_cast<void *>(stream));
#else
        return BM_NOT_SUPPORTED;
#endif
    }
};
} // namespace mf
} // namespace ock
#endif // DL_HYBRID_API_H
