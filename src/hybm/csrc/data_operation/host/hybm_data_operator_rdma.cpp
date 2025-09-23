/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hybm_data_operator_rdma.h"
#include "dl_acl_api.h"
#include "hybm_space_allocator.h"
#include "hybm_rbtree_range_pool.h"

using namespace ock::mf;

namespace {
constexpr uint64_t RDMA_SWAP_SPACE_SIZE = 1024 * 1024 * 128;
}

thread_local void *HostDataOpRDMA::stream_ = nullptr;

int32_t HostDataOpRDMA::Initialize() noexcept
{
    if (inited_) {
        return BM_OK;
    }

    auto ret = DlAclApi::AclrtCreateStream(&stream_);
    if (ret != 0) {
        BM_LOG_ERROR("create stream failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    rdmaSwapBaseAddr_ = malloc(RDMA_SWAP_SPACE_SIZE);
    if (rdmaSwapBaseAddr_ == nullptr) {
        BM_LOG_ERROR("Failed to malloc rdma swap memory, size: " << RDMA_SWAP_SPACE_SIZE);
        return BM_MALLOC_FAILED;
    }

    transport::TransportMemoryRegion input;
    input.addr = reinterpret_cast<uint64_t>(rdmaSwapBaseAddr_);
    input.size = RDMA_SWAP_SPACE_SIZE;
    ret = transportManager_->RegisterMemoryRegion(input);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to register rdma swap memory, size: " << RDMA_SWAP_SPACE_SIZE);
        free(rdmaSwapBaseAddr_);
        rdmaSwapBaseAddr_ = nullptr;
        return BM_MALLOC_FAILED;
    }
    rdmaSwapMemoryAllocator_ = std::make_shared<RbtreeRangePool>((uint8_t *) rdmaSwapBaseAddr_, RDMA_SWAP_SPACE_SIZE);
    inited_ = true;
    return BM_OK;
}

void HostDataOpRDMA::UnInitialize() noexcept
{
    if (!inited_) {
        return;
    }

    if (stream_ != nullptr) {
        int32_t ret = DlAclApi::AclrtDestroyStream(stream_);
        if (ret != 0) {
            BM_LOG_ERROR("destroy stream failed: " << ret);
        }
        stream_ = nullptr;
    }
    if (rdmaSwapBaseAddr_ != nullptr) {
        free(rdmaSwapBaseAddr_);
        rdmaSwapBaseAddr_ = nullptr;
    }
    inited_ = false;
}

HostDataOpRDMA::~HostDataOpRDMA()
{
    UnInitialize();
}

int32_t HostDataOpRDMA::DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                                 const ExtOptions &options) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    auto ret = PrepareThreadLocalStream();
    if (ret != BM_OK) {
        return ret;
    }
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST:
            ret = CopyHost2Gva(params.src, params.dest, params.dataSize, options);
            break;
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST:
            ret = CopyDevice2Gva(params.src, params.dest, params.dataSize, options);
            break;
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST:
            ret = CopyGva2Gva(params.src, params.dest, params.dataSize, options);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST:
            ret = CopyGva2Host(params.src, params.dest, params.dataSize, options);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE:
            ret = CopyGva2Device(params.src, params.dest, params.dataSize, options);
            break;
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int HostDataOpRDMA::PrepareThreadLocalStream() noexcept
{
    if (stream_ != nullptr) {
        return BM_OK;
    }

    auto ret = DlAclApi::AclrtCreateStream(&stream_);
    if (ret != 0) {
        BM_LOG_ERROR("create thread local stream failed: " << ret);
        return ret;
    }
    return BM_OK;
}

