/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_data_op_host_rdma.h"
#include "dl_acl_api.h"
#include "hybm_space_allocator.h"
#include "hybm_ptracer.h"

using namespace ock::mf;

namespace {
constexpr uint64_t RDMA_SWAP_SPACE_SIZE = 1024 * 1024 * 128;
}

int32_t HostDataOpRDMA::Initialize() noexcept
{
    if (inited_) {
        return BM_OK;
    }
    rdmaSwapBaseAddr_ = malloc(RDMA_SWAP_SPACE_SIZE);
    if (rdmaSwapBaseAddr_ == nullptr) {
        BM_LOG_ERROR("Failed to malloc rdma swap memory, size: " << RDMA_SWAP_SPACE_SIZE);
        return BM_MALLOC_FAILED;
    }

    transport::TransportMemoryRegion input;
    input.addr = reinterpret_cast<uint64_t>(rdmaSwapBaseAddr_);
    input.size = RDMA_SWAP_SPACE_SIZE;
    input.flags = transport::REG_MR_FLAG_DRAM;
    if (transportManager_ != nullptr) {
        auto ret = transportManager_->RegisterMemoryRegion(input);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to register rdma swap memory, size: " << RDMA_SWAP_SPACE_SIZE);
            free(rdmaSwapBaseAddr_);
            rdmaSwapBaseAddr_ = nullptr;
            return BM_MALLOC_FAILED;
        }
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
    if (transportManager_ != nullptr && rdmaSwapBaseAddr_ != nullptr) {
        transportManager_->UnregisterMemoryRegion(reinterpret_cast<uint64_t>(rdmaSwapBaseAddr_));
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
    int ret;
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

int32_t HostDataOpRDMA::DataCopy2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction,
                                   const ExtOptions &options) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    int ret;
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
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    BM_LOG_ERROR("not supported data copy async!");
    return BM_ERROR;
}

int32_t HostDataOpRDMA::Wait(int32_t waitId) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    return BM_OK;
}

int32_t HostDataOpRDMA::CopyHost2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.destRankId == rankId_) {
        return DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
    }

    if (transportManager_ != nullptr) {
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(length);
        auto tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc host srcVa: " << reinterpret_cast<uintptr_t>(srcVA)
                                                         << " destVa: " << reinterpret_cast<uintptr_t>(destVA)
                                                         << " length: " << length);
            return BM_MALLOC_FAILED;
        }
        auto ret = DlAclApi::AclrtMemcpy(tmpHost, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
            return ret;
        }

        ret = transportManager_->WriteRemote(options.destRankId, (uint64_t)tmpHost, (uint64_t)destVA, length);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
        }
        return ret;
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
}

int32_t HostDataOpRDMA::CopyGva2Host(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.srcRankId == rankId_) {
        return DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
    }

    if (transportManager_ != nullptr) {
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(length);
        auto tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc host srcVa: " << reinterpret_cast<uintptr_t>(srcVA)
                                                         << " destVa: " << reinterpret_cast<uintptr_t>(destVA)
                                                         << " length: " << length);
            return BM_MALLOC_FAILED;
        }

        auto ret = transportManager_->ReadRemote(options.srcRankId, (uint64_t)tmpHost, (uint64_t)srcVA, length);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
            return ret;
        }

        ret = DlAclApi::AclrtMemcpy(destVA, length, tmpHost, length, ACL_MEMCPY_HOST_TO_HOST);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
        }
        return ret;
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
}

int32_t HostDataOpRDMA::CopyDevice2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.destRankId == rankId_) {
        return DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_DEVICE_TO_HOST);
    }

    if (transportManager_ != nullptr) {
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(length);
        auto tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc host srcVa: " << reinterpret_cast<uintptr_t>(srcVA)
                                                         << " destVa: " << reinterpret_cast<uintptr_t>(destVA)
                                                         << " length: " << length);
            return BM_MALLOC_FAILED;
        }
        auto ret = DlAclApi::AclrtMemcpy(tmpHost, length, srcVA, length, ACL_MEMCPY_DEVICE_TO_HOST);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
            return ret;
        }
        ret = transportManager_->WriteRemote(options.destRankId, (uint64_t)tmpHost, (uint64_t)destVA, length);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
        }
        return ret;
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
}

