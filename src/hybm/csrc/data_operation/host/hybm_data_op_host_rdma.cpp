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

#include <sys/mman.h>
#include "hybm_space_allocator.h"
#include "hybm_ptracer.h"
#include "dl_hybrid_api.h"
#include "hybm_stream_manager.h"
#include "hybm_va_manager.h"
#include "hcom_service_c_define.h"
#include "hybm_data_op_host_rdma.h"

using namespace ock::mf;

namespace {
#if defined(NO_XPU)
constexpr uint64_t RDMA_SWAP_SPACE_SIZE = 1024 * 1024 * 1024 * 4ULL;
#else
constexpr uint64_t RDMA_SWAP_SPACE_SIZE = 1024 * 1024 * 1024;
#endif
} // namespace

Result HostDataOpRDMA::Initialize() noexcept
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
    BM_LOG_INFO("Allocated memory for swap buffer, buffer size : " << RDMA_SWAP_SPACE_SIZE);
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
    rdmaSwapMemoryAllocator_ = std::make_shared<RbtreeRangePool>((uint8_t *)rdmaSwapBaseAddr_, RDMA_SWAP_SPACE_SIZE);
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

void *HostDataOpRDMA::GetLocalMrAddr(hybm_copy_params &params, hybm_data_copy_direction direction) noexcept
{
    auto realDirection = direction;
    if (UNLIKELY(direction == HYBM_DATA_COPY_DIRECTION_AUTO)) {
        auto &vaMgr = ock::mf::HybmVaManager::GetInstance();
        realDirection =
            vaMgr.InferCopyDirection(reinterpret_cast<uint64_t>(params.src), reinterpret_cast<uint64_t>(params.dest));
    }

    // tbm: 后续使用静态断言或抽函数保证下面的判断不失效
    if (realDirection <= HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE) {
        return params.src;
    } else {
        return params.dest;
    }
}

void HostDataOpRDMA::PreRegisterLocalMr(hybm_copy_params &params, hybm_data_copy_direction direction) noexcept
{
    if (UNLIKELY(transportManager_ == nullptr) || params.dataSize <= HYBM_PRE_REG_SIZE_THRES) {
        return;
    }

    transport::TransportMemoryRegion mr;
    mr.size = params.dataSize;
    mr.flags = transport::REG_MR_FLAG_DRAM;
    mr.addr = reinterpret_cast<uint64_t>(GetLocalMrAddr(params, direction));

    if (transportManager_->QueryHasRegistered(reinterpret_cast<uint64_t>(mr.addr), mr.size)) {
        return;
    }

    auto ret = transportManager_->RegisterMemoryRegion(mr);
    if (ret != BM_OK) {
        BM_LOG_WARN("Failed to pre register host rdma mr, addr:" << mr.addr << " size: " << mr.size);
    }
}

Result HostDataOpRDMA::DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                                const ExtOptions &options) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    Result ret;
    PreRegisterLocalMr(params, direction);
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
    transportManager_->UnregisterMemoryRegion(reinterpret_cast<uint64_t>(GetLocalMrAddr(params, direction)));
    return ret;
}

Result HostDataOpRDMA::DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                                     const ExtOptions &options) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    BM_LOG_ERROR("not supported data copy async!");
    return BM_ERROR;
}

Result HostDataOpRDMA::Wait(int32_t waitId) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    return BM_OK;
}

Result HostDataOpRDMA::CopyHost2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.destRankId == rankId_) {
        return DlHybridApi::Memcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
    }

    if (transportManager_ != nullptr) {
        return SafePut(srcVA, destVA, length, options, true);
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
}

Result HostDataOpRDMA::CopyGva2Host(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.srcRankId == rankId_) {
        return DlHybridApi::Memcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_HOST);
    }

    if (transportManager_ != nullptr) {
        return SafeGet(srcVA, destVA, length, options, true);
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
}

Result HostDataOpRDMA::CopyDevice2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.destRankId == rankId_) {
        return DlHybridApi::Memcpy(destVA, length, srcVA, length, ACL_MEMCPY_DEVICE_TO_HOST);
    }

    if (transportManager_ != nullptr) {
        return SafePut(srcVA, destVA, length, options, false);
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
}

