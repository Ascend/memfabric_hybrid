/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_entity_compose.h"

#include <algorithm>

#include "hybm_common_include.h"
#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "hybm_device_mem_segment.h"
#include "hybm_data_operator_sdma.h"
#include "hybm_data_operator_rdma.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_default_transport_manager.h"

namespace ock {
namespace mf {

struct ComposeTransportExtraInfo {
    uint32_t rankId;
    uint16_t hostInfoLen;
    uint16_t deviceInfoLen;
    transport::TransportMemoryKey hostMemKey;
    transport::TransportMemoryKey deviceMemKey;
};

struct ComposeEntityExportInfo {
    uint64_t magic{0};
    uint64_t version{0};
    uint32_t rankId{0};
    char nic[64]{};
};

int32_t HybmEntityCompose::Initialize(const hybm_options *options) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(!initialized, "Entity is already initialized.", BM_OK);
    BM_ASSERT_LOG_AND_RETURN(id_ >= 0 && (uint32_t)(id_) < HYBM_ENTITY_NUM_MAX, "Input entity id is invalid, input: "
                             << id_ << " must be less than: " << HYBM_ENTITY_NUM_MAX, BM_INVALID_PARAM);

    auto ret = CheckOptions(options);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to check options ret: " << ret);
        return ret;
    }
    options_ = *options;

    ret = DlAclApi::AclrtCreateStream(&stream_);
    if (ret != 0) {
        BM_LOG_ERROR("create stream failed: " << ret);
        return ret;
    }

    ret = InitSegment();
    if (ret != 0) {
        BM_LOG_ERROR("Failed to init segment ret: " << ret);
        CleanEntitySource();
        return ret;
    }

    ret = InitTransManager();
    if (ret != 0) {
        BM_LOG_ERROR("Failed to init transport manager ret: " << ret);
        CleanEntitySource();
        return ret;
    }

    ret = InitDataOperator();
    if (ret != 0) {
        BM_LOG_ERROR("Failed to init data operator ret: " << ret);
        CleanEntitySource();
        return ret;
    }

    initialized = true;
    BM_LOG_INFO("Success to init compose entity, id: " << id_ << " hbm size: " << options->deviceVASpace
                << " dram size: " << options->hostVASpace);
    return BM_OK;
}

void HybmEntityCompose::CleanEntitySource()
{
    deviceSegment_.reset();
    hostSegment_.reset();
    dataOperator_.reset();
    transportManager_.reset();
    importedMemories_.clear();
    DlAclApi::AclrtDestroyStream(stream_);
    stream_ = nullptr;
    hostGva_ = nullptr;
    deviceGva_ = nullptr;
    transportPrepared_ = false;
}

void HybmEntityCompose::UnInitialize() noexcept
{
    if (!initialized) {
        return;
    }

    CleanEntitySource();
    initialized = false;
}

int32_t HybmEntityCompose::ReserveMemorySpace(void **reservedMem) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);

    auto ret = deviceSegment_->ReserveMemorySpace(&deviceGva_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to reserve device segment, ret: " << ret);
        return ret;
    }

    ret = hostSegment_->ReserveMemorySpace(&hostGva_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to reserve host segment, ret: " << ret);
        return ret;
    }
    *reservedMem = hostGva_;
    return BM_OK;
}

int32_t HybmEntityCompose::UnReserveMemorySpace() noexcept
{
    return BM_OK;
}

