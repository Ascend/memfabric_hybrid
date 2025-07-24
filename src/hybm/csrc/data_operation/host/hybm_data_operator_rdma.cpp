/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_data_operator_rdma.h"
#include "dl_acl_api.h"
#include "space_allocator.h"
#include "rbtree_range_pool.h"

using namespace ock::mf;

namespace {
constexpr uint64_t RDMA_SWAP_SPACE_SIZE = 1024 * 1024 * 128;
}

int32_t HostDataOpRDMA::Initialized() noexcept
{
    rdmaSwapBaseAddr_ = malloc(RDMA_SWAP_SPACE_SIZE);
    if (rdmaSwapBaseAddr_ == nullptr) {
        BM_LOG_ERROR("Failed to malloc rdma swap memory, size: " << RDMA_SWAP_SPACE_SIZE);
        return BM_MALLOC_FAILED;
    }

    HybmTransMemReg input;
    input.addr = reinterpret_cast<uint64_t>(rdmaSwapBaseAddr_);
    input.size = RDMA_SWAP_SPACE_SIZE;
    MrInfo info{};
    auto ret = transportManager_->RegisterMemoryRegion(input, info);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to register rdma swap memory, addr: " << rdmaSwapBaseAddr_
                     << " size: " << RDMA_SWAP_SPACE_SIZE);
        free(rdmaSwapBaseAddr_);
        rdmaSwapBaseAddr_ = nullptr;
        return BM_MALLOC_FAILED;
    }
    rdmaSwapMemoryAllocator_ = std::make_shared<RbtreeRangePool>((uint8_t *) rdmaSwapBaseAddr_, RDMA_SWAP_SPACE_SIZE);
    return BM_OK;
}

void HostDataOpRDMA::UnInitialized() noexcept
{
    if (rdmaSwapBaseAddr_ != nullptr) {
        free(rdmaSwapBaseAddr_);
        rdmaSwapBaseAddr_ = nullptr;
    }
}

HostDataOpRDMA::~HostDataOpRDMA()
{
    UnInitialized();
}

int32_t HostDataOpRDMA::DataCopy(const void *srcVA, void *destVA, uint64_t length, hybm_data_copy_direction direction,
                                 const ExtOptions &options) noexcept
{
    int ret;
    switch (direction) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST:
            ret = CopyHost2Gva(srcVA, destVA, length, options);
            break;
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

int32_t
HostDataOpRDMA::DataCopyAsync(const void *srcVA, void *destVA, uint64_t length, hybm_data_copy_direction direction,
                              const ExtOptions &options) noexcept
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
        BM_LOG_ERROR("Failed to malloc host srcVa: " << srcVA << " destVa: "
                                                     << destVA << " length: " << length);
        return BM_MALLOC_FAILED;
    }
    auto ret = DlAclApi::AclrtMemcpy(tmpHost, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
        rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
        return ret;
    }

    ret = transportManager_->RdmaOneSideTrans(options.destRankId, (uint64_t) tmpHost, (uint64_t) destVA, length, false);
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
        BM_LOG_ERROR("Failed to malloc host srcVa: " << srcVA << " destVa: "
                                                     << destVA << " length: " << length);
        return BM_MALLOC_FAILED;
    }

    auto ret = transportManager_->RdmaOneSideTrans(options.srcRankId ,(uint64_t) tmpHost, (uint64_t) srcVA,
                                                   length, true);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
        rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
        return ret;
    }

    ret = DlAclApi::AclrtMemcpy(tmpHost, length, destVA, length, ACL_MEMCPY_HOST_TO_HOST);
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
        BM_LOG_ERROR("Failed to malloc host srcVa: " << srcVA << " destVa: "
                                                     << destVA << " length: " << length);
        return BM_MALLOC_FAILED;
    }
    auto ret = DlAclApi::AclrtMemcpy(tmpHost, length, srcVA, length, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
        rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
        return ret;
    }
    ret = transportManager_->RdmaOneSideTrans(options.destRankId, (uint64_t) tmpHost, (uint64_t) destVA, length, false);
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
        BM_LOG_ERROR("Failed to malloc host tmp memory srcVa: " << srcVA << " destVa: "
                                                                << destVA << " length: " << length);
        return BM_MALLOC_FAILED;
    }
    auto ret = transportManager_->RdmaOneSideTrans(options.srcRankId, (uint64_t) tmpHost, (uint64_t) srcVA,
                                                   length, true);
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

    uint64_t size = width * height;
    if (options.destRankId == rankId_) {
        return DlAclApi::AclrtMemcpy2d(destVA, dpitch, srcVA, spitch, width, height, ACL_MEMCPY_DEVICE_TO_HOST);
    }

    auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(size);
    auto tmpHost = tmpRdmaMemory.Address();
    if (tmpHost == nullptr) {
        BM_LOG_ERROR("Failed to malloc host srcVa: " << srcVA << " destVa: "
                                                     << destVA << " length: " << size);
        return BM_MALLOC_FAILED;
    }
    auto ret = DlAclApi::AclrtMemcpy2d(tmpHost, dpitch, srcVA, spitch, width, height, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
        rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
        return ret;
    }
    ret = transportManager_->RdmaOneSideTrans(options.destRankId, (uint64_t) tmpHost, (uint64_t) destVA, size, false);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
    }
    rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
    return ret;
}

int32_t HostDataOpRDMA::CopyGva2Device2d(const void *srcVA, uint64_t spitch, void *destVA, uint64_t dpitch,
                                         uint64_t width, uint64_t height, const ExtOptions &options)
{    if (spitch != width) {
        BM_LOG_ERROR("Not support 2d memory on host");
        return BM_ERROR;
    }

    uint64_t size = width * height;
    if (options.srcRankId == rankId_) {
        return DlAclApi::AclrtMemcpy2d(destVA, dpitch, srcVA, spitch, width, height, ACL_MEMCPY_HOST_TO_DEVICE);
    }

    auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(size);
    auto tmpHost = tmpRdmaMemory.Address();
    if (tmpHost == nullptr) {
        BM_LOG_ERROR("Failed to malloc host srcVa: " << srcVA << " destVa: "
                                                     << destVA << " length: " << size);
        return BM_MALLOC_FAILED;
    }
    auto ret = transportManager_->RdmaOneSideTrans(options.srcRankId, (uint64_t) tmpHost, (uint64_t) srcVA, size, true);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
        rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
        return ret;
    }
    ret = DlAclApi::AclrtMemcpy2d(destVA, dpitch, tmpHost, spitch, width, height, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
    }
    rdmaSwapMemoryAllocator_->Release(tmpRdmaMemory);
    return ret;
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
                     << destVA << " length: " << length << " ret: " << ret);
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
                                            uint64_t width,uint64_t height, uint32_t kind, const ExtOptions &options)
{
    void *st = stream_;
    if (options.stream != nullptr) {
        st = options.stream;
    }

    auto ret = DlAclApi::AclrtMemcpy2dAsync(destVA, dpitch, srcVA, spitch, width, height, kind, st);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to add aclrt memory copy2d async task srcVa: " << srcVA << " spitch: " << spitch
                     << " destVA: " << destVA << " width: " << width << " height: " << height
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