Result HostDataOpRDMA::CopyGva2Device(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
{
    if (options.srcRankId == rankId_) {
        return DlHybridApi::Memcpy(destVA, length, srcVA, length, ACL_MEMCPY_HOST_TO_DEVICE);
    }

    if (transportManager_ != nullptr) {
        return SafeGet(srcVA, destVA, length, options, false);
    } else {
        BM_LOG_ERROR("no transport to other ranks.");
        return BM_ERROR;
    }
}

Result HostDataOpRDMA::CopyGva2Gva(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options)
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

Result ock::mf::HostDataOpRDMA::SafePut(const void *srcVA,
    void *destVA, uint64_t length, const ExtOptions &options, bool isLocalHost)
{
    Result ret = 0;
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
            ret = DlHybridApi::Memcpy(tmpHost, currentChunkSize, currentSrc, currentChunkSize, ACL_MEMCPY_HOST_TO_HOST);
        } else {
            ret =
                DlHybridApi::Memcpy(tmpHost, currentChunkSize, currentSrc, currentChunkSize, ACL_MEMCPY_DEVICE_TO_HOST);
        }
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy device data to host ret: " << ret);
            return ret;
        }
        ret = transportManager_->WriteRemote(options.destRankId, (uint64_t)tmpHost, (uint64_t)currentDest,
                                             currentChunkSize);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
            return ret;
        }
        offset += currentChunkSize;
        remainingLength -= currentChunkSize;
    }
    return ret;
}

Result ock::mf::HostDataOpRDMA::SafeGet(const void *srcVA, void *destVA, uint64_t length, const ExtOptions &options,
                                        bool isLocalHost)
{
    Result ret = 0;
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
        TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_READ_REMOTE);
        ret =
            transportManager_->ReadRemote(options.srcRankId, (uint64_t)tmpHost, (uint64_t)currentSrc, currentChunkSize);
        TP_TRACE_END(TP_HYBM_HOST_RDMA_READ_REMOTE, ret);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to copy host data to remote host memory ret: " << ret);
            return ret;
        }
        if (isLocalHost) {
            ret =
                DlHybridApi::Memcpy(currentDest, currentChunkSize, tmpHost, currentChunkSize, ACL_MEMCPY_HOST_TO_HOST);
        } else {
            ret = DlHybridApi::Memcpy(currentDest, currentChunkSize, tmpHost, currentChunkSize,
                                      ACL_MEMCPY_HOST_TO_DEVICE);
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

void HostDataOpRDMA::BatchPreRegisterLocalMr(hybm_batch_copy_params &params,
                                             hybm_data_copy_direction direction) noexcept
{
    for (size_t i = 0; i < params.batchSize; i++) {
        hybm_copy_params param = {
            .src = &params.sources[i], .dest = &params.destinations[i], .dataSize = params.dataSizes[i]};
        PreRegisterLocalMr(param, direction);
    }
}

void HostDataOpRDMA::BatchUnRegisterLocalMr(hybm_batch_copy_params &params, hybm_data_copy_direction direction) noexcept
{
    for (size_t i = 0; i < params.batchSize; i++) {
        hybm_copy_params param = {
            .src = &params.sources[i], .dest = &params.destinations[i], .dataSize = params.dataSizes[i]};
        transportManager_->UnregisterMemoryRegion(reinterpret_cast<uint64_t>(GetLocalMrAddr(param, direction)));
    }
}

Result HostDataOpRDMA::BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                     const ExtOptions &options) noexcept
{
    BM_ASSERT_RETURN(inited_, BM_NOT_INITIALIZED);
    BatchPreRegisterLocalMr(params, direction);
    Result ret = BM_OK;
    switch (direction) {
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_LD_TO_GH);
            ret = BatchCopyLD2GH(params.destinations, params.sources, params.dataSizes, params.batchSize, options);
            TP_TRACE_END(TP_HYBM_HOST_RDMA_LD_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE: {
            TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_BATCH_GH_TO_LD);
            ret = BatchCopyGH2LD(params.destinations, params.sources, params.dataSizes, params.batchSize, options);
            TP_TRACE_END(TP_HYBM_HOST_RDMA_BATCH_GH_TO_LD, ret);
            break;
        }
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_LH_TO_GH);
            ret = BatchCopyLH2GH(params.destinations, params.sources, params.dataSizes, params.batchSize, options);
            TP_TRACE_END(TP_HYBM_HOST_RDMA_LH_TO_GH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_BATCH_GH_TO_LH);
            ret = BatchCopyGH2LH(params.destinations, params.sources, params.dataSizes, params.batchSize, options);
            TP_TRACE_END(TP_HYBM_HOST_RDMA_BATCH_GH_TO_LH, ret);
            break;
        }
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST: {
            TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_BATCH_GH_TO_GH);
            ret = BatchCopyGH2GH(params.destinations, params.sources, params.dataSizes, params.batchSize, options);
            TP_TRACE_END(TP_HYBM_HOST_RDMA_BATCH_GH_TO_GH, ret);
            break;
        }
        default:
            BM_LOG_ERROR("data copy invalid direction: " << direction);
            ret = BM_INVALID_PARAM;
    }
    BatchUnRegisterLocalMr(params, direction);
    return ret;
}