int32_t HybmEntityCompose::AllocLocalMemory(uint64_t size, uint32_t flags, hybm_mem_slice_t &slice) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);

    auto ret = deviceSegment_->AllocLocalMemory(options_.deviceVASpace, deviceSlice_);
    if (ret != 0) {
        BM_LOG_ERROR("segment allocate slice with size: " << options_.deviceVASpace << " failed: " << ret);
        return ret;
    }
    transport::TransportMemoryRegion info;
    info.size = deviceSlice_->size_;
    info.addr = deviceSlice_->vAddress_;
    info.access = transport::REG_MR_ACCESS_FLAG_BOTH_READ_WRITE;
    info.flags = transport::REG_MR_FLAG_HBM;
    ret = transportManager_->RegisterMemoryRegion(info);
    if (ret != 0) {
        FreeLocalMemory(deviceSlice_->ConvertToId(), 0);
        BM_LOG_ERROR("register memory region allocate failed: " << ret << ", info: " << info);
        return ret;
    }

    ret = hostSegment_->AllocLocalMemory(options_.hostVASpace, hostSlice_);
    if (ret != 0) {
        FreeLocalMemory(deviceSlice_->ConvertToId(), 0);
        BM_LOG_ERROR("segment allocate slice with size: " << options_.hostVASpace << " failed: " << ret);
        return ret;
    }
    info.size = hostSlice_->size_;
    info.addr = hostSlice_->vAddress_;
    info.access = transport::REG_MR_ACCESS_FLAG_BOTH_READ_WRITE;
    info.flags = transport::REG_MR_FLAG_DRAM;
    ret = transportManager_->RegisterMemoryRegion(info);
    if (ret != 0) {
        FreeLocalMemory(deviceSlice_->ConvertToId(), 0);
        FreeLocalMemory(hostSlice_->ConvertToId(), 0);
        BM_LOG_ERROR("register memory region allocate failed: " << ret << ", info: " << info);
        return ret;
    }
    return UpdateHybmDeviceInfo(0);
}

void HybmEntityCompose::FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept {}

int32_t HybmEntityCompose::ExportExchangeInfo(hybm_exchange_info &desc, uint32_t flags) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);
    std::string info;
    ComposeEntityExportInfo exportInfo;
    exportInfo.magic = EXPORT_INFO_MAGIC;
    exportInfo.version = EXPORT_INFO_VERSION;
    exportInfo.rankId = options_.rankId;
    auto &nic = transportManager_->GetNic();
    if (nic.size() >= sizeof(exportInfo.nic)) {
        BM_LOG_ERROR("transport get nic(" << nic << ") too long.");
        return BM_ERROR;
    }
    std::copy_n(nic.c_str(), nic.size(), exportInfo.nic);
    auto ret = LiteralExInfoTranslater<ComposeEntityExportInfo>{}.Serialize(exportInfo, info);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        return BM_ERROR;
    }

    if (info.size() > sizeof(desc.desc)) {
        BM_LOG_ERROR("export to string wrong size: " << info.size() << ", the correct size is: " << sizeof(desc.desc));
        return BM_ERROR;
    }

    std::copy_n(info.data(), sizeof(desc.desc), desc.desc);
    desc.descLen = info.size();
    return BM_OK;
}

int32_t HybmEntityCompose::ExportExchangeInfo(hybm_mem_slice_t slice, hybm_exchange_info &desc, uint32_t flags) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);

    std::string deviceInfo;
    auto ret = deviceSegment_->Export(deviceSlice_, deviceInfo);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to export to string ret: " << ret);
        return ret;
    }
    std::string hostInfo;
    ret = hostSegment_->Export(hostSlice_, hostInfo);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to export to string ret: " << ret);
        return ret;
    }
    ComposeTransportExtraInfo transportInfo;
    transportInfo.rankId = options_.rankId;
    transportInfo.deviceInfoLen = deviceInfo.size();
    transportInfo.hostInfoLen = hostInfo.size();
    ret = transportManager_->QueryMemoryKey(deviceSlice_->vAddress_, transportInfo.deviceMemKey);
    if (ret != BM_OK) {
        BM_LOG_ERROR("query memory key for device slice: " << deviceSlice_->index_ << " failed: " << ret);
        return ret;
    }
    ret = transportManager_->QueryMemoryKey(hostSlice_->vAddress_, transportInfo.hostMemKey);
    if (ret != BM_OK) {
        BM_LOG_ERROR("query memory key for host slice: " << hostSlice_->index_ << " failed: " << ret);
        return ret;
    }

    if (deviceInfo.size() + hostInfo.size() + sizeof(transportInfo) > sizeof(desc.desc)) {
        BM_LOG_ERROR("Size is to long hostInfo: " << hostInfo.size() << " deviceInfo: " << deviceInfo.size()
                     << " extraInfo: " << sizeof(transportInfo));
        return BM_ERROR;
    }
    uint8_t *offset = desc.desc;
    std::copy_n(deviceInfo.data(), deviceInfo.size(), offset);
    offset += deviceInfo.size();
    std::copy_n(hostInfo.data(), hostInfo.size(), offset);
    offset += hostInfo.size();
    std::copy_n(reinterpret_cast<uint8_t *>(&transportInfo), sizeof(transportInfo), offset);
    desc.descLen = deviceInfo.size() + hostInfo.size() + sizeof(transportInfo);
    return BM_OK;
}