int32_t HostDataOpRDMA::DataCopy2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
                                   const ExtOptions &options) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    auto ret = PrepareThreadLocalStream();
    if (ret != BM_OK) {
        return ret;
    }
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST:
            ret = CopyHost2Gva2d(params, options);
            break;
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST:
            ret = CopyDevice2Gva2d(params, options);
            break;
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST:
            ret = CopyGva2Gva2d(params, options);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST:
            ret = CopyGva2Host2d(params, options);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE:
            ret = CopyGva2Device2d(params, options);
            break;
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int32_t HostDataOpRDMA::DataCopyAsync(hybm_copy_params &params,
                                      hybm_data_copy_direction direction, const ExtOptions &options) noexcept
{
    BM_LOG_ERROR("not supported data copy async!");
    return BM_ERROR;
}

int32_t HostDataOpRDMA::Wait(int32_t waitId) noexcept
{
    BM_LOG_ERROR("not supported wait!");
    return BM_ERROR;
}

int32_t HostDataOpRDMA::CopyHost2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.destRankId == rankId_) {
        return DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
    }

    auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(length);
    auto tmpHost = tmpRdmaMemory.Address();
    if (tmpHost == nullptr) {
        BM_LOG_ERROR("Failed to malloc host, length: " << length);
        return BM_MALLOC_FAILED;
    }
    auto ret = DlAclApi::AclrtMemcpy(tmpHost, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
        rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
        return ret;
    }

    ret = transportManager_->WriteRemote(options.destRankId, (uint64_t) tmpHost, (uint64_t) destVA, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
    }
    rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
    return ret;
}

int32_t HostDataOpRDMA::CopyGva2Host(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.srcRankId == rankId_) {
        return DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
    }
    auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(length);
    auto tmpHost = tmpRdmaMemory.Address();
    if (tmpHost == nullptr) {
        BM_LOG_ERROR("Failed to malloc host, length: " << length);
        return BM_MALLOC_FAILED;
    }

    auto ret = transportManager_->ReadRemote(options.srcRankId, (uint64_t) tmpHost, (uint64_t) srcVA, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
        rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
        return ret;
    }

    ret = DlAclApi::AclrtMemcpy(destVA, length, tmpHost, length, ACL_MEMCPY_HOST_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
    }
    rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
    return ret;
}

int32_t HostDataOpRDMA::CopyDevice2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.destRankId == rankId_) {
        return DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_DEVICE_TO_HOST);
    }

    auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(length);
    auto tmpHost = tmpRdmaMemory.Address();
    if (tmpHost == nullptr) {
        BM_LOG_ERROR("Failed to malloc host srcVa, length: " << length);
        return BM_MALLOC_FAILED;
    }
    auto ret = DlAclApi::AclrtMemcpy(tmpHost, length, srcVA, length, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
        rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
        return ret;
    }
    ret = transportManager_->WriteRemote(options.destRankId, (uint64_t) tmpHost, (uint64_t) destVA, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
    }
    rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
    return ret;
}

int32_t HostDataOpRDMA::CopyGva2Device(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.srcRankId == rankId_) {
        return DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_DEVICE);
    }

    auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(length);
    auto tmpHost = tmpRdmaMemory.Address();
    if (tmpHost == nullptr) {
        BM_LOG_ERROR("Failed to malloc host tmp memory, length: " << length);
        return BM_MALLOC_FAILED;
    }
    auto ret = transportManager_->ReadRemote(options.srcRankId, (uint64_t) tmpHost, (uint64_t) srcVA, length);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
        rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
        return ret;
    }
    ret = DlAclApi::AclrtMemcpy(destVA, length, tmpHost, length, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy host data to device ret: " << ret);
    }
    rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
    return ret;
}

int32_t HostDataOpRDMA::CopyGva2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.srcRankId == rankId_) {
        return CopyHost2Gva(srcVA, destVA, length, options);
    }

    if (options.destRankId == rankId_) {
        return CopyGva2Host(srcVA, destVA, length, options);
    }

    BM_LOG_ERROR("Not support remote gva to remote gva");
    return BM_INVALID_PARAM;
}