Result HostDataOpRDMA::BatchCopyLD2LH(void **hostAddrs, void **deviceAddrs, const uint64_t *counts, uint32_t batchSize,
                                      const ExtOptions &options) noexcept
{
    TP_TRACE_BEGIN(TP_HYBM_ACL_BATCH_LD_TO_LH);
    void *st = options.stream;
    Result ret = BM_OK;
    if (st == nullptr) {
        st = HybmStreamManager::GetThreadAclStream();
    }

    for (size_t i = 0; i < batchSize; ++i) {
        auto destAddr = hostAddrs[i];
        auto srcAddr = deviceAddrs[i];
        auto count = counts[i];
        ret = DlHybridApi::MemcpyAsync(destAddr, count, srcAddr, count, ACL_MEMCPY_DEVICE_TO_HOST, st);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local device to local host failed: " << ret << " stream:"
                                                                              << reinterpret_cast<uintptr_t>(st));
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret = DlHybridApi::StreamSynchronize(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << reinterpret_cast<uintptr_t>(st));
    }
    TP_TRACE_END(TP_HYBM_ACL_BATCH_LD_TO_LH, ret);
    return ret;
}

Result HostDataOpRDMA::BatchCopyLH2LD(void **deviceAddrs, void **hostAddrs, const uint64_t *counts, uint32_t batchSize,
                                      const ExtOptions &options) noexcept
{
    TP_TRACE_BEGIN(TP_HYBM_ACL_BATCH_LH_TO_LD);
    void *st = options.stream;
    Result ret = BM_OK;
    if (st == nullptr) {
        st = HybmStreamManager::GetThreadAclStream();
    }

    for (size_t i = 0; i < batchSize; ++i) {
        auto destAddr = deviceAddrs[i];
        auto srcAddr = hostAddrs[i];
        auto count = counts[i];
        ret = DlHybridApi::MemcpyAsync(destAddr, count, srcAddr, count, ACL_MEMCPY_HOST_TO_DEVICE, st);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local host to local device failed: " << ret << " stream:"
                                                                              << reinterpret_cast<uintptr_t>(st));
            return BM_DL_FUNCTION_FAILED;
        }
    }

    ret = DlHybridApi::StreamSynchronize(st);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << reinterpret_cast<uintptr_t>(st));
    }
    TP_TRACE_END(TP_HYBM_ACL_BATCH_LH_TO_LD, ret);
    return ret;
}