int32_t HybmEntityCompose::ImportExchangeInfo(const hybm_exchange_info *desc, uint32_t count, uint32_t flags) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);
    const ComposeTransportExtraInfo *extraInfo = nullptr;
    std::unordered_map<uint32_t, std::vector<transport::TransportMemoryKey>> transportInfo;
    std::vector<std::string> deviceInfos;
    std::vector<std::string> hostInfos;
    for (auto i = 0U; i < count; i++) {
        if (desc[i].descLen <= sizeof(ComposeTransportExtraInfo)) {
            BM_LOG_ERROR("import slice infos[" << i << "] size too short: " << desc[i].descLen);
            return BM_INVALID_PARAM;
        }

        extraInfo = (const ComposeTransportExtraInfo *)(const void *)
                    (desc[i].desc + (desc[i].descLen - sizeof(ComposeTransportExtraInfo)));
        transportInfo[extraInfo->rankId].emplace_back(extraInfo->deviceMemKey);
        transportInfo[extraInfo->rankId].emplace_back(extraInfo->hostMemKey);
        deviceInfos.emplace_back((const char *)desc[i].desc, extraInfo->deviceInfoLen);
        hostInfos.emplace_back((const char *)desc[i].desc + extraInfo->deviceInfoLen, extraInfo->hostInfoLen);
    }
    auto ret = deviceSegment_->Import(deviceInfos);
    if (ret != BM_OK) {
        BM_LOG_ERROR("segment import infos failed: " << ret);
        return ret;
    }
    ret = hostSegment_->Import(hostInfos);
    if (ret != BM_OK) {
        BM_LOG_ERROR("segment import infos failed: " << ret);
        return ret;
    }

    for (auto &e : transportInfo) {
        importedMemories_[e.first].insert(importedMemories_[e.first].end(), e.second.begin(), e.second.end());
    }
    return BM_OK;
}

int32_t HybmEntityCompose::ImportEntityExchangeInfo(const hybm_exchange_info *desc, uint32_t count,
                                                    uint32_t flags) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);
    LiteralExInfoTranslater<ComposeEntityExportInfo> translator;
    std::vector<ComposeEntityExportInfo> deserializedInfos(count);
    for (auto i = 0U; i < count; i++) {
        auto ret =
                translator.Deserialize(std::string((const char *)desc[i].desc, desc[i].descLen), deserializedInfos[i]);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
    }

    transport::HybmTransPrepareOptions prepareOptions;
    for (auto i = 0U; i < deserializedInfos.size(); i++) {
        auto &info = deserializedInfos[i];
        if (deserializedInfos[i].magic != EXPORT_INFO_MAGIC) {
            BM_LOG_ERROR("import info(" << i << ") magic(" << info.magic << ") invalid.");
            return BM_INVALID_PARAM;
        }

        transport::TransportRankPrepareInfo prepareInfo;
        prepareInfo.nic = info.nic;
        auto pos = importedMemories_.find(info.rankId);
        if (pos == importedMemories_.end()) {
            BM_LOG_ERROR("not import memory for rankId: " << info.rankId);
            return BM_INVALID_PARAM;
        }

        prepareInfo.memKeys = pos->second;
        prepareOptions.options.emplace(info.rankId, prepareInfo);
    }

    int ret;
    if (transportPrepared_) {
        ret = transportManager_->UpdateRankOptions(prepareOptions);
    } else {
        ret = transportManager_->Prepare(prepareOptions);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to prepare transport connect data, ret: " << ret);
            return ret;
        }
        ret = transportManager_->Connect();
    }

    if (ret != BM_OK) {
        BM_LOG_ERROR("Connect Transport: " << ret);
        return ret;
    }

    transportPrepared_ = true;
    return BM_OK;
}