int32_t HostDataOpRDMA::CopyGva2Device(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.srcRankId == rankId_) {
        return DlAclApi::AclrtMemcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_DEVICE);
    }

    if (transportManager_ != nullptr) {
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(length);
        auto tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc host tmp memory srcVa: " << reinterpret_cast<uintptr_t>(srcVA) << " destVa: "
                                                                    << reinterpret_cast<uintptr_t>(destVA)
                                                                    << " length: " << length);
            return BM_MALLOC_FAILED;
        }
        auto ret = transportManager_->ReadRemote(options.srcRankId, (uint64_t)tmpHost, (uint64_t)srcVA, length);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
            return ret;
        }
        ret = DlAclApi::AclrtMemcpy(destVA, length, tmpHost, length, ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to device ret: " << ret);
        }
        return ret;
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
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

    if (transportManager_ != nullptr) {
        uint64_t size = params.width * params.height;
        if (options.destRankId == rankId_) {
            return DlAclApi::AclrtMemcpy2d(params.dest, params.dpitch, params.src, params.spitch, params.width,
                                           params.height, ACL_MEMCPY_DEVICE_TO_HOST);
        }

        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(size);
        auto tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc host srcVa: " << reinterpret_cast<uintptr_t>(params.src) << " destVa: "
                         << reinterpret_cast<uintptr_t>(params.dest) << " length: " << size);
            return BM_MALLOC_FAILED;
        }
        auto ret = DlAclApi::AclrtMemcpy2d(tmpHost, params.dpitch, params.src, params.spitch, params.width,
                                           params.height, ACL_MEMCPY_DEVICE_TO_HOST);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
            return ret;
        }
        ret = transportManager_->WriteRemote(options.destRankId, (uint64_t)tmpHost, (uint64_t)params.dest, size);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
        }
        return ret;
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
}

int32_t HostDataOpRDMA::CopyGva2Device2d(hybm_copy_2d_params &params, const ExtOptions &options)
{
    if (params.spitch != params.width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }

    uint64_t size = params.width * params.height;
    if (options.srcRankId == rankId_) {
        return DlAclApi::AclrtMemcpy2d(params.dest, params.dpitch, params.src, params.spitch, params.width,
                                       params.height, ACL_MEMCPY_HOST_TO_DEVICE);
    }

    if (transportManager_ != nullptr) {
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(size);
        auto tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc host srcVa: " << reinterpret_cast<uintptr_t>(params.src) << " destVa: "
                         << reinterpret_cast<uintptr_t>(params.dest) << " length: " << size);
            return BM_MALLOC_FAILED;
        }
        auto ret = transportManager_->ReadRemote(options.srcRankId, (uint64_t)tmpHost, (uint64_t)params.src, size);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
            return ret;
        }
        ret = DlAclApi::AclrtMemcpy2d(params.dest, params.dpitch, tmpHost, params.spitch, params.width,
                                      params.height, ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
        }
        return ret;
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
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

int32_t HostDataOpRDMA::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                      const ExtOptions &options) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    auto ret = 0;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_LD_TO_GH);
            ret = BatchCopyLD2GH(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options);
            TP_TRACE_END(TP_HYBM_HOST_RDMA_LD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_BATCH_GH_TO_LD);
            ret = BatchCopyGH2LD(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options);
            TP_TRACE_END(TP_HYBM_HOST_RDMA_BATCH_GH_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_LH_TO_GH);
            ret = BatchCopyLH2GH(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options);
            TP_TRACE_END(TP_HYBM_HOST_RDMA_LH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_BATCH_GH_TO_LH);
            ret = BatchCopyGH2LH(params.destinations, params.sources, params.dataSizes,
                                 params.batchSize, options);
            TP_TRACE_END(TP_HYBM_HOST_RDMA_BATCH_GH_TO_LH, ret);
            break;
        }
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int HostDataOpRDMA::BatchCopyLD2LH(void **hostAddrs, void **deviceAddrs, const uint64_t *counts,
                                   uint32_t batchSize, const ExtOptions &options) noexcept
{
    void *st = options.stream;
    auto ret = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        auto destAddr = hostAddrs[i];
        auto srcAddr = deviceAddrs[i];
        auto count = counts[i];
        ret = DlAclApi::AclrtMemcpyAsync(destAddr, count, srcAddr, count, ACL_MEMCPY_DEVICE_TO_HOST, st);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local device to local host failed: " << ret << " stream:"
                                                                              << reinterpret_cast<uintptr_t>(st));
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << reinterpret_cast<uintptr_t>(st));
    }
    return ret;
}

int HostDataOpRDMA::BatchCopyLH2LD(void **deviceAddrs, void **hostAddrs, const uint64_t *counts,
                                   uint32_t batchSize, const ExtOptions &options) noexcept
{
    void *st = options.stream;
    auto ret = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        auto destAddr = deviceAddrs[i];
        auto srcAddr = hostAddrs[i];
        auto count = counts[i];
        ret = DlAclApi::AclrtMemcpyAsync(destAddr, count, srcAddr, count, ACL_MEMCPY_HOST_TO_DEVICE, st);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local host to local device failed: " << ret << " stream:"
                                                                              << reinterpret_cast<uintptr_t>(st));
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << reinterpret_cast<uintptr_t>(st));
    }
    return ret;
}

