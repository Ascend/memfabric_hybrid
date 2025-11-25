/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "hybm_data_op_host_rdma.h"
#include <sys/mman.h>
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
    rdmaSwapBaseAddr_ =
        mmap(nullptr, RDMA_SWAP_SPACE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_HUGETLB | MAP_PRIVATE, -1, 0);
    if (rdmaSwapBaseAddr_ == MAP_FAILED) {
        BM_LOG_ERROR("Failed to alloc size:" << RDMA_SWAP_SPACE_SIZE << " error:" << errno << ", "
                                             << SafeStrError(errno));
        return BM_ERROR;
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
        munmap(rdmaSwapBaseAddr_, RDMA_SWAP_SPACE_SIZE);
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
        return SafePut(srcVA, destVA, length, options, true);
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
        return SafeGet(srcVA, destVA, length, options, true);
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
        return SafePut(srcVA, destVA, length, options, false);
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
        return SafeGet(srcVA, destVA, length, options, false);
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

int32_t ock::mf::HostDataOpRDMA::SafePut(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options,
                                         bool isLocalHost)
{
    int32_t ret = 0;
    uintptr_t srcBase = reinterpret_cast<uintptr_t>(srcVA);
    uintptr_t destBase = reinterpret_cast<uintptr_t>(destVA);
    uint64_t remainingLength = length;
    uint64_t offset = 0;
    while (remainingLength > 0) {
        uint64_t currentChunkSize = std::min(remainingLength, RDMA_SWAP_SPACE_SIZE);
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(currentChunkSize);
        auto tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc host srcVa: " << reinterpret_cast<uintptr_t>(srcVA)
                                                         << " destVa: " << reinterpret_cast<uintptr_t>(destVA)
                                                         << " length: " << currentChunkSize);
            return BM_MALLOC_FAILED;
        }
        const void *currentSrc = reinterpret_cast<const void *>(srcBase + offset);
        void *currentDest = reinterpret_cast<void *>(destBase + offset);
        if (isLocalHost) {
            ret = DlAclApi::AclrtMemcpy(tmpHost, currentChunkSize, currentSrc,
                currentChunkSize, ACL_MEMCPY_HOST_TO_HOST);
        } else {
            ret = DlAclApi::AclrtMemcpy(tmpHost, currentChunkSize, currentSrc,
                currentChunkSize, ACL_MEMCPY_DEVICE_TO_HOST);
        }
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
            return ret;
        }
        ret = transportManager_->WriteRemote(options.destRankId, (uint64_t)tmpHost,
            (uint64_t)currentDest, currentChunkSize);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
            return ret;
        }
        offset += currentChunkSize;
        remainingLength -= currentChunkSize;
    }
    return ret;
}

int32_t ock::mf::HostDataOpRDMA::SafeGet(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options,
                                         bool isLocalHost)
{
    int32_t ret = 0;
    uintptr_t srcBase = reinterpret_cast<uintptr_t>(srcVA);
    uintptr_t destBase = reinterpret_cast<uintptr_t>(destVA);
    uint64_t remainingLength = length;
    uint64_t offset = 0;
    while (remainingLength > 0) {
        uint64_t currentChunkSize = std::min(remainingLength, RDMA_SWAP_SPACE_SIZE);
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(currentChunkSize);
        auto tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc host srcVa: " << reinterpret_cast<uintptr_t>(srcVA)
                                                         << " destVa: " << reinterpret_cast<uintptr_t>(destVA)
                                                         << " length: " << currentChunkSize);
            return BM_MALLOC_FAILED;
        }
        const void *currentSrc = reinterpret_cast<const void *>(srcBase + offset);
        void *currentDest = reinterpret_cast<void *>(destBase + offset);
        ret = transportManager_->ReadRemote(options.srcRankId, (uint64_t)tmpHost,
            (uint64_t)currentSrc, currentChunkSize);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
            return ret;
        }
        if (isLocalHost) {
            ret = DlAclApi::AclrtMemcpy(currentDest, currentChunkSize, tmpHost,
                currentChunkSize, ACL_MEMCPY_HOST_TO_HOST);
        } else {
            ret = DlAclApi::AclrtMemcpy(currentDest, currentChunkSize, tmpHost,
                currentChunkSize, ACL_MEMCPY_HOST_TO_DEVICE);
        }
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
            return ret;
        }
        offset += currentChunkSize;
        remainingLength -= currentChunkSize;
    }
    return ret;
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