int32_t HybmEntityCompose::SetExtraContext(const void *context, uint32_t size) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);
    BM_ASSERT_LOG_AND_RETURN(context != nullptr, "Invalid param, context is null", BM_INVALID_PARAM);
    if (size > HYBM_DEVICE_USER_CONTEXT_PRE_SIZE) {
        BM_LOG_ERROR("set extra context failed, context size is too large: " << size << " limit: "
                                                                             << HYBM_DEVICE_USER_CONTEXT_PRE_SIZE);
        return BM_INVALID_PARAM;
    }

    uint64_t addr = HYBM_DEVICE_USER_CONTEXT_ADDR + id_ * HYBM_DEVICE_USER_CONTEXT_PRE_SIZE;
    auto ret = DlAclApi::AclrtMemcpy((void *)addr, HYBM_DEVICE_USER_CONTEXT_PRE_SIZE, context, size,
                                     ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("memcpy user context failed, ret: " << ret);
        return BM_ERROR;
    }

    return UpdateHybmDeviceInfo(size);
}

void HybmEntityCompose::Unmap() noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.",);

    deviceSegment_->Unmap();
    hostSegment_->Unmap();
}

int32_t HybmEntityCompose::Mmap() noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);

    auto ret = deviceSegment_->Mmap();
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to mmap device segment, ret: " << ret);
        return ret;
    }
    ret = hostSegment_->Mmap();
    if (ret != BM_OK) {
        deviceSegment_->Unmap();
        BM_LOG_ERROR("Failed to mmap host segment, ret: " << ret);
        return ret;
    }
    return BM_OK;
}

int32_t HybmEntityCompose::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);
    auto ret = deviceSegment_->RemoveImported(ranks);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to remove device segment, ret: " << ret);
        return ret;
    }
    ret = hostSegment_->RemoveImported(ranks);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to remove host segment, ret: " << ret);
        return ret;
    }
    return BM_OK;
}

int32_t HybmEntityCompose::CopyData(const void *src, void *dest, uint64_t length, hybm_data_copy_direction direction,
                                    void *stream, uint32_t flags) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);
    std::shared_ptr<MemSegment> srcSegment;
    std::shared_ptr<MemSegment> destSegment;
    GetSegmentFromDirect(direction, srcSegment, destSegment);
    ExtOptions options{};
    options.flags = flags;
    options.stream = stream;
    options.srcRankId = srcSegment ? srcSegment->GetRankIdByAddr(src, length) : UINT32_MAX;
    options.destRankId = destSegment ? destSegment->GetRankIdByAddr(dest, length) : UINT32_MAX;
    return dataOperator_->DataCopy(src, dest, length, direction, options);
}

int32_t HybmEntityCompose::CopyData2d(const void *src, uint64_t spitch, void *dest, uint64_t dpitch, uint64_t width,
                                      uint64_t height, hybm_data_copy_direction direction, void *stream,
                                      uint32_t flags) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);
    // 获取 srcRankId和destRankId
    std::shared_ptr<MemSegment> srcSegment;
    std::shared_ptr<MemSegment> destSegment;
    GetSegmentFromDirect(direction, srcSegment, destSegment);
    ExtOptions options{};
    options.flags = flags;
    options.stream = stream;
    options.srcRankId = srcSegment ? srcSegment->GetRankIdByAddr(src, spitch * height) : UINT32_MAX;
    options.destRankId = destSegment ? destSegment->GetRankIdByAddr(dest, dpitch * height) : UINT32_MAX;
    return dataOperator_->DataCopy2d(src, spitch, dest, dpitch, width, height, direction, options);
}

bool HybmEntityCompose::CheckAddressInEntity(const void *ptr, uint64_t length) const noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "Entity is not initialized.", BM_NOT_INITIALIZED);

    return deviceSegment_->MemoryInRange(ptr, length) | hostSegment_->MemoryInRange(ptr, length);
}

