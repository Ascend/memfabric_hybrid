/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <cstring>
#include <iomanip>
#include "runtime_api.h"
#include "hybm_logger.h"
#include "hybm_cmd.h"
#include "hybm_ex_info_transfer.h"
#include "devmm_svm_gva.h"
#include "hybm_devide_mem_segment.h"

namespace ock {
namespace mf {
int MemSegmentDevice::deviceId_{-1};
int MemSegmentDevice::pid_{-1};
uint32_t MemSegmentDevice::sdid_{0};

Result MemSegmentDevice::ValidateOptions() noexcept
{
    if (options_.segType != HyBM_MST_HBM || options_.size == 0 || (options_.size % DEVICE_LARGE_PAGE_SIZE) != 0) {
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

Result MemSegmentDevice::PrepareVirtualMemory(uint32_t rankNo, uint32_t rankCnt, void **address) noexcept
{
    if (rankCount_ > 0) {
        BM_LOG_ERROR("already prepare virtual memory.");
        return BM_ERROR;
    }

    if (rankNo >= rankCnt) {
        BM_LOG_ERROR("rank(" << rankNo << ") but total " << rankCnt);
        return BM_INVALID_PARAM;
    }

    void *base = nullptr;
    totalVirtualSize_ = rankCnt * options_.size;
    auto ret = RuntimeApi::HalGvaReserveMemory(&base, totalVirtualSize_, deviceId_, 0ULL);
    if (ret != 0 || base == nullptr) {
        BM_LOG_ERROR("prepare virtual memory size(" << totalVirtualSize_ << ") failed. ret: " << ret);
        return BM_MALLOC_FAILED;
    }

    rankIndex_ = rankNo;
    rankCount_ = rankCnt;
    globalVirtualAddress_ = reinterpret_cast<uint8_t *>(base);
    allocatedSize_ = 0UL;
    sliceCount_ = 0;
    *address = base;
    return BM_OK;
}

Result MemSegmentDevice::AllocMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    if ((size % DEVICE_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.size) {
        BM_LOG_ERROR("invalid allocate memory size : " << size << ", now used " << allocatedSize_ << " of "
                                                       << options_.size);
        return BM_INVALID_PARAM;
    }

    auto localVirtualBase = globalVirtualAddress_ + options_.size * rankIndex_;
    auto ret = RuntimeApi::HalGvaAlloc((void *)(localVirtualBase + allocatedSize_), size, 0);
    if (ret != BM_OK) {
        BM_LOG_ERROR("HalGvaAlloc memory failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    auto sliceAddr = localVirtualBase + allocatedSize_;
    allocatedSize_ += size;
    slice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_SVM,
                                       (uint64_t)(ptrdiff_t)(void *)sliceAddr, size);
    slices_.emplace(slice->index_, slice);
    BM_LOG_DEBUG("allocate slice(idx:" << slice->index_ << ", size:" << slice->size_ << ", address:0x" << std::hex
                                       << slice->vAddress_ << ").");

    return BM_OK;
}

Result MemSegmentDevice::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

// export不可重入
Result MemSegmentDevice::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
{
    auto pos = slices_.find(slice->index_);
    if (pos == slices_.end()) {
        BM_LOG_ERROR("input slice(idx:" << slice->index_ << ") not exist.");
        return BM_INVALID_PARAM;
    }

    if (pos->second.slice != slice) {
        BM_LOG_ERROR("input slice(magic:" << std::hex << slice->magic_ << ") not match.");
        return BM_INVALID_PARAM;
    }

    auto exp = exportMap_.find(slice->index_);
    if (exp != exportMap_.end()) { // RtIpcSetMemoryName不支持重复调用
        exInfo = exp->second;
        return BM_OK;
    }

    HbmExportInfo info{};
    auto ret = RuntimeApi::RtIpcSetMemoryName((void *)(ptrdiff_t)slice->vAddress_, slice->size_, info.shmName,
                                              sizeof(info.shmName));
    if (ret != 0) {
        BM_LOG_ERROR("set memory name failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    info.magic = EXPORT_INFO_MAGIC;
    info.version = EXPORT_INFO_VERSION;
    info.mappingOffset = slice->vAddress_ - (uint64_t)(ptrdiff_t)(globalVirtualAddress_ + options_.size * rankIndex_);
    info.sliceIndex = static_cast<uint32_t>(slice->index_);
    info.deviceId = deviceId_;
    info.pid = pid_;
    info.rankId = rankIndex_;
    info.size = slice->size_;
    info.entityId = entityId_;
    info.sdid = sdid_;
    info.pageTblType = MEM_PT_TYPE_SVM;
    info.memSegType = HyBM_MST_HBM;
    info.exchangeType = HyBM_INFO_EXG_IN_NODE;
    ret = LiteralExInfoTranslater<HbmExportInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        RuntimeApi::RtIpcDestroyMemoryName(info.shmName);
        return BM_ERROR;
    }

    exportMap_[slice->index_] = exInfo;
    return BM_OK;
}

// import可重入
Result MemSegmentDevice::Import(const std::vector<std::string> &allExInfo) noexcept
{
    LiteralExInfoTranslater<HbmExportInfo> translator;
    std::vector<HbmExportInfo> deserializedInfos{allExInfo.size()};
    for (auto i = 0U; i < allExInfo.size(); i++) {
        auto ret = translator.Deserialize(allExInfo[i], deserializedInfos[i]);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
    }

    uint32_t localIdx = UINT32_MAX;
    for (auto i = 0U; i < deserializedInfos.size(); i++) {
        if (deserializedInfos[i].magic != EXPORT_INFO_MAGIC) {
            BM_LOG_ERROR("import info(" << i << ") magic(" << deserializedInfos[i].magic << ") invalid.");
            return BM_INVALID_PARAM;
        }

        if (deserializedInfos[i].rankId == rankIndex_) {
            localIdx = i;
        }
    }
    BM_ASSERT_RETURN(localIdx < deserializedInfos.size(), BM_INVALID_PARAM);

    for (auto i = 0U; i < deserializedInfos.size(); i++) {
        if (deserializedInfos[i].rankId == rankIndex_) {
            continue;
        }

        if (deserializedInfos[i].deviceId != deviceId_) {
            auto ret = RuntimeApi::AclrtDeviceEnablePeerAccess(deserializedInfos[i].deviceId, 0);
            if (ret != 0) {
                BM_LOG_ERROR("enable device access failed:" << ret << " local_device:" << deviceId_
                    << " remote_device:" << (int)deserializedInfos[i].deviceId);
                return BM_DL_FUNCTION_FAILED;
            }
        }

        auto ret = RuntimeApi::RtSetIpcMemorySuperPodPid(deserializedInfos[localIdx].shmName,
                                                         deserializedInfos[i].sdid, &deserializedInfos[i].pid, 1);
        if (ret != 0) {
            BM_LOG_ERROR("enable white list for rank(" << deserializedInfos[i].rankId << ") failed: " << ret 
                << ", local rank = " << rankIndex_ << ", shmName=" << deserializedInfos[localIdx].shmName);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    std::copy(deserializedInfos.begin(), deserializedInfos.end(), std::back_inserter(imports_));
    return BM_OK;
}

Result MemSegmentDevice::Mmap() noexcept
{
    if (imports_.empty()) {
        return BM_OK;
    }

    for (auto &im : imports_) {
        if (im.rankId == rankIndex_) {
            continue;
        }

        auto remoteAddress = globalVirtualAddress_ + options_.size * im.rankId + im.mappingOffset;
        if (mappedMem_.find((uint64_t)remoteAddress) != mappedMem_.end()) {
            BM_LOG_INFO("remote slice on rank(" << im.rankId << ") has maped: " << (void *)remoteAddress);
            continue;
        }

        BM_LOG_DEBUG("remote slice on rank(" << im.rankId << ") should map to: " << (void *)remoteAddress
                                            << ", size = " << im.size);
        auto ret = RuntimeApi::HalGvaOpen((void *)remoteAddress, im.shmName, im.size, 0);
        if (ret != BM_OK) {
            BM_LOG_ERROR("HalGvaOpen memory failed:" << ret);
            return -1;
        }
        mappedMem_.insert((uint64_t)remoteAddress);
    }
    imports_.clear();
    return BM_OK;
}

Result MemSegmentDevice::Start() noexcept
{
    return 0;
}

Result MemSegmentDevice::Stop() noexcept
{
    for (auto va : mappedMem_) {
        (void)RuntimeApi::HalGvaClose((void *)va, 0);
    }
    mappedMem_.clear();

    // TODO: free local slice memory
    return 0;
}

Result MemSegmentDevice::Join(uint32_t rank) noexcept
{
    return 0;
}

Result MemSegmentDevice::Leave(uint32_t rank) noexcept
{
    uint64_t addr = reinterpret_cast<uint64_t>(globalVirtualAddress_) + options_.size * rank;
    auto it = mappedMem_.lower_bound(addr);
    auto st = it;
    while (it != mappedMem_.end() && (*it) < addr + options_.size) {
        (void)RuntimeApi::HalGvaClose((void *)(*it), 0);
        it++;
    }

    if (st != it) {
        mappedMem_.erase(st, it);
    }
    return 0;
}


std::shared_ptr<MemSlice> MemSegmentDevice::GetMemSlice(hybm_mem_slice_t slice) const noexcept
{
    auto index = MemSlice::GetIndexFrom(slice);
    auto pos = slices_.find(index);
    if (pos == slices_.end()) {
        return nullptr;
    }

    auto target = pos->second.slice;
    if (!target->ValidateId(slice)) {
        return nullptr;
    }

    return target;
}

bool MemSegmentDevice::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    if (begin < globalVirtualAddress_) {
        return false;
    }

    if ((const uint8_t *)begin + size >= globalVirtualAddress_ + totalVirtualSize_) {
        return false;
    }

    return true;
}

void MemSegmentDevice::FreeMemory() noexcept {}

int MemSegmentDevice::GetDeviceId(int deviceId) noexcept
{
    if (deviceId < 0) {
        return BM_INVALID_PARAM;
    }

    if (deviceId_ >= 0) {
        if (deviceId == deviceId_) {
            return 0;
        }

        return BM_INVALID_PARAM;
    }

    uint32_t tgid = 0;
    auto ret = RuntimeApi::RtDeviceGetBareTgid(&tgid);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get bare tgid failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    constexpr auto sdidInfo = 26;
    int64_t value = 0;
    ret = RuntimeApi::RtGetDeviceInfo(deviceId, 0, sdidInfo, &value);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get sdid failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    deviceId_ = deviceId;
    pid_ = static_cast<int>(tgid);
    sdid_ = static_cast<uint32_t>(value);
    return BM_OK;
}
}
}
