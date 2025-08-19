/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include "dl_acl_api.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_device_user_mem_seg.h"

namespace ock {
namespace mf {
constexpr uint8_t MAX_DEVICE_COUNT = 16;

MemSegmentDeviceUseMem::MemSegmentDeviceUseMem(const MemSegmentOptions &options, int eid) noexcept
    : MemSegment{options, eid}
{
}

MemSegmentDeviceUseMem::~MemSegmentDeviceUseMem()
{
    if (!memNames_.empty()) {
        for (auto& name: memNames_) {
            DlAclApi::RtIpcDestroyMemoryName(name.c_str());
        }
        BM_LOG_INFO("Finish to destroy memory names.");
    } else {
        BM_LOG_INFO("Sender does not need to destroy memory names.");
    }
    memNames_.clear();
    CloseMemory();
}

Result MemSegmentDeviceUseMem::ValidateOptions() noexcept
{
    return BM_OK;
}

Result MemSegmentDeviceUseMem::ReserveMemorySpace(void **address) noexcept
{
    BM_LOG_ERROR("MemSegmentDeviceUseMem NOT SUPPORT ReserveMemorySpace");
    return BM_NOT_SUPPORTED;
}

Result MemSegmentDeviceUseMem::UnreserveMemorySpace() noexcept
{
    BM_LOG_INFO("un-reserve memory space.");
    return BM_OK;
}

Result MemSegmentDeviceUseMem::AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    BM_LOG_ERROR("MemSegmentDeviceUseMem NOT SUPPORT AllocLocalMemory");
    return BM_NOT_SUPPORTED;
}

Result MemSegmentDeviceUseMem::RegisterMemory(const void *addr, uint64_t size,
                                              std::shared_ptr<MemSlice> &slice) noexcept
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

    for (auto &remoteDev : importedDeviceInfo_) {
        ret = DlAclApi::RtSetIpcMemorySuperPodPid(name, remoteDev.first, (int *)&remoteDev.second.pid, 1);
        if (ret != 0) {
            BM_LOG_ERROR("set shm(" << name << ") for sdid=" << remoteDev.first << " pid=" << remoteDev.second.pid
                                    << " failed: " << ret);
            DlAclApi::RtIpcDestroyMemoryName(name);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    memNames_.emplace_back(name);
    slice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_SVM,
                                    reinterpret_cast<uint64_t>(addr), size);
    registerSlices_.emplace(slice->index_, RegisterSlice{slice, name});
    addressedSlices_.emplace(slice->vAddress_, slice->size_);
    return BM_OK;
}

Result MemSegmentDeviceUseMem::ReleaseSliceMemory(const std::shared_ptr<MemSlice> &slice) noexcept
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

    addressedSlices_.erase(pos->second.slice->vAddress_);
    registerSlices_.erase(pos);
    return BM_OK;
}

Result MemSegmentDeviceUseMem::Export(std::string &exInfo) noexcept
{
    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(GetDeviceInfo(), "get device info failed.");

    HbmExportDeviceInfo info;
    info.sdid = sdid_;
    info.pid = pid_;
    info.deviceId = deviceId_;

    auto ret = LiteralExInfoTranslater<HbmExportDeviceInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        return BM_ERROR;
    }

    BM_LOG_DEBUG("export device info(sdid=" << sdid_ << ", pid=" << pid_ << ", deviceId=" << deviceId_ << ")");
    return BM_OK;
}

Result MemSegmentDeviceUseMem::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
{
    auto pos = registerSlices_.find(slice->index_);
    if (pos == registerSlices_.end()) {
        BM_LOG_ERROR("release slice : " << slice->index_ << " not exist.");
        return BM_INVALID_PARAM;
    }

    HbmExportSliceInfo info;
    info.address = pos->second.slice->vAddress_;
    info.size = pos->second.slice->size_;
    info.deviceId = static_cast<uint32_t>(deviceId_);
    std::copy_n(pos->second.name.c_str(), std::min(pos->second.name.size(), sizeof(info.name) - 1), info.name);

    auto ret = LiteralExInfoTranslater<HbmExportSliceInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        return BM_ERROR;
    }

    BM_LOG_DEBUG("export slice success.");
    return BM_OK;
}

Result MemSegmentDeviceUseMem::GetExportSliceSize(size_t &size) noexcept
{
    size = sizeof(HbmExportSliceInfo);
    return BM_OK;
}

Result MemSegmentDeviceUseMem::Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept
{
    if (allExInfo.empty()) {
        return BM_OK;
    }

    Result ret = BM_ERROR;
    auto index = 0;
    for (auto &info : allExInfo) {
        std::shared_ptr<MemSlice> rms;
        if (info.length() == sizeof(HbmExportDeviceInfo)) {
            ret = ImportDeviceInfo(info);
        } else if (info.length() == sizeof(HbmExportSliceInfo)) {
            ret = ImportSliceInfo(info, rms);
        } else {
            BM_LOG_ERROR("invalid import info size : " << info.length());
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
            for (auto j = 0; j < index; j++) {
                DlAclApi::RtIpcCloseMemory(addresses[j]);
            }
            break;
        }

        void *address = nullptr;
        if (rms != nullptr) {
            address = (void *)(ptrdiff_t)(rms->vAddress_);
        }
        addresses[index++] = address;
    }

    return ret;
}

Result MemSegmentDeviceUseMem::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    BM_LOG_ERROR("MemSegmentDeviceUseMem NOT SUPPORT RemoveImported");
    return BM_NOT_SUPPORTED;
}

Result MemSegmentDeviceUseMem::Mmap() noexcept
{
    BM_LOG_ERROR("MemSegmentDeviceUseMem NOT SUPPORT Mmap");
    return BM_NOT_SUPPORTED;
}