int HybmEntityCompose::CheckOptions(const hybm_options *options) noexcept
{
    if (options == nullptr) {
        BM_LOG_ERROR("initialize with nullptr.");
        return BM_INVALID_PARAM;
    }

    if (options->rankId >= options->rankCount) {
        BM_LOG_ERROR("local rank id: " << options->rankId << " invalid, total is " << options->rankCount);
        return BM_INVALID_PARAM;
    }

    if (options->hostVASpace == 0UL || (options->hostVASpace % DEVICE_LARGE_PAGE_SIZE) != 0UL ||
        options->deviceVASpace == 0UL || (options->deviceVASpace % DEVICE_LARGE_PAGE_SIZE) != 0UL) {
        BM_LOG_ERROR("invalid local memory size(D:" << options->deviceVASpace << ",H:" << options->hostVASpace <<
            ") should be times of " << DEVICE_LARGE_PAGE_SIZE << " and large than 0");
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

int HybmEntityCompose::UpdateHybmDeviceInfo(uint32_t extCtxSize) noexcept
{
    HybmDeviceMeta info;
    auto addr = HYBM_DEVICE_META_ADDR + HYBM_DEVICE_GLOBAL_META_SIZE + id_ * HYBM_DEVICE_PRE_META_SIZE;

    SetHybmDeviceInfo(info);
    info.extraContextSize = extCtxSize;
    auto ret = DlAclApi::AclrtMemcpy((void *)addr, DEVICE_LARGE_PAGE_SIZE, &info, sizeof(HybmDeviceMeta),
                                     ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("update hybm info memory failed, ret: " << ret);
        return BM_ERROR;
    }
    return BM_OK;
}

void HybmEntityCompose::SetHybmDeviceInfo(HybmDeviceMeta &info)
{
    info.entityId = id_;
    info.rankId = options_.rankId;
    info.rankSize = options_.rankCount;
    info.symmetricSize = options_.singleRankVASpace;
    info.extraContextSize = 0;
    if (transportManager_ != nullptr) {
        info.qpInfoAddress = (uint64_t)(ptrdiff_t)transportManager_->GetQpInfo();
    } else {
        info.qpInfoAddress = 0UL;
    }
}

Result HybmEntityCompose::InitSegment()
{
    BM_ASSERT_LOG_AND_RETURN(options_.deviceVASpace != 0, "Failed to init segment hbm space is 0.", BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(options_.hostVASpace != 0, "Failed to init segment dram space is 0.", BM_INVALID_PARAM);

    auto ret = InitDramSegment();
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to init dram segment, ret: " << ret);
        return ret;
    }

    ret = InitHbmSegment();
    if (ret != BM_OK) {
        hostSegment_.reset();
        BM_LOG_ERROR("Failed to init dram segment, ret: " << ret);
        return ret;
    }
    return BM_OK;
}

Result HybmEntityCompose::InitHbmSegment()
{
    MemSegmentOptions segmentOptions;
    segmentOptions.size = options_.deviceVASpace;
    segmentOptions.segId = HybmGetInitDeviceId();
    segmentOptions.segType = HYBM_MST_HBM;
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;
    deviceSegment_ = MemSegment::Create(segmentOptions, id_);
    if (deviceSegment_ == nullptr) {
        BM_LOG_ERROR("Failed to create hbm segment");
        return BM_ERROR;
    }

    return MemSegmentDevice::GetDeviceId(HybmGetInitDeviceId());
}

Result HybmEntityCompose::InitDramSegment()
{
    if (options_.singleRankVASpace == 0) {
        BM_LOG_INFO("Dram rank space is zero.");
        return BM_OK;
    }

    MemSegmentOptions segmentOptions;
    segmentOptions.size = options_.hostVASpace;
    segmentOptions.segId = HybmGetInitDeviceId();
    segmentOptions.segType = HYBM_MST_DRAM;
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;
    hostSegment_ = MemSegment::Create(segmentOptions, id_);
    if (hostSegment_ == nullptr) {
        BM_LOG_ERROR("Failed to create dram segment");
        return BM_ERROR;
    }

    return MemSegmentDevice::GetDeviceId(HybmGetInitDeviceId());
}

Result HybmEntityCompose::InitTransManager()
{
    if (options_.bmDataOpType == HYBM_DOP_TYPE_SDMA) {
        transportManager_ = std::make_shared<transport::DefaultTransportManager>();
        return BM_OK;
    }
    transportManager_ = transport::TransportManager::Create(transport::TT_COMPOSE);
    std::string deviceNic = "device#tcp://0.0.0.0/0:10002";
    std::string hostNic = "host#" + std::string(options_.nic);
    transport::TransportOptions options;
    options.rankId = options_.rankId;
    options.rankCount = options_.rankCount;
    options.nic = deviceNic + ';' + hostNic;
    options.protocol = options_.bmDataOpType;
    auto ret = transportManager_->OpenDevice(options);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to open device, ret: " << ret);
        transportManager_ = nullptr;
    }
    return ret;
}

Result HybmEntityCompose::InitDataOperator()
{
    if (options_.bmType == HYBM_TYPE_HBM_AI_CORE_INITIATE) {
        return BM_OK;
    }
    switch (options_.bmDataOpType) {
        case HYBM_DOP_TYPE_TCP:
        case HYBM_DOP_TYPE_ROCE:
            dataOperator_ = std::make_shared<HostDataOpRDMA>(options_.rankId, stream_, transportManager_);
            break;
        case HYBM_DOP_TYPE_SDMA:
            dataOperator_ = std::make_shared<HostDataOpSDMA>(stream_);
            break;
        default:
            return BM_ERROR;
    }
    return dataOperator_->Initialized();
}

bool HybmEntityCompose::SdmaReaches(uint32_t remoteRank) const noexcept
{
    if (deviceSegment_ == nullptr) {
        BM_LOG_ERROR("SdmaReaches() segment is null");
        return false;
    }

    return deviceSegment_->CheckSmdaReaches(remoteRank);
}

void *HybmEntityCompose::GetReservedMemoryPtr(hybm_mem_type memType) noexcept
{
    switch (memType) {
        case HYBM_MEM_TYPE_DEVICE:
            return deviceGva_;
        case HYBM_MEM_TYPE_HOST:
            return hostGva_;
        default:
            BM_LOG_ERROR("Failed to get reserved memory ptr Invalid mem type: " << memType);
            return nullptr;
    }
}

std::shared_ptr<MemSlice> HybmEntityCompose::FindMemSlice(hybm_mem_slice_t slice,
                                                          std::shared_ptr<MemSegment> &ownerSegment)
{
    auto realSlice = hostSegment_->GetMemSlice(slice);
    if (realSlice != nullptr) {
        ownerSegment = hostSegment_;
        return realSlice;
    }
    realSlice = deviceSegment_->GetMemSlice(slice);
    if (realSlice != nullptr) {
        ownerSegment = deviceSegment_;
        return realSlice;
    }
    return nullptr;
}

void HybmEntityCompose::GetSegmentFromDirect(hybm_data_copy_direction direct, std::shared_ptr<MemSegment> &src,
                                             std::shared_ptr<MemSegment> &dest)
{
    switch (direct) {
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST:
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE:
        case HYBM_GLOBAL_HOST_TO_LOCAL_HOST:
        case HYBM_GLOBAL_HOST_TO_LOCAL_DEVICE:
            src = hostSegment_;
            break;
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST:
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE:
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_HOST:
        case HYBM_GLOBAL_DEVICE_TO_LOCAL_DEVICE:
            src = deviceSegment_;
            break;
        default:
            break;
    }
    switch (direct) {
        case HYBM_LOCAL_HOST_TO_GLOBAL_HOST:
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_HOST:
        case HYBM_GLOBAL_HOST_TO_GLOBAL_HOST:
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_HOST:
            dest = hostSegment_;
            break;
        case HYBM_LOCAL_HOST_TO_GLOBAL_DEVICE:
        case HYBM_LOCAL_DEVICE_TO_GLOBAL_DEVICE:
        case HYBM_GLOBAL_DEVICE_TO_GLOBAL_DEVICE:
        case HYBM_GLOBAL_HOST_TO_GLOBAL_DEVICE:
            dest = deviceSegment_;
            break;
        default:
            break;
    }
}
}
}