void HostDataOpRDMA::ClassifyDataAddr(void **globalAddrs, void **localAddrs, const uint64_t *counts, uint32_t batchSize,
                                      std::unordered_map<uint32_t, CopyDescriptor> &rmtRankMap,
                                      std::unordered_map<uint32_t, CopyDescriptor> &localRankMap,
                                      const uint32_t globalRankId) noexcept
{
    for (size_t i = 0; i < batchSize; ++i) {
        uint32_t gvaRankId = GetRankIdByGva(reinterpret_cast<uint64_t>(globalAddrs[i]));
        if (gvaRankId == UINT32_MAX) {
            gvaRankId = globalRankId;
        }
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

Result HostDataOpRDMA::BatchWriteLD2RH(uint32_t rmtRankId, CopyDescriptor &rmtCopyDescriptor,
                                       const ExtOptions &options) noexcept
{
    Result ret = BM_OK;
    ExtOptions tmpOptions = options;
    tmpOptions.destRankId = rmtRankId;
    size_t batchSize = rmtCopyDescriptor.counts.size();
    uint64_t *ptr = new uint64_t[batchSize * 3];
    BM_ASSERT_RETURN(ptr != nullptr, BM_MALLOC_FAILED);
    uint64_t batchOffset = 0;
    while (batchOffset < batchSize) {
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
            ret = BM_INVALID_PARAM;
            break;
        }
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(currentBatchDataSize);
        void *tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc swap length: " << currentBatchDataSize);
            ret = BM_MALLOC_FAILED;
            break;
        }

        size_t currentBatchSize = batchEnd - batchOffset;
        void **tmpRdmaAddrs = reinterpret_cast<void **>(ptr);
        void **tmplocalAddrs = reinterpret_cast<void **>(ptr + currentBatchSize);
        uint64_t *tmpCounts = (ptr + currentBatchSize + currentBatchSize);
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

        Result errorCode = BM_OK;
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            auto globalAddr = rmtCopyDescriptor.globalAddrs[i];
            auto tmpAddr = tmpRdmaAddrs[i - batchOffset];
            auto count = rmtCopyDescriptor.counts[i];
            ret = transportManager_->WriteRemoteAsync(rmtRankId, (uint64_t)tmpAddr, (uint64_t)globalAddr, count);
            if (ret != BM_OK) {
                errorCode = ret;
                BM_LOG_ERROR("Failed to copy swap memory to remote host memory ret: "
                             << ret << " localRankId:" << rankId_ << " remoteRankId:" << rmtRankId);
                break;
            }
        }
        ret = transportManager_->Synchronize(rmtRankId);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to sync read remote ret: " << ret);
            return ret;
        }
        if (errorCode != BM_OK) {
            return errorCode;
        }
        batchOffset = batchEnd;
    }
    delete[] ptr;
    return ret;
}

Result HostDataOpRDMA::BatchReadRH2LD(uint32_t rmtRankId, CopyDescriptor &rmtCopyDescriptor,
                                      const ExtOptions &options) noexcept
{
    Result ret = BM_OK;
    ExtOptions tmpOptions = options;
    tmpOptions.srcRankId = rmtRankId;
    size_t batchSize = rmtCopyDescriptor.counts.size();
    uint64_t *ptr = new uint64_t[batchSize * 3];
    BM_ASSERT_RETURN(ptr != nullptr, BM_MALLOC_FAILED);
    uint64_t batchOffset = 0;
    while (batchOffset < batchSize) {
        uint64_t currentBatchDataSize = 0;
        size_t batchEnd = batchOffset;
        while (batchEnd < batchSize &&
               currentBatchDataSize + rmtCopyDescriptor.counts[batchEnd] <= RDMA_SWAP_SPACE_SIZE) {
            currentBatchDataSize += rmtCopyDescriptor.counts[batchEnd];
            ++batchEnd;
        }
        if (currentBatchDataSize == 0) {
            BM_LOG_ERROR("Single count exceeds HBM_SWAP_SPACE_SIZE: " << rmtCopyDescriptor.counts[batchOffset] << " > "
                                                                      << RDMA_SWAP_SPACE_SIZE);
            ret = BM_INVALID_PARAM;
            break;
        }
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(currentBatchDataSize);
        void *tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc swap length: " << currentBatchDataSize);
            ret = BM_MALLOC_FAILED;
            break;
        }
        size_t currentBatchSize = batchEnd - batchOffset;
        void **tmpRdmaAddrs = reinterpret_cast<void **>(ptr);
        void **tmplocalAddrs = reinterpret_cast<void **>(ptr + currentBatchSize);
        uint64_t *tmpCounts = (ptr + currentBatchSize + currentBatchSize);
        uint64_t offset = 0;
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            tmpRdmaAddrs[i - batchOffset] = reinterpret_cast<void *>(static_cast<uint8_t *>(tmpHost) + offset);
            tmplocalAddrs[i - batchOffset] = rmtCopyDescriptor.localAddrs[i];
            tmpCounts[i - batchOffset] = rmtCopyDescriptor.counts[i];
            offset += rmtCopyDescriptor.counts[i];
        }
        Result errorCode = BM_OK;
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            auto globalAddr = rmtCopyDescriptor.globalAddrs[i];
            auto tmpAddr = tmpRdmaAddrs[i - batchOffset];
            auto count = rmtCopyDescriptor.counts[i];
            ret = transportManager_->ReadRemoteAsync(rmtRankId, (uint64_t)tmpAddr, (uint64_t)globalAddr, count);
            if (ret != BM_OK) {
                errorCode = ret;
                BM_LOG_ERROR("Failed to copy swap memory to remote host memory ret: "
                             << ret << " localRankId:" << rankId_ << " remoteRankId:" << rmtRankId);
                break;
            }
        }
        ret = transportManager_->Synchronize(rmtRankId);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to sync read remote ret: " << ret);
            return ret;
        }
        if (errorCode != 0) {
            return errorCode;
        }
        ret = BatchCopyLH2LD((void **)tmplocalAddrs, (void **)tmpRdmaAddrs, tmpCounts, currentBatchSize, tmpOptions);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to copy local device to swap memory: " << ret);
            break;
        }
        batchOffset = batchEnd;
    }
    delete[] ptr;
    return ret;
}