void HostDataOpRDMA::ClassifyDataAddr(void **globalAddrs, void **localAddrs, const uint64_t *counts, uint32_t batchSize,
                                      std::unordered_map<uint32_t, CopyDescriptor> &rmtRankMap,
                                      std::unordered_map<uint32_t, CopyDescriptor> &localRankMap) noexcept
{
    for (size_t i = 0; i < batchSize; ++i) {
        uint32_t gvaRankId = GetRankIdByGva(reinterpret_cast<uint64_t>(globalAddrs[i]));
        if (gvaRankId == rankId_) {
            auto iter = localRankMap.find(gvaRankId);
            if (iter == localRankMap.end()) {
                CopyDescriptor desc{};
                desc.localAddrs.push_back(localAddrs[i]);
                desc.globalAddrs.push_back(globalAddrs[i]);
                desc.counts.push_back(counts[i]);
                localRankMap.emplace(std::make_pair(gvaRankId, desc));
            } else {
                iter->second.localAddrs.push_back(localAddrs[i]);
                iter->second.globalAddrs.push_back(globalAddrs[i]);
                iter->second.counts.push_back(counts[i]);
            }
        } else {
            auto iter = rmtRankMap.find(gvaRankId);
            if (iter == rmtRankMap.end()) {
                CopyDescriptor desc{};
                desc.localAddrs.push_back(localAddrs[i]);
                desc.globalAddrs.push_back(globalAddrs[i]);
                desc.counts.push_back(counts[i]);
                rmtRankMap.emplace(std::make_pair(gvaRankId, desc));
            } else {
                iter->second.localAddrs.push_back(localAddrs[i]);
                iter->second.globalAddrs.push_back(globalAddrs[i]);
                iter->second.counts.push_back(counts[i]);
            }
        }
    }
}

int HostDataOpRDMA::BatchWriteLD2RH(uint32_t rmtRankId, CopyDescriptor &rmtCopyDescriptor,
                                    const ExtOptions &options) noexcept
{
    auto ret = 0;
    ExtOptions tmpOptions = options;
    tmpOptions.destRankId = rmtRankId;
    // 分批处理：每批最大不超过 RDMA_SWAP_SPACE_SIZE
    size_t batchSize = rmtCopyDescriptor.counts.size();
    uint64_t batchOffset = 0; // 当前处理的 rmtCopyDescriptor 索引
    while (batchOffset < batchSize) {
        // 计算当前批次能拷贝的最大数据量
        uint64_t currentBatchDataSize = 0;
        size_t batchEnd = batchOffset;
        while (batchEnd < batchSize &&
               currentBatchDataSize + rmtCopyDescriptor.counts[batchEnd] <= RDMA_SWAP_SPACE_SIZE) {
            currentBatchDataSize += rmtCopyDescriptor.counts[batchEnd];
            ++batchEnd;
        }

        // 如果连一个都放不下，说明单个 count 太大
        if (currentBatchDataSize == 0) {
            BM_LOG_ERROR("Single count exceeds HBM_SWAP_SPACE_SIZE: " << rmtCopyDescriptor.counts[batchOffset] << " > "
                                                                      << RDMA_SWAP_SPACE_SIZE);
            return BM_INVALID_PARAM;
        }

        // 分配当前批次的临时 HBM 内存
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(currentBatchDataSize);
        void *tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc swap length: " << currentBatchDataSize);
            return BM_MALLOC_FAILED;
        }

        // 先copy到tmp内存
        size_t currentBatchSize = batchEnd - batchOffset;
        void *tmpRdmaAddrs[currentBatchSize];
        void *tmplocalAddrs[currentBatchSize];
        uint64_t tmpCounts[currentBatchSize];
        uint64_t offset = 0;
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            tmpRdmaAddrs[i - batchOffset] = reinterpret_cast<void *>(static_cast<uint8_t *>(tmpHost) + offset);
            tmplocalAddrs[i - batchOffset] = rmtCopyDescriptor.localAddrs[i];
            tmpCounts[i - batchOffset] = rmtCopyDescriptor.counts[i];
            offset += rmtCopyDescriptor.counts[i];
        }

        ret = BatchCopyLD2LH((void **)tmpRdmaAddrs, (void **)tmplocalAddrs, tmpCounts, currentBatchSize, tmpOptions);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to copy local device to swap memory: " << ret);
            return ret;
        }

        // 从tmp copy到目标位置
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            auto glabalAddr = rmtCopyDescriptor.globalAddrs[i];
            auto tmpAddr = tmpRdmaAddrs[i - batchOffset];
            auto count = rmtCopyDescriptor.counts[i];
            ret = transportManager_->WriteRemote(rmtRankId, (uint64_t)tmpAddr, (uint64_t)glabalAddr, count);
            if (ret != BM_OK) {
                BM_LOG_ERROR("Failed to copy swap memory to remote host memory ret: "
                             << ret << " localRankId:" << rankId_ << " remoteRankId:" << rmtRankId);
            }
        }

        // 下一次迭代
        batchOffset = batchEnd;
    }
    return ret;
}