std::shared_ptr<MemSlice> MemSegmentDeviceUseMem::GetMemSlice(hybm_mem_slice_t slice) const noexcept
{
    std::shared_ptr<MemSlice> target;
    auto index = MemSlice::GetIndexFrom(slice);
    auto pos = registerSlices_.find(index);
    if (pos != registerSlices_.end()) {
        target = pos->second.slice;
    } else if ((pos = remoteSlices_.find(index)) != remoteSlices_.end()) {
        target = pos->second.slice;
    } else {
        BM_LOG_ERROR("cannot get slice: " << slice);
        return nullptr;
    }

    if (!target->ValidateId(slice)) {
        return nullptr;
    }

    return target;
}

Result MemSegmentDeviceUseMem::Unmap() noexcept
{
    BM_LOG_ERROR("MemSegmentDeviceUseMem NOT SUPPORT Unmap");
    return BM_NOT_SUPPORTED;
}

bool MemSegmentDeviceUseMem::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    auto address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(begin));
    auto pos = addressedSlices_.lower_bound(address);
    if (pos == addressedSlices_.end()) {
        return false;
    }

    return (pos->first + pos->second >= address + size);
}

Result MemSegmentDeviceUseMem::ImportDeviceInfo(const std::string &info) noexcept
{
    HbmExportDeviceInfo deviceInfo;
    LiteralExInfoTranslater<HbmExportDeviceInfo> translator;
    auto ret = translator.Deserialize(info, deviceInfo);
    if (ret != 0) {
        BM_LOG_ERROR("deserialize device info failed: " << ret);
        return ret;
    }

    if (deviceInfo.deviceId >= MAX_DEVICE_COUNT) {
        BM_LOG_ERROR("Invalid deviceInfo device id: " << deviceInfo.deviceId);
        return BM_ERROR;
    }

    if (deviceInfo.deviceId != deviceId_ && !enablePeerDevices_.test(deviceInfo.deviceId)) {
        ret = DlAclApi::AclrtDeviceEnablePeerAccess(deviceInfo.deviceId, 0);
        if (ret != 0) {
            BM_LOG_ERROR("AclrtDeviceEnablePeerAccess for device: " << deviceInfo.deviceId << " failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
        enablePeerDevices_.set(deviceInfo.deviceId);
        BM_LOG_DEBUG("enable peer access for : " << deviceInfo.deviceId);
    }

    for (auto &it : registerSlices_) {
        ret = DlAclApi::RtSetIpcMemorySuperPodPid(it.second.name.c_str(), deviceInfo.sdid, (int *)&deviceInfo.pid, 1);
        if (ret != 0) {
            BM_LOG_ERROR("RtSetIpcMemorySuperPodPid failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
        BM_LOG_DEBUG("set whitelist for shm(" << it.second.name << ") sdid=" << deviceInfo.sdid
                                              << ", pid=" << deviceInfo.pid);
    }

    importedDeviceInfo_.emplace(deviceInfo.sdid, deviceInfo);
    return BM_OK;
}

Result MemSegmentDeviceUseMem::ImportSliceInfo(const std::string &info, std::shared_ptr<MemSlice> &remoteSlice) noexcept
{
    HbmExportSliceInfo sliceInfo;
    LiteralExInfoTranslater<HbmExportSliceInfo> translator;
    auto ret = translator.Deserialize(info, sliceInfo);
    if (ret != 0) {
        BM_LOG_ERROR("deserialize slice info failed: " << ret);
        return ret;
    }

    if (sliceInfo.deviceId >= MAX_DEVICE_COUNT) {
        BM_LOG_ERROR("Invalid sliceInfo device id: " << sliceInfo.deviceId);
        return BM_ERROR;
    }

    if (sliceInfo.deviceId != static_cast<uint32_t>(deviceId_) && !enablePeerDevices_.test(sliceInfo.deviceId)) {
        ret = DlAclApi::AclrtDeviceEnablePeerAccess(sliceInfo.deviceId, 0);
        if (ret != 0) {
            BM_LOG_ERROR("AclrtDeviceEnablePeerAccess for device: " << sliceInfo.deviceId << " failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
        enablePeerDevices_.set(sliceInfo.deviceId);
        BM_LOG_DEBUG("enable peer access for : " << sliceInfo.deviceId);
    }

    void *address = nullptr;
    ret = DlAclApi::RtIpcOpenMemory(&address, sliceInfo.name);
    if (ret != 0) {
        BM_LOG_ERROR("IpcOpenMemory(" << sliceInfo.name << ") failed:" << ret << ",sdid=" << sdid_ << ",pid=" << pid_);
        return BM_DL_FUNCTION_FAILED;
    }
    registerAddrs_.emplace_back(address);

    remoteSlice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_SVM,
                                            reinterpret_cast<uint64_t>(address), sliceInfo.size);
    remoteSlices_.emplace(remoteSlice->index_, RegisterSlice{remoteSlice, sliceInfo.name});
    importedSliceInfo_.emplace(sliceInfo.name, sliceInfo);
    addressedSlices_.emplace(remoteSlice->vAddress_, remoteSlice->size_);
    return BM_OK;
}

Result MemSegmentDeviceUseMem::GetDeviceInfo() noexcept
{
    if (InitDeviceInfo() != BM_OK) {
        return BM_ERROR;
    }
    return BM_OK;
}

void MemSegmentDeviceUseMem::CloseMemory() noexcept
{
    for (auto& addr: registerAddrs_) {
        if (DlAclApi::RtIpcCloseMemory(addr) != 0) {
            BM_LOG_WARN("Failed to close memory. This may affect future memory registration.");
        }
        addr = nullptr;
    }
    registerAddrs_.clear();
    BM_LOG_INFO("close memory finish.");
}
}
}