Result HostDataOpRDMA::BatchCopyLD2GH(void **destinations, void **sources, const uint64_t *counts, uint32_t batchSize,
                                      const ExtOptions &options) noexcept
{
    Result ret = BM_OK;
    ExtOptions tmpOptions = options;
    std::unordered_map<uint32_t, CopyDescriptor> rmtRankMap{};
    std::unordered_map<uint32_t, CopyDescriptor> localRankMap{};
    ClassifyDataAddr(destinations, sources, counts, batchSize, rmtRankMap, localRankMap, options.destRankId);

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

Result HostDataOpRDMA::BatchCopyGH2LD(void **destinations, void **sources, const uint64_t *counts, uint32_t batchSize,
                                      const ExtOptions &options) noexcept
{
    Result ret = BM_OK;
    ExtOptions tmpOptions = options;
    std::unordered_map<uint32_t, CopyDescriptor> rmtRankMap{};
    std::unordered_map<uint32_t, CopyDescriptor> localRankMap{};
    ClassifyDataAddr(sources, destinations, counts, batchSize, rmtRankMap, localRankMap, options.srcRankId);

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

Result HostDataOpRDMA::BatchCopyLH2LH(void **destAddrs, void **srcAddrs, const uint64_t *counts,
                                      uint32_t batchSize) noexcept
{
    Result ret = BM_OK;
    for (uint32_t i = 0; i < batchSize; ++i) {
        ret = DlHybridApi::Memcpy(destAddrs[i], counts[i], srcAddrs[i], counts[i], ACL_MEMCPY_HOST_TO_HOST);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to copy local host to local host ret: " << ret);
            return ret;
        }
    }
    return 0;
}

Result HostDataOpRDMA::InnerBatchReadRH2LH(const CopyDescriptor &rmtCopyDescriptor, const ExtOptions &options,
                                           uint64_t batchOffset, size_t batchEnd, void *tmpRdmaAddrs[]) const
{
    Result ret;
    TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_READ_REMOTE_ASYNC);
    Result errorCode = BM_OK;
    for (size_t i = batchOffset; i < batchEnd; ++i) {
        auto globalAddr = rmtCopyDescriptor.globalAddrs[i];
        auto tmpAddr = tmpRdmaAddrs[i - batchOffset];
        auto count = rmtCopyDescriptor.counts[i];
        TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_READ_REMOTE_ASYNC_SUBMIT);
        ret = transportManager_->ReadRemoteAsync(options.srcRankId, (uint64_t)tmpAddr, (uint64_t)globalAddr, count);
        TP_TRACE_END(TP_HYBM_HOST_RDMA_READ_REMOTE_ASYNC_SUBMIT, ret);
        if (ret != BM_OK) {
            errorCode = ret;
            BM_LOG_ERROR("Failed to Call ReadRemoteAsync ret: " << ret << " localRankId:" << rankId_
                                                                << " remoteRankId:" << options.srcRankId);
            break;
        }
    }
    TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_READ_REMOTE_ASYNC_WAIT);
    ret = transportManager_->Synchronize(options.srcRankId);
    TP_TRACE_END(TP_HYBM_HOST_RDMA_READ_REMOTE_ASYNC_WAIT, ret);
    TP_TRACE_END(TP_HYBM_HOST_RDMA_READ_REMOTE_ASYNC, ret);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to sync read remote ret: " << ret);
        return ret;
    }
    if (errorCode != BM_OK) {
        return errorCode;
    }
    return ret;
}