int HostDataOpRDMA::BatchReadRH2LD(uint32_t rmtRankId, CopyDescriptor &rmtCopyDescriptor,
                                   const ExtOptions &options) noexcept
{
    auto ret = 0;
    ExtOptions tmpOptions = options;
    tmpOptions.srcRankId = rmtRankId;
    // 分批处理：每批最大不超过 RDMA_SWAP_SPACE_SIZE
    size_t batchSize = rmtCopyDescriptor.counts.size();
    uint64_t batchOffset = 0; // 当前处理的 rmtCopyDescriptor 索引
    while (batchOffset < batchSize) {
        // 计算当前批次能拷贝的最大数据量
        uint64_t currentBatchDataSize = 0;
        size_t batchEnd = batchOffset;
        while (batchEnd < batchSize &&
               currentBatchDataSize + rmtCopyDescriptor.counts[batchEnd] <= RDMA_SWAP_SPACE_SIZE) {
            currentBatchDataSize += rmtCopyDescriptor.counts[batchEnd];
            ++batchEnd;
        }

        // 如果连一个都放不下，说明单个 count 太大
        if (currentBatchDataSize == 0) {
            BM_LOG_ERROR("Single count exceeds HBM_SWAP_SPACE_SIZE: " << rmtCopyDescriptor.counts[batchOffset] << " > "
                                                                      << RDMA_SWAP_SPACE_SIZE);
            return BM_INVALID_PARAM;
        }

        // 分配当前批次的临时 HBM 内存
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(currentBatchDataSize);
        void *tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc swap length: " << currentBatchDataSize);
            return BM_MALLOC_FAILED;
        }

        // 先copy到tmp内存
        size_t currentBatchSize = batchEnd - batchOffset;
        void *tmpRdmaAddrs[currentBatchSize];
        void *tmplocalAddrs[currentBatchSize];
        uint64_t tmpCounts[currentBatchSize];
        uint64_t offset = 0;
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            tmpRdmaAddrs[i - batchOffset] = reinterpret_cast<void *>(static_cast<uint8_t *>(tmpHost) + offset);
            tmplocalAddrs[i - batchOffset] = rmtCopyDescriptor.localAddrs[i];
            tmpCounts[i - batchOffset] = rmtCopyDescriptor.counts[i];
            offset += rmtCopyDescriptor.counts[i];
        }

        for (size_t i = batchOffset; i < batchEnd; ++i) {
            auto glabalAddr = rmtCopyDescriptor.globalAddrs[i];
            auto tmpAddr = tmpRdmaAddrs[i - batchOffset];
            auto count = rmtCopyDescriptor.counts[i];
            ret = transportManager_->ReadRemote(rmtRankId, (uint64_t)tmpAddr, (uint64_t)glabalAddr, count);
            if (ret != BM_OK) {
                BM_LOG_ERROR("Failed to copy swap memory to remote host memory ret: "
                             << ret << " localRankId:" << rankId_ << " remoteRankId:" << rmtRankId);
            }
        }

        // 从tmp copy到目标位置
        ret = BatchCopyLH2LD((void **)tmplocalAddrs, (void **)tmpRdmaAddrs, tmpCounts, currentBatchSize, tmpOptions);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to copy local device to swap memory: " << ret);
            return ret;
        }

        // 下一次迭代
        batchOffset = batchEnd;
    }
    return ret;
}

int HostDataOpRDMA::BatchCopyLD2GH(void **destinations, void **sources, const uint64_t *counts, uint32_t batchSize,
                                   const ExtOptions &options) noexcept
{
    auto ret = 0;
    ExtOptions tmpOptions = options;
    std::unordered_map<uint32_t, CopyDescriptor> rmtRankMap{};
    std::unordered_map<uint32_t, CopyDescriptor> localRankMap{};
    ClassifyDataAddr(destinations, sources, counts, batchSize, rmtRankMap, localRankMap);

    for (auto &it : localRankMap) {
        tmpOptions.destRankId = it.first;
        ret = BatchCopyLD2LH(it.second.globalAddrs.data(), it.second.localAddrs.data(), it.second.counts.data(),
                             it.second.counts.size(), tmpOptions);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to write local device to local host ret: " << ret);
            return ret;
        }
    }

    for (auto &it : rmtRankMap) {
        ret = BatchWriteLD2RH(it.first, it.second, options);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to write local device to remote host ret: " << ret);
            return ret;
        }
    }
    return ret;
}

int HostDataOpRDMA::BatchCopyGH2LD(void **destinations, void **sources, const uint64_t *counts, uint32_t batchSize,
                                   const ExtOptions &options) noexcept
{
    auto ret = 0;
    ExtOptions tmpOptions = options;
    std::unordered_map<uint32_t, CopyDescriptor> rmtRankMap{};
    std::unordered_map<uint32_t, CopyDescriptor> localRankMap{};
    ClassifyDataAddr(sources, destinations, counts, batchSize, rmtRankMap, localRankMap);

    for (auto &it : localRankMap) {
        tmpOptions.srcRankId = it.first;
        ret = BatchCopyLH2LD(it.second.localAddrs.data(), it.second.globalAddrs.data(), it.second.counts.data(),
                             it.second.counts.size(), tmpOptions);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to write local device to local host ret: " << ret);
            return ret;
        }
    }

    for (auto &it : rmtRankMap) {
        ret = BatchReadRH2LD(it.first, it.second, options);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to write local device to remote host ret: " << ret);
            return ret;
        }
    }
    return ret;
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