int HostDataOpRDMA::BatchCopyLD2GH(void **gvaAddrs, void **deviceAddrs, const uint64_t *counts,
                                   uint32_t batchSize, const ExtOptions &options) noexcept
{
    if (options.destRankId == rankId_) {
        return BatchCopyLD2LH(gvaAddrs, deviceAddrs, counts, batchSize, options);
    }
    auto ret = 0;
    uint64_t totalSize = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        totalSize += counts[i];
        BM_ASSERT_RETURN(totalSize <= RDMA_SWAP_SPACE_SIZE, BM_INVALID_PARAM);
    }
    auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(totalSize);
    void *tmpHost = tmpRdmaMemory.Address();
    if (tmpHost == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap length: " << totalSize);
        return BM_MALLOC_FAILED;
    }
    void *tmpRdmaAddrs[batchSize];
    uint64_t offset = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        tmpRdmaAddrs[i] = reinterpret_cast<void *>(static_cast<uint8_t *>(tmpHost) + offset);
        offset += counts[i];
    }
    ret = BatchCopyLD2LH(tmpRdmaAddrs, deviceAddrs, counts, batchSize, options);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to copy local device to swap memory: " << ret);
        return ret;
    }
    ret = transportManager_->WriteRemote(options.destRankId, (uint64_t)tmpHost, (uint64_t)gvaAddrs[0], totalSize);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy swap memory to remote host memory ret: " << ret << " localRankId:" << rankId_
            << " remoteRankId:" << options.destRankId);
    }
    return ret;
}

int HostDataOpRDMA::BatchCopyGH2LD(void **deviceAddrs, void **gvaAddrs, const uint64_t *counts,
                                   uint32_t batchSize, const ExtOptions &options) noexcept
{
    if (options.srcRankId == rankId_) {
        return BatchCopyLH2LD(deviceAddrs, gvaAddrs, counts, batchSize, options);
    }
    auto ret = 0;
    uint64_t totalSize = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        totalSize += counts[i];
        BM_ASSERT_RETURN(totalSize <= RDMA_SWAP_SPACE_SIZE, BM_INVALID_PARAM);
    }
    auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(totalSize);
    void *tmpHost = tmpRdmaMemory.Address();
    if (tmpHost == nullptr) {
        BM_LOG_ERROR("Failed to malloc swap length: " << totalSize);
        return BM_MALLOC_FAILED;
    }
    ret = transportManager_->ReadRemote(options.srcRankId, (uint64_t)tmpHost, (uint64_t)gvaAddrs[0], totalSize);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to read remote host to local host ret: " << ret);
        return ret;
    }
    void *tmpRdmaAddrs[batchSize];
    uint64_t offset = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        tmpRdmaAddrs[i] = reinterpret_cast<void *>(static_cast<uint8_t *>(tmpHost) + offset);
        offset += counts[i];
    }
    ret = BatchCopyLH2LD(deviceAddrs, tmpRdmaAddrs, counts, batchSize, options);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to read swap memory to local device: " << ret << " localRankId:" << rankId_
                                                                    << " remoteRankId:" << options.destRankId);
        return ret;
    }
    return 0;
}

int HostDataOpRDMA::BatchCopyLH2GH(void **gvaAddrs, void **hostAddrs, const uint64_t *counts,
                                   uint32_t batchSize, const ExtOptions &options) noexcept
{
    auto ret = 0;
    for (auto i = 0U; i < batchSize; i++) {
        ret = CopyHost2Gva(hostAddrs[i], gvaAddrs[i], counts[i], options);
        if (ret != 0) {
            BM_LOG_ERROR("copy local host to GVA failed: " << ret);
            return ret;
        }
    }
    return BM_OK;
}

int HostDataOpRDMA::BatchCopyGH2LH(void **hostAddrs, void **gvaAddrs, const uint64_t *counts,
                                   uint32_t batchSize, const ExtOptions &options) noexcept
{
    auto ret = 0;
    for (auto i = 0U; i < batchSize; i++) {
        ret = CopyGva2Host(gvaAddrs[i], hostAddrs[i], counts[i], options);
        if (ret != 0) {
            BM_LOG_ERROR("copy GVA to local host failed: " << ret);
            return ret;
        }
    }
    return BM_OK;
}