int32_t HostDataOpRDMA::CopyHost2Gva2d(hybm_copy_2d_params &params, const ExtOptions &options)
{
    if (params.spitch != params.width || params.dpitch != params.width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }
    uint64_t size = params.width * params.height;
    return CopyHost2Gva(params.src, params.dest, size, options);
}

int32_t HostDataOpRDMA::CopyGva2Host2d(hybm_copy_2d_params &params, const ExtOptions &options)
{
    if (params.spitch != params.width || params.dpitch != params.width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }
    uint64_t size = params.width * params.height;
    return CopyGva2Host(params.src, params.dest, size, options);
}

int32_t HostDataOpRDMA::CopyDevice2Gva2d(hybm_copy_2d_params &params, const ExtOptions &options)
{
    if (params.dpitch != params.width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }

    uint64_t size = params.width * params.height;
    if (options.destRankId == rankId_) {
        return DlAclApi::AclrtMemcpy2d(params.dest, params.dpitch, params.src, params.spitch,
                                       params.width, params.height, ACL_MEMCPY_DEVICE_TO_HOST);
    }

    auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(size);
    auto tmpHost = tmpRdmaMemory.Address();
    if (tmpHost == nullptr) {
        BM_LOG_ERROR("Failed to malloc host, length: " << size);
        return BM_MALLOC_FAILED;
    }
    auto ret = DlAclApi::AclrtMemcpy2d(tmpHost, params.dpitch, params.src, params.spitch,
                                       params.width, params.height, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
        rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
        return ret;
    }
    ret = transportManager_->WriteRemote(options.destRankId, (uint64_t) tmpHost, (uint64_t) params.dest, size);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
    }
    rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
    return ret;
}

int32_t HostDataOpRDMA::CopyGva2Device2d(hybm_copy_2d_params &params, const ExtOptions &options)
{
    if (params.spitch != params.width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }

    uint64_t size = params.width * params.height;
    if (options.srcRankId == rankId_) {
        return DlAclApi::AclrtMemcpy2d(params.dest, params.dpitch, params.src, params.spitch,
                                       params.width, params.height, ACL_MEMCPY_HOST_TO_DEVICE);
    }

    auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(size);
    auto tmpHost = tmpRdmaMemory.Address();
    if (tmpHost == nullptr) {
        BM_LOG_ERROR("Failed to malloc host, length: " << size);
        return BM_MALLOC_FAILED;
    }
    auto ret = transportManager_->ReadRemote(options.srcRankId, (uint64_t) tmpHost, (uint64_t) params.src, size);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
        rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
        return ret;
    }
    ret = DlAclApi::AclrtMemcpy2d(params.dest, params.dpitch, tmpHost, params.spitch,
                                  params.width, params.height, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
    }
    rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
    return ret;
}

int32_t HostDataOpRDMA::CopyGva2Gva2d(hybm_copy_2d_params &params, const ExtOptions &options)
{
    if (params.spitch != params.width || params.dpitch != params.width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }
    uint64_t size = params.width * params.height;
    return CopyGva2Gva(params.src, params.dest, size, options);
}

int32_t HostDataOpRDMA::RtMemoryCopyAsync(const void *srcVA, void *destVA, uint64_t length,
                                          uint32_t kind, const ExtOptions &options)
{
    void *st = stream_;
    if (options.stream != nullptr) {
        st = options.stream;
    }

    auto ret = DlAclApi::AclrtMemcpyAsync(destVA, length, srcVA, length, kind, st);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to add aclrt memory copy async task, length: " << length << " ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

int32_t HostDataOpRDMA::RtMemoryCopy2dAsync(hybm_copy_2d_params &params, uint32_t kind, const ExtOptions &options)
{
    void *st = stream_;
    if (options.stream != nullptr) {
        st = options.stream;
    }

    auto ret = DlAclApi::AclrtMemcpy2dAsync(params.dest, params.dpitch, params.src,
                                            params.spitch, params.width, params.height, kind, st);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to add aclrt memory copy2d async task, width: " << params.width
            << " height: " << params.height
            << " kind: " << kind << " ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}
