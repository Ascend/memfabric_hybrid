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
#include "dl_acl_api.h"
#include "hybm_networks_common.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_va_manager.h"
#include "hybm_dev_user_legacy_segment.h"

namespace ock {
namespace mf {
constexpr uint8_t MAX_DEVICE_COUNT = 16;
HybmDevUserLegacySegment::HybmDevUserLegacySegment(const MemSegmentOptions &options, int eid) noexcept
    : HybmDevLegacySegment{options, eid}
{}

HybmDevUserLegacySegment::~HybmDevUserLegacySegment()
{
    if (!memNames_.empty()) {
        for (auto &name : memNames_) {
            DlAclApi::RtIpcDestroyMemoryName(name.c_str());
        }
        BM_LOG_INFO("Finish to destroy memory names.");
    } else {
        BM_LOG_INFO("Sender does not need to destroy memory names.");
    }
    memNames_.clear();
    CloseMemory();
}

Result HybmDevUserLegacySegment::ValidateOptions() noexcept
{
    return BM_OK;
}

Result HybmDevUserLegacySegment::ReserveMemorySpace(void **address) noexcept
{
    BM_ASSERT_RETURN(address != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(options_.isSecondMapping == true, BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(options_.rankId < options_.rankCnt,
                             "rank(" << options_.rankId << ") but total " << options_.rankCnt, BM_INVALID_PARAM);

    totalVirtualSize_ = options_.rankCnt * options_.maxSize;
    auto gvaInfo = HybmVaManager::GetInstance().AllocReserveGva(options_.rankId, totalVirtualSize_, 0,
                                                                HYBM_MEM_TYPE_DEVICE, options_.isSecondMapping);
    BM_ASSERT_RETURN(gvaInfo.va[HVM_GVA] > 0, BM_ERROR);
    globalVirtualAddress_ = (uint8_t *)reinterpret_cast<void *>(gvaInfo.va[HVM_GVA]);
    lvaBase_ = globalVirtualAddress_ + options_.maxSize * options_.rankId;
    return BM_OK;
}

Result HybmDevUserLegacySegment::AllocLocalMemory(uint64_t size, MemSlicePtr &slice) noexcept
{
    BM_LOG_ERROR("HybmDevUserLegacySegment NOT SUPPORT AllocLocalMemory");
    return BM_NOT_SUPPORTED;
}

Result HybmDevUserLegacySegment::RegisterMemory(const void *addr, uint64_t size, MemSlicePtr &slice) noexcept
{
    if (addr == nullptr || size == 0) {
        BM_LOG_ERROR("input address parameter is invalid.");
        return BM_INVALID_PARAM;
    }

    char name[DEVICE_SHM_NAME_SIZE + 1U]{};
    auto ret = DlAclApi::RtIpcSetMemoryName(addr, size, name, sizeof(name));
    if (ret != 0) {
        BM_LOG_ERROR("set memory name failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    std::unique_lock<std::mutex> uniqueLock{mutex_};
    for (auto &remoteDev : importedDeviceInfo_) {
        if (!CanSdmaReaches(remoteDev.second.superPodId, remoteDev.second.serverId, remoteDev.second.logicDeviceId)) {
            continue;
        }
        ret = DlAclApi::RtSetIpcMemorySuperPodPid(name, remoteDev.second.sdid, (int *)&remoteDev.second.pid, 1);
        if (ret != 0) {
            BM_LOG_ERROR("set shm(" << name << ") for sdid=" << remoteDev.second.sdid << " pid=" << remoteDev.second.pid
                                    << " failed: " << ret);
            DlAclApi::RtIpcDestroyMemoryName(name);
            return BM_DL_FUNCTION_FAILED;
        }
        BM_LOG_INFO("set shm(" << name << ") for sdid=" << remoteDev.second.sdid << " pid=" << remoteDev.second.pid
                               << " success.");
    }

    uint64_t gva = reinterpret_cast<uint64_t>(lvaBase_) + allocatedSize_;
    slice = std::make_shared<MemSlice>(sliceCount_++, HYBM_MEM_TYPE_DEVICE, MEM_PT_TYPE_SVM,
                                       gva, reinterpret_cast<uint64_t>(addr), size);
    ret = HybmVaManager::GetInstance().AddVaInfo(
        {gva, slice->vAddress_, slice->vAddress_, size, HYBM_MEM_TYPE_DEVICE}, options_.rankId);
    if (ret != 0) {
        BM_LOG_ERROR("AddVaInfo failed, size: " << size << " ret: " << ret);
        DlAclApi::RtIpcDestroyMemoryName(name);
        slice = nullptr;
        return ret;
    }

    memNames_.emplace_back(name);
    registerSlices_.emplace(slice->index_, RegisterSlice{slice, name});
    allocatedSize_ += size;
    uniqueLock.unlock();
    return BM_OK;
}

Result HybmDevUserLegacySegment::ReleaseSliceMemory(const MemSlicePtr &slice) noexcept
{
    auto pos = registerSlices_.find(slice->index_);
    if (pos == registerSlices_.end()) {
        BM_LOG_ERROR("release slice : " << slice->index_ << " not exist.");
        return BM_INVALID_PARAM;
    }

    auto ret = DlAclApi::RtIpcDestroyMemoryName(pos->second.name.c_str());
    if (ret != 0) {
        BM_LOG_ERROR("destroy memory name failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    registerSlices_.erase(pos);
    return BM_OK;
}

Result HybmDevUserLegacySegment::Export(std::string &exInfo) noexcept
{
    HbmExportDeviceInfo info;
    info.logicDeviceId = logicDeviceId_;
    info.rankId = options_.rankId;
    info.pid = HybmDevLegacySegment::pid_;
    HybmDevLegacySegment::GetDeviceInfo(info.sdid, info.serverId, info.superPodId);
    auto ret = LiteralExInfoTranslater<HbmExportDeviceInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        return BM_ERROR;
    }

    BM_LOG_DEBUG("export device info(sdid=" << sdid_ << ", pid=" << pid_ << ", deviceId=" << logicDeviceId_ << ")");
    return BM_OK;
}

Result HybmDevUserLegacySegment::Export(const MemSlicePtr &slice, std::string &exInfo) noexcept
{
    auto pos = registerSlices_.find(slice->index_);
    if (pos == registerSlices_.end()) {
        BM_LOG_ERROR("release slice : " << slice->index_ << " not exist.");
        return BM_INVALID_PARAM;
    }

    uint32_t sdId;
    HbmExportSliceInfo info;
    info.gva = pos->second.slice->gva_;
    info.address = pos->second.slice->vAddress_;
    info.size = pos->second.slice->size_;
    info.logicDeviceId = static_cast<uint32_t>(logicDeviceId_);
    info.rankId = options_.rankId;
    HybmDevLegacySegment::GetDeviceInfo(sdId, info.serverId, info.superPodId);
    std::copy_n(pos->second.name.c_str(), std::min(pos->second.name.size(), sizeof(info.name) - 1), info.name);
    auto ret = LiteralExInfoTranslater<HbmExportSliceInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        return BM_ERROR;
    }

    BM_LOG_DEBUG("export slice success.");
    return BM_OK;
}

Result HybmDevUserLegacySegment::GetExportSliceSize(size_t &size) noexcept
{
    size = sizeof(HbmExportSliceInfo);
    return BM_OK;
}

void HybmDevUserLegacySegment::RollbackIpcMemory(void *addresses[], uint32_t count)
{
    for (uint32_t j = 0; j < count; j++) {
        if (addresses[j] != nullptr) {
            DlAclApi::RtIpcCloseMemory(addresses[j]);
        }
    }
}

Result HybmDevUserLegacySegment::Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept
{
    if (allExInfo.empty()) {
        return BM_OK;
    }

    Result ret = BM_ERROR;
    uint32_t index = 0U;
    for (auto &info : allExInfo) {
        MemSlicePtr rms;
        auto magic = *reinterpret_cast<const uint64_t *>(info.data());
        if (magic == ENTITY_EXPORT_INFO_MAGIC) {
            ret = ImportDeviceInfo(info);
        } else if (magic == HBM_SLICE_EXPORT_INFO_MAGIC) {
            ret = ImportSliceInfo(info, rms);
        } else {
            BM_LOG_ERROR("invalid import magic : " << magic);
            ret = BM_INVALID_PARAM;
        }
        if (addresses == nullptr) {
            if (ret != BM_OK) {
                break;
            }
            // kv trans addresses is null need continue
            continue;
        }
        if (ret != BM_OK) {
            // rollback
            RollbackIpcMemory(addresses, index);
            break;
        }

        void *address = nullptr;
        if (rms != nullptr) {
            address = (void *)(ptrdiff_t)(rms->gva_);
        }
        addresses[index++] = address;
    }

    return ret;
}

Result HybmDevUserLegacySegment::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    std::unique_lock<std::mutex> uniqueLock{mutex_};
    for (auto rankId : ranks) {
        importedDeviceInfo_.erase(rankId);
        RemoveSliceInfo(rankId);
    }
    uniqueLock.unlock();
    return BM_OK;
}

void HybmDevUserLegacySegment::RemoveSliceInfo(const uint32_t rankId) noexcept
{
    // Clear Imported SliceInfo
    auto it = rankToRemoteSlices_.find(rankId);
    if (it == rankToRemoteSlices_.end()) {
        return;
    }
    auto &remoteSliceVec = it->second;
    for (auto &remoteSlice : remoteSliceVec) {
        registerAddrs_.erase(reinterpret_cast<void *>(static_cast<ptrdiff_t>(remoteSlice->vAddress_)));
        HybmVaManager::GetInstance().RemoveOneVaInfo(remoteSlice->gva_);
        auto rIt = remoteSlices_.find(remoteSlice->index_);
        if (rIt == remoteSlices_.end()) {
            continue;
        }
        auto sIt = importedSliceInfo_.find(rIt->second.name);
        if (sIt == importedSliceInfo_.end()) {
            remoteSlices_.erase(remoteSlice->index_);
            continue;
        }
        auto &sliceInfo = sIt->second;
        if ((options_.dataOpType & HYBM_DOP_TYPE_SDMA) &&
            CanSdmaReaches(sliceInfo.superPodId, sliceInfo.serverId, sliceInfo.logicDeviceId)) {
            void *address = reinterpret_cast<void *>(static_cast<ptrdiff_t>(remoteSlice->vAddress_ << 16 >> 16));
            BM_LOG_INFO("RtIpcCloseMemory start address="
                        << address
                        << ", vAddress_ = " << reinterpret_cast<void *>(static_cast<ptrdiff_t>(remoteSlice->vAddress_))
                        << ", deviceId=" << logicDeviceId_ << ", sliceInfo.logicDeviceId=" << sliceInfo.logicDeviceId
                        << ", sliceInfo.rankId=" << sliceInfo.rankId);
            auto ret = DlAclApi::RtIpcCloseMemory(address);
            if (ret != 0) {
                BM_LOG_WARN("Failed to close memory, address="
                            << address << ", vAddress_"
                            << reinterpret_cast<void *>(static_cast<ptrdiff_t>(remoteSlice->vAddress_)) << ", deviceId="
                            << logicDeviceId_ << ", sliceInfo.logicDeviceId=" << sliceInfo.logicDeviceId
                            << ", sliceInfo.rankId=" << sliceInfo.rankId << ", ret:" << ret
                            << ", This may affect future memory registration.");
            }
        }
        BM_LOG_INFO("RemoveSliceInfo, rankId=" << rankId << ", remoteSlice->index_=" << remoteSlice->index_
                                               << ",slice name " << rIt->second.name);
        importedSliceInfo_.erase(rIt->second.name);
        remoteSlices_.erase(remoteSlice->index_);
    }
    rankToRemoteSlices_.erase(rankId);
}

Result HybmDevUserLegacySegment::Mmap() noexcept
{
    BM_LOG_ERROR("HybmDevUserLegacySegment NOT SUPPORT Mmap");
    return BM_NOT_SUPPORTED;
}

MemSlicePtr HybmDevUserLegacySegment::GetMemSlice(hybm_mem_slice_t slice, bool quiet) const noexcept
{
    MemSlicePtr target;
    auto index = MemSlice::GetIndexFrom(slice);
    auto pos = registerSlices_.find(index);
    if (pos != registerSlices_.end()) {
        target = pos->second.slice;
    } else if ((pos = remoteSlices_.find(index)) != remoteSlices_.end()) {
        target = pos->second.slice;
    } else {
        if (quiet) {
            BM_LOG_DEBUG("cannot get slice: " << slice);
        } else {
            BM_LOG_ERROR("cannot get slice: " << slice);
        }
        return nullptr;
    }

    if (!target->ValidateId(slice)) {
        return nullptr;
    }

    return target;
}

Result HybmDevUserLegacySegment::Unmap() noexcept
{
    BM_LOG_ERROR("HybmDevUserLegacySegment NOT SUPPORT Unmap");
    return BM_NOT_SUPPORTED;
}

Result HybmDevUserLegacySegment::ImportDeviceInfo(const std::string &info) noexcept
{
    HbmExportDeviceInfo deviceInfo;
    LiteralExInfoTranslater<HbmExportDeviceInfo> translator;
    auto ret = translator.Deserialize(info, deviceInfo);
    if (ret != 0) {
        BM_LOG_ERROR("deserialize device info failed: " << ret);
        return ret;
    }

    if (deviceInfo.logicDeviceId >= MAX_DEVICE_COUNT) {
        BM_LOG_ERROR("Invalid deviceInfo device id: " << deviceInfo.logicDeviceId);
        return BM_ERROR;
    }

    if (deviceInfo.logicDeviceId != logicDeviceId_ && !enablePeerDevices_.test(deviceInfo.logicDeviceId)) {
        ret = DlAclApi::RtEnableP2P(deviceId_, deviceInfo.logicDeviceId, 0);
        if (ret != 0) {
            BM_LOG_ERROR("enable device access failed:" << ret << " local_device:" << deviceId_
                                                        << " logic_device:" << logicDeviceId_
                                                        << " remote_logic_device:" << deviceInfo.logicDeviceId);
            return BM_DL_FUNCTION_FAILED;
        }
        enablePeerDevices_.set(deviceInfo.logicDeviceId);
        BM_LOG_DEBUG("enable peer access for : " << deviceInfo.logicDeviceId);
    }
    std::unique_lock<std::mutex> uniqueLock{mutex_};
    for (auto &it : registerSlices_) {
        ret = DlAclApi::RtSetIpcMemorySuperPodPid(it.second.name.c_str(), deviceInfo.sdid, (int *)&deviceInfo.pid, 1);
        if (ret != 0) {
            BM_LOG_ERROR("RtSetIpcMemorySuperPodPid failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
        BM_LOG_DEBUG("set whitelist for shm(" << it.second.name << ") sdid=" << deviceInfo.sdid
                                              << ", pid=" << deviceInfo.pid);
    }

    importedDeviceInfo_.emplace(deviceInfo.rankId, deviceInfo);
    uniqueLock.unlock();
    return BM_OK;
}

Result HybmDevUserLegacySegment::ImportSliceInfo(const std::string &info, MemSlicePtr &remoteSlice) noexcept
{
    HbmExportSliceInfo sliceInfo;
    LiteralExInfoTranslater<HbmExportSliceInfo> translator;
    auto ret = translator.Deserialize(info, sliceInfo);
    if (ret != 0) {
        BM_LOG_ERROR("deserialize slice info failed: " << ret);
        return ret;
    }

    if (sliceInfo.logicDeviceId >= MAX_DEVICE_COUNT) {
        BM_LOG_ERROR("Invalid sliceInfo device id: " << sliceInfo.logicDeviceId);
        return BM_ERROR;
    }

    void *address = nullptr;
    std::unique_lock<std::mutex> uniqueLock{mutex_};
    if ((options_.dataOpType & HYBM_DOP_TYPE_SDMA) &&
        CanSdmaReaches(sliceInfo.superPodId, sliceInfo.serverId, sliceInfo.logicDeviceId)) {
        if (sliceInfo.logicDeviceId != static_cast<uint32_t>(logicDeviceId_) &&
            !enablePeerDevices_.test(sliceInfo.logicDeviceId)) {
            ret = DlAclApi::RtEnableP2P(deviceId_, sliceInfo.logicDeviceId, 0);
            if (ret != 0) {
                BM_LOG_ERROR("AclrtDeviceEnablePeerAccess for device: " << sliceInfo.logicDeviceId
                                                                        << " failed: " << ret);
                return BM_DL_FUNCTION_FAILED;
            }
            enablePeerDevices_.set(sliceInfo.logicDeviceId);
            BM_LOG_DEBUG("enable peer access for : " << sliceInfo.logicDeviceId);
        }

        ret = DlAclApi::RtIpcOpenMemory(&address, sliceInfo.name);
        if (ret != 0) {
            BM_LOG_ERROR("IpcOpenMemory(" << sliceInfo.name << ") failed:" << ret << ",sdid=" << sdid_
                                          << ", pid=" << pid_ << ", deviceId=" << logicDeviceId_
                                          << ", sliceInfo.logicDeviceId=" << sliceInfo.logicDeviceId);
            return BM_DL_FUNCTION_FAILED;
        }
        BM_LOG_INFO("IpcOpenMemory(" << sliceInfo.name << ") success, sdid=" << sdid_ << ", pid=" << pid_
                                     << ", deviceId=" << logicDeviceId_
                                     << ", sliceInfo.logicDeviceId=" << sliceInfo.logicDeviceId);
        registerAddrs_.insert(address);
    } else if (options_.dataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        address = nullptr;
    }

    remoteSlice = std::make_shared<MemSlice>(sliceCount_++, HYBM_MEM_TYPE_DEVICE, MEM_PT_TYPE_SVM, sliceInfo.gva,
                                             reinterpret_cast<uint64_t>(address), sliceInfo.size);
    rankToRemoteSlices_[sliceInfo.rankId].push_back(remoteSlice);
    remoteSlices_.emplace(remoteSlice->index_, RegisterSlice{remoteSlice, sliceInfo.name});
    importedSliceInfo_.emplace(sliceInfo.name, sliceInfo);
    uniqueLock.unlock();
    auto memType = HYBM_MEM_TYPE_DEVICE;
    ret = HybmVaManager::GetInstance().AddVaInfoFromExternal({remoteSlice->gva_, remoteSlice->vAddress_, 0,
        remoteSlice->size_, memType}, options_.rankId, sliceInfo.rankId);
    BM_ASSERT_RETURN(ret == BM_OK, ret);
    return BM_OK;
}

void HybmDevUserLegacySegment::CloseMemory() noexcept
{
    for (auto &addr : registerAddrs_) {
        if (DlAclApi::RtIpcCloseMemory(addr) != 0) {
            BM_LOG_WARN("Failed to close memory. This may affect future memory registration.");
        }
    }
    for (auto &it : registerSlices_) {
        HybmVaManager::GetInstance().RemoveOneVaInfo(it.second.slice->gva_);
    }

    registerAddrs_.clear();
    BM_LOG_INFO("close memory finish.");
}

bool HybmDevUserLegacySegment::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    if (begin < globalVirtualAddress_) {
        return false;
    }

    if (reinterpret_cast<const uint8_t *>(begin) + size > globalVirtualAddress_ + totalVirtualSize_) {
        return false;
    }

    return true;
}

bool HybmDevUserLegacySegment::GetRankIdByAddr(const void *addr, uint64_t size, uint32_t &rankId) const noexcept
{
    if (!MemoryInRange(addr, size)) {
        rankId = options_.rankId;
        return false;
    } else {
        uint64_t offset = static_cast<const uint8_t *>(addr) - static_cast<const uint8_t *>(globalVirtualAddress_);
        rankId = offset / options_.maxSize;
        return true;
    }
}

bool HybmDevUserLegacySegment::CheckSdmaReaches(uint32_t rankId) const noexcept
{
    auto pos = importedDeviceInfo_.find(rankId);
    if (pos == importedDeviceInfo_.end()) {
        return false;
    }

    uint32_t sdId;
    uint32_t serverId;
    uint32_t superPodId;
    HybmDevLegacySegment::GetDeviceInfo(sdId, serverId, superPodId);

    if (pos->second.serverId == serverId) {
        return true;
    }

    if (pos->second.superPodId == invalidSuperPodId || superPodId == invalidSuperPodId) {
        return false;
    }

    return pos->second.superPodId == superPodId;
}

} // namespace mf
} // namespace ock