Result HostDataOpRDMA::BatchReadRH2LH(CopyDescriptor &rmtCopyDescriptor, const ExtOptions &options) noexcept
{
    Result ret = BM_OK;
    size_t batchSize = rmtCopyDescriptor.counts.size();
    uint64_t batchOffset = 0;
    while (batchOffset < batchSize) {
        uint64_t currentBatchDataSize = 0;
        size_t batchEnd = batchOffset;
        while (batchEnd < batchSize &&
               currentBatchDataSize + rmtCopyDescriptor.counts[batchEnd] <= RDMA_SWAP_SPACE_SIZE) {
            currentBatchDataSize += rmtCopyDescriptor.counts[batchEnd];
            ++batchEnd;
        }
        if (currentBatchDataSize == 0) {
            BM_LOG_ERROR("Single count exceeds HBM_SWAP_SPACE_SIZE: " << rmtCopyDescriptor.counts[batchOffset] << " > "
                                                                      << RDMA_SWAP_SPACE_SIZE);
            return BM_INVALID_PARAM;
        }
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(currentBatchDataSize);
        void *tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc swap length: " << currentBatchDataSize);
            return BM_MALLOC_FAILED;
        }
        size_t currentBatchSize = batchEnd - batchOffset;
        std::vector<void *> tmpRdmaAddrs(currentBatchSize);
        std::vector<void *> tmplocalAddrs(currentBatchSize);
        std::vector<uint64_t> tmpCounts(currentBatchSize);
        uint64_t offset = 0;
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            tmpRdmaAddrs[i - batchOffset] = static_cast<uint8_t *>(tmpHost) + offset;
            tmplocalAddrs[i - batchOffset] = rmtCopyDescriptor.localAddrs[i];
            tmpCounts[i - batchOffset] = rmtCopyDescriptor.counts[i];
            offset += rmtCopyDescriptor.counts[i];
        }
        ret = InnerBatchReadRH2LH(rmtCopyDescriptor, options, batchOffset, batchEnd, tmpRdmaAddrs.data());
        if (ret != BM_OK) {
            return ret;
        }
        TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_BATCH_LOCAL_COPY);
        ret = BatchCopyLH2LH(tmplocalAddrs.data(), tmpRdmaAddrs.data(), tmpCounts.data(), currentBatchSize);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to copy local device to swap memory: " << ret);
            return ret;
        }
        TP_TRACE_END(TP_HYBM_HOST_RDMA_BATCH_LOCAL_COPY, ret);
        batchOffset = batchEnd;
    }
    return ret;
}

Result HostDataOpRDMA::InnerBatchWriteLH2RH(const CopyDescriptor &rmtCopyDescriptor, const ExtOptions &options,
    uint64_t batchOffset, size_t batchEnd, void *tmpRdmaAddrs[]) const
{
    Result ret;

    Result errorCode = BM_OK;
    for (size_t i = batchOffset; i < batchEnd; ++i) {
        auto globalAddr = rmtCopyDescriptor.globalAddrs[i];
        auto tmpAddr = tmpRdmaAddrs[i - batchOffset];
        auto count = rmtCopyDescriptor.counts[i];
        TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_READ_REMOTE_ASYNC_SUBMIT);
        ret = transportManager_->WriteRemoteAsync(options.destRankId, (uint64_t)tmpAddr, (uint64_t)globalAddr, count);
        TP_TRACE_END(TP_HYBM_HOST_RDMA_READ_REMOTE_ASYNC_SUBMIT, ret);
        if (ret != BM_OK) {
            errorCode = ret;
            BM_LOG_ERROR("Failed to Call ReadRemoteAsync ret: " << ret << " localRankId:" << rankId_
                                                                << " remoteRankId:" << options.srcRankId);
            break;
        }
    }
    ret = transportManager_->Synchronize(options.destRankId);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to sync read remote ret: " << ret);
        return ret;
    }
    if (errorCode != BM_OK) {
        return errorCode;
    }
    return ret;
}

Result HostDataOpRDMA::BatchWriteLH2RH(CopyDescriptor &rmtCopyDescriptor, const ExtOptions &options) noexcept
{
    Result ret = BM_OK;
    size_t batchSize = rmtCopyDescriptor.counts.size();
    uint64_t batchOffset = 0;
    while (batchOffset < batchSize) {
        uint64_t currentBatchDataSize = 0;
        size_t batchEnd = batchOffset;
        while (batchEnd < batchSize &&
               currentBatchDataSize + rmtCopyDescriptor.counts[batchEnd] <= RDMA_SWAP_SPACE_SIZE) {
            currentBatchDataSize += rmtCopyDescriptor.counts[batchEnd];
            ++batchEnd;
        }
        if (currentBatchDataSize == 0) {
            BM_LOG_ERROR("Single count exceeds HBM_SWAP_SPACE_SIZE: " << rmtCopyDescriptor.counts[batchOffset] << " > "
                                                                      << RDMA_SWAP_SPACE_SIZE);
            return BM_INVALID_PARAM;
        }
        auto tmpRdmaMemory = rdmaSwapMemoryAllocator_->Allocate(currentBatchDataSize);
        void *tmpHost = tmpRdmaMemory.Address();
        if (tmpHost == nullptr) {
            BM_LOG_ERROR("Failed to malloc swap length: " << currentBatchDataSize);
            return BM_MALLOC_FAILED;
        }
        size_t currentBatchSize = batchEnd - batchOffset;
        std::vector<void *> tmpRdmaAddrs(currentBatchSize);
        std::vector<void *> tmplocalAddrs(currentBatchSize);
        std::vector<uint64_t> tmpCounts(currentBatchSize);
        uint64_t offset = 0;
        for (size_t i = batchOffset; i < batchEnd; ++i) {
            tmpRdmaAddrs[i - batchOffset] = static_cast<uint8_t *>(tmpHost) + offset;
            tmplocalAddrs[i - batchOffset] = rmtCopyDescriptor.localAddrs[i];
            tmpCounts[i - batchOffset] = rmtCopyDescriptor.counts[i];
            offset += rmtCopyDescriptor.counts[i];
        }
        TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_BATCH_LOCAL_COPY);
        ret = BatchCopyLH2LH(tmpRdmaAddrs.data(), tmplocalAddrs.data(), tmpCounts.data(), currentBatchSize);
        if (ret != 0) {
            BM_LOG_ERROR("Failed to copy local device to swap memory: " << ret);
            return ret;
        }
        TP_TRACE_END(TP_HYBM_HOST_RDMA_BATCH_LOCAL_COPY, ret);

        ret = InnerBatchWriteLH2RH(rmtCopyDescriptor, options, batchOffset, batchEnd, tmpRdmaAddrs.data());
        if (ret != BM_OK) {
            return ret;
        }
        batchOffset = batchEnd;
    }
    return ret;
}

Result HostDataOpRDMA::BatchCopyLH2GH(void **gvaAddrs, void **hostAddrs, const uint64_t *counts, uint32_t batchSize,
                                      const ExtOptions &options) noexcept
{
    Result ret = BM_OK;
    if (options.destRankId == rankId_) {
        for (auto i = 0U; i < batchSize; i++) {
            auto ret = DlHybridApi::Memcpy(gvaAddrs[i], counts[i], hostAddrs[i], counts[i], ACL_MEMCPY_HOST_TO_HOST);
            if (ret != 0) {
                BM_LOG_ERROR("copy local host to GVA failed: " << ret);
                return ret;
            }
        }
    } else {
        bool registered = true;
        for (uint32_t i = 0U; i < batchSize; i++) {
            if (!transportManager_->QueryHasRegistered(reinterpret_cast<uint64_t>(hostAddrs[i]), counts[i])) {
                registered = false;
                break;
            }
        }
        if (registered) {
            ret = BatchCopyGH2GH(gvaAddrs, hostAddrs, counts, batchSize, options);
        } else {
            std::vector<void *> localAddrs;
            std::vector<void *> globalAddrs;
            std::vector<uint64_t> countArr;
            localAddrs.reserve(batchSize);
            globalAddrs.reserve(batchSize);
            countArr.reserve(batchSize);
            for (uint32_t i = 0; i < batchSize; ++i) {
                localAddrs.push_back(hostAddrs[i]);
                globalAddrs.push_back(gvaAddrs[i]);
                countArr.push_back(counts[i]);
            }
            CopyDescriptor desc{localAddrs, globalAddrs, countArr};
            ret = BatchWriteLH2RH(desc, options);
        }
    }
    if (ret != 0) {
        BM_LOG_ERROR("Failed to BatchCopyGH2LH ret: " << ret);
        return ret;
    }
    return BM_OK;
}


Result HostDataOpRDMA::BatchCopyGH2LH(void **hostAddrs, void **gvaAddrs, const uint64_t *counts, uint32_t batchSize,
                                      const ExtOptions &options) noexcept
{
    Result ret = BM_OK;
    if (options.srcRankId == rankId_) {
        TP_TRACE_BEGIN(TP_HYBM_HOST_RDMA_BATCH_LH_TO_LH);
        ret = BatchCopyLH2LH(hostAddrs, gvaAddrs, counts, batchSize);
        TP_TRACE_END(TP_HYBM_HOST_RDMA_BATCH_LH_TO_LH, ret);
    } else {
        bool registered = true;
        for (uint32_t i = 0U; i < batchSize; i++) {
            if (!transportManager_->QueryHasRegistered(reinterpret_cast<uint64_t>(hostAddrs[i]), counts[i])) {
                registered = false;
                break;
            }
        }
        if (registered) {
            ret = BatchCopyGH2GH(hostAddrs, gvaAddrs, counts, batchSize, options);
        } else {
            CopyDescriptor desc{};
            desc.localAddrs.reserve(batchSize);
            desc.globalAddrs.reserve(batchSize);
            desc.counts.reserve(batchSize);
            for (uint32_t i = 0; i < batchSize; ++i) {
                desc.localAddrs.push_back(hostAddrs[i]);
                desc.globalAddrs.push_back(gvaAddrs[i]);
                desc.counts.push_back(counts[i]);
            }
            ret = BatchReadRH2LH(desc, options);
        }
    }
    if (ret != 0) {
        BM_LOG_ERROR("Failed to BatchCopyGH2LH ret: " << ret);
        return ret;
    }
    return BM_OK;
}

Result HostDataOpRDMA::BatchCopyGH2GH(void **destAddrs, void **srcAddrs, const uint64_t *counts, uint32_t batchSize,
                                      const ExtOptions &options) noexcept
{
    Result ret = BM_OK;
    bool isPut = options.srcRankId == rankId_;
    Result errorCode = BM_OK;

    CopyDescriptor smallIoDes;
    CopyDescriptor bigIoDes;
    for (auto i = 0U; i < batchSize; i++) {
        if (counts[i] <= SMALL_IO_LIMIT_SIZE) {
            smallIoDes.localAddrs.emplace_back(srcAddrs[i]);
            smallIoDes.globalAddrs.emplace_back(destAddrs[i]);
            smallIoDes.counts.emplace_back(counts[i]);
        } else {
            bigIoDes.localAddrs.emplace_back(srcAddrs[i]);
            bigIoDes.globalAddrs.emplace_back(destAddrs[i]);
            bigIoDes.counts.emplace_back(counts[i]);
        }
    }

    if (!smallIoDes.counts.empty()) {
        if (isPut) {
            ret = transportManager_->WriteRemoteBatchAsync(options.destRankId, smallIoDes);
        } else {
            ret = transportManager_->ReadRemoteBatchAsync(options.srcRankId, smallIoDes);
        }
    }
    if (ret == 0) {
        for (auto i = 0U; i < bigIoDes.counts.size(); i++) {
            if (isPut) {
                ret = transportManager_->WriteRemoteAsync(options.destRankId,
                    reinterpret_cast<uint64_t>(bigIoDes.localAddrs[i]),
                    reinterpret_cast<uint64_t>(bigIoDes.globalAddrs[i]), bigIoDes.counts[i]);
            } else {
                ret = transportManager_->ReadRemoteAsync(options.srcRankId,
                    reinterpret_cast<uint64_t>(bigIoDes.globalAddrs[i]),
                    reinterpret_cast<uint64_t>(bigIoDes.localAddrs[i]), bigIoDes.counts[i]);
            }
            if (ret != 0) {
                errorCode = ret;
                BM_LOG_ERROR("Failed to submit host rdma task, ret: " << ret << " srcRank:" << options.srcRankId
                                                                      << " destRank:" << options.destRankId
                                                                      << " length:" << counts[i]);
                break;
            }
        }
    } else {
        BM_LOG_ERROR("Failed to submit host rdma task, ret: " << ret << " srcRank:" << options.srcRankId
                                                              << " destRank:" << options.destRankId);
        errorCode = ret;
    }

    ret = transportManager_->Synchronize(isPut ? options.destRankId : options.srcRankId);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to sync host rdma tasks, ret: " << ret);
        return ret;
    }
    if (errorCode != BM_OK) {
        return errorCode;
    }
    return ret;
}