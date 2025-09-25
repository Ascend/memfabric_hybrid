/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_data_operator_rdma.h"
#include "dl_acl_api.h"
#include "space_allocator.h"
#include "rbtree_range_pool.h"
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

int32_t HostDataOpRDMA::DataCopy(const void *srcVA, void *destVA, uint64_t length, hybm_data_copy_direction direction,
                                 const ExtOptions &options) noexcept
{
    int ret;
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: {
            ret = CopyHost2Gva(srcVA, destVA, length, options);
            break;
        }
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST:
            ret = CopyDevice2Gva(srcVA, destVA, length, options);
            break;
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST:
            ret = CopyGva2Gva(srcVA, destVA, length, options);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST:
            ret = CopyGva2Host(srcVA, destVA, length, options);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE:
            ret = CopyGva2Device(srcVA, destVA, length, options);
            break;
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int32_t HostDataOpRDMA::DataCopy2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                                   uint64_t width, uint64_t height, hybm_data_copy_direction direction,
                                   const ExtOptions &options) noexcept
{
    if (width > std::numeric_limits<uint64_t>::max() / height) {
        BM_LOG_ERROR("multiply width(" << width << ") and height(" << height << ") will overflow");
        return BM_INVALID_PARAM;
    }
    int ret;
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST:
            ret = CopyHost2Gva2d(srcVA, spitch, destVA, dpitch, width, height, options);
            break;
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST:
            ret = CopyDevice2Gva2d(srcVA, spitch, destVA, dpitch, width, height, options);
            break;
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST:
            ret = CopyGva2Gva2d(srcVA, spitch, destVA, dpitch, width, height, options);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST:
            ret = CopyGva2Host2d(srcVA, spitch, destVA, dpitch, width, height, options);
            break;
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE:
            ret = CopyGva2Device2d(srcVA, spitch, destVA, dpitch, width, height, options);
            break;
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    return ret;
}

int32_t HostDataOpRDMA::DataCopyAsync(const void *srcVA, void *destVA, uint64_t length,
                                      hybm_data_copy_direction direction, const ExtOptions &options) noexcept
{
    BM_LOG_ERROR("not supported data copy async!");
    return BM_ERROR;
}

int32_t HostDataOpRDMA::Wait(int32_t waitId) noexcept
{
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
            BM_LOG_ERROR("Failed to malloc host srcVa: " << srcVA << " destVa: " << destVA << " length: " << length);
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
            BM_LOG_ERROR("Failed to malloc host srcVa: " << srcVA << " destVa: " << destVA << " length: " << length);
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
            BM_LOG_ERROR("Failed to malloc host srcVa: " << srcVA << " destVa: " << destVA << " length: " << length);
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
            BM_LOG_ERROR("Failed to malloc host tmp memory srcVa: " << srcVA << " destVa: " << destVA
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

int32_t HostDataOpRDMA::CopyHost2Gva2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                                       uint64_t width, uint64_t height, const ExtOptions &options)
{
    if (spitch != width || dpitch != width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }
    uint64_t size = width * height;
    return CopyHost2Gva(srcVA, destVA, size, options);
}

int32_t HostDataOpRDMA::CopyGva2Host2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                                       uint64_t width, uint64_t height, const ExtOptions &options)
{
    if (spitch != width || dpitch != width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }
    uint64_t size = width * height;
    return CopyGva2Host(srcVA, destVA, size, options);
}

int32_t HostDataOpRDMA::CopyDevice2Gva2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                                         uint64_t width, uint64_t height, const ExtOptions &options)
{
    if (dpitch != width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }

    if (transportManager_ != nullptr) {
        uint64_t size = width * height;
        if (options.destRankId == rankId_) {
            return DlAclApi::AclrtMemcpy2d(destVA, dpitch, srcVA, spitch, width, height, ACL_MEMCPY_DEVICE_TO_HOST);
        }

        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(size);
        auto tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc host srcVa: " << srcVA << " destVa: " << destVA << " length: " << size);
            return BM_MALLOC_FAILED;
        }
        auto ret = DlAclApi::AclrtMemcpy2d(tmpHost, dpitch, srcVA, spitch, width, height, ACL_MEMCPY_DEVICE_TO_HOST);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
            return ret;
        }
        ret = transportManager_->WriteRemote(options.destRankId, (uint64_t)tmpHost, (uint64_t)destVA, size);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
        }
        return ret;
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
}

int32_t HostDataOpRDMA::CopyGva2Device2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                                         uint64_t width, uint64_t height, const ExtOptions &options)
{
    if (spitch != width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }

    uint64_t size = width * height;
    if (options.srcRankId == rankId_) {
        return DlAclApi::AclrtMemcpy2d(destVA, dpitch, srcVA, spitch, width, height, ACL_MEMCPY_HOST_TO_DEVICE);
    }

    if (transportManager_ != nullptr) {
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(size);
        auto tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc host srcVa: " << srcVA << " destVa: " << destVA << " length: " << size);
            return BM_MALLOC_FAILED;
        }
        auto ret = transportManager_->ReadRemote(options.srcRankId, (uint64_t)tmpHost, (uint64_t)srcVA, size);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
            return ret;
        }
        ret = DlAclApi::AclrtMemcpy2d(destVA, dpitch, tmpHost, spitch, width, height, ACL_MEMCPY_HOST_TO_DEVICE);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
        }
        return ret;
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
}

int32_t HostDataOpRDMA::CopyGva2Gva2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                                      uint64_t width, uint64_t height, const ExtOptions &options)
{
    if (spitch != width || dpitch != width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }
    uint64_t size = width * height;
    return CopyGva2Gva(srcVA, destVA, size, options);
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
        BM_LOG_ERROR("Failed to add aclrt memory copy async task srcVa: " << srcVA << " destVa: "
                                                                          << destVA << " length: " << length
                                                                          << " ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

int32_t HostDataOpRDMA::RtMemoryCopy2dAsync(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                                            uint64_t width, uint64_t height, uint32_t kind, const ExtOptions &options)
{
    void *st = stream_;
    if (options.stream != nullptr) {
        st = options.stream;
    }

    auto ret = DlAclApi::AclrtMemcpy2dAsync(destVA, dpitch, srcVA, spitch, width, height, kind, st);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to add aclrt memory copy2d async task srcVa: " << srcVA << " spitch: " << spitch
                                                                            << " destVA: " << destVA << " width: "
                                                                            << width << " height: " << height
                                                                            << " kind: " << kind << " ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    ret = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

int32_t HostDataOpRDMA::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                      const ExtOptions &options) noexcept
{
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

int HostDataOpRDMA::BatchCopyLD2LH(void **hostAddrs, const void **deviceAddrs, const uint32_t *counts,
                                   uint32_t batchSize, const ExtOptions &options) noexcept
{
    void *st = stream_;
    if (options.stream != nullptr) {
        st = options.stream;
    }
    auto ret = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        auto destAddr = hostAddrs[i];
        auto srcAddr = deviceAddrs[i];
        auto count = counts[i];
        ret = DlAclApi::AclrtMemcpyAsync(destAddr, count, srcAddr, count, ACL_MEMCPY_DEVICE_TO_HOST, st);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local device to local host failed: " << ret << " stream:" << st);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret  = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
    }
    return ret;
}

int HostDataOpRDMA::BatchCopyLH2LD(void **deviceAddrs, const void **hostAddrs, const uint32_t *counts,
                                   uint32_t batchSize, const ExtOptions &options) noexcept
{
    void *st = stream_;
    if (options.stream != nullptr) {
        st = options.stream;
    }
    auto ret = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        auto destAddr = deviceAddrs[i];
        auto srcAddr = hostAddrs[i];
        auto count = counts[i];
        ret = DlAclApi::AclrtMemcpyAsync(destAddr, count, srcAddr, count, ACL_MEMCPY_HOST_TO_DEVICE, st);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local host to local device failed: " << ret << " stream:" << st);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret  = DlAclApi::AclrtSynchronizeStream(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << st);
    }
    return ret;
}

int HostDataOpRDMA::BatchCopyLD2GH(void **gvaAddrs, const void **deviceAddrs, const uint32_t *counts,
                                   uint32_t batchSize, const ExtOptions &options) noexcept
{
    if (options.destRankId == rankId_) {
        return BatchCopyLD2LH(gvaAddrs, deviceAddrs, counts, batchSize, options);
    }
    auto ret = 0;
    uint64_t totalSize = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        totalSize += counts[i];
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

int HostDataOpRDMA::BatchCopyGH2LD(void **deviceAddrs, const void **gvaAddrs, const uint32_t *counts,
                                   uint32_t batchSize, const ExtOptions &options) noexcept
{
    if (options.srcRankId == rankId_) {
        return BatchCopyLH2LD(deviceAddrs, gvaAddrs, counts, batchSize, options);
    }
    auto ret = 0;
    uint64_t totalSize = 0;
    for (size_t i = 0; i < batchSize; ++i) {
        totalSize += counts[i];
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
    const void *tmpRdmaAddrs[batchSize];
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

int HostDataOpRDMA::BatchCopyLH2GH(void **gvaAddrs, const void **hostAddrs, const uint32_t *counts,
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

int HostDataOpRDMA::BatchCopyGH2LH(void **hostAddrs, const void **gvaAddrs, const uint32_t *counts,
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