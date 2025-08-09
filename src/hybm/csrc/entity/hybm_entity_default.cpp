/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <algorithm>
#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "hybm_devide_mem_segment.h"
#include "hybm_data_operator_sdma.h"
#include "hybm_default_transport_manager.h"
#include "hybm_entity_default.h"
#include "hybm_data_operator_rdma.h"
#include "hybm_ex_info_transfer.h"

namespace ock {
namespace mf {

struct TransportExtraInfo {
    uint32_t rankId;
    transport::TransportMemoryKey memKey;
};

MemEntityDefault::MemEntityDefault(int id) noexcept : id_(id), initialized(false) {}

MemEntityDefault::~MemEntityDefault() = default;

int32_t MemEntityDefault::Initialize(const hybm_options *options) noexcept
{
    if (initialized) {
        BM_LOG_WARN("the object is already initialized.");
        return BM_OK;
    }

    if (id_ < 0 || (uint32_t)(id_) >= HYBM_ENTITY_NUM_MAX) {
        BM_LOG_ERROR("input entity id is invalid, input: " << id_ << " must be less than: " << HYBM_ENTITY_NUM_MAX);
        return BM_INVALID_PARAM;
    }

    auto ret = CheckOptions(options);
    if (ret != BM_OK) {
        return ret;
    }

    ret = DlAclApi::AclrtCreateStream(&stream_);
    if (ret != 0) {
        BM_LOG_ERROR("create stream failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    options_ = *options;
    ret = InitSegment();
    if (ret != 0) {
        DlAclApi::AclrtDestroyStream(stream_);
        stream_ = nullptr;
        return ret;
    }

    ret = InitTransManager();
    if (ret != 0) {
        DlAclApi::AclrtDestroyStream(stream_);
        stream_ = nullptr;
        return ret;
    }

    ret = InitDataOperator();
    if (ret != 0) {
        DlAclApi::AclrtDestroyStream(stream_);
        stream_ = nullptr;
        return ret;
    }

    initialized = true;
    return BM_OK;
}

void MemEntityDefault::UnInitialize() noexcept
{
    if (!initialized) {
        return;
    }

    segment_.reset();
    dataOperator_.reset();
    transportManager_.reset();
    DlAclApi::AclrtDestroyStream(stream_);
    stream_ = nullptr;
}

int32_t MemEntityDefault::ReserveMemorySpace(void **reservedMem) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    return segment_->ReserveMemorySpace(reservedMem);
}

int32_t MemEntityDefault::UnReserveMemorySpace() noexcept
{
    return BM_OK;
}

int32_t MemEntityDefault::AllocLocalMemory(uint64_t size, uint32_t flags, hybm_mem_slice_t &slice) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    if ((size % DEVICE_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("allocate memory size: " << size << " invalid, page size is: " << DEVICE_LARGE_PAGE_SIZE);
        return BM_INVALID_PARAM;
    }

    std::shared_ptr<MemSlice> realSlice;
    auto ret = segment_->AllocLocalMemory(size, realSlice);
    if (ret != 0) {
        BM_LOG_ERROR("segment allocate slice with size: " << size << " failed: " << ret);
        return ret;
    }

    slice = realSlice->ConvertToId();
    transport::TransportMemoryRegion info;
    info.size = realSlice->size_;
    info.addr = realSlice->vAddress_;
    info.access = transport::REG_MR_ACCESS_FLAG_BOTH_READ_WRITE;
    info.flags = segment_->GetMemoryType() == HYBM_MEM_TYPE_DEVICE ? transport::REG_MR_FLAG_HBM :
                                                                     transport::REG_MR_FLAG_DRAM;
    ret = transportManager_->RegisterMemoryRegion(info);
    if (ret != 0) {
        BM_LOG_ERROR("register memory region allocate failed: " << ret << ", info: " << info);
        return ret;
    }

    return UpdateHybmDeviceInfo(0);
}

void MemEntityDefault::FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept {}

int32_t MemEntityDefault::ExportExchangeInfo(hybm_exchange_info &desc, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    std::string info;
    EntityExportInfo exportInfo;
    exportInfo.magic = EXPORT_INFO_MAGIC;
    exportInfo.version = EXPORT_INFO_VERSION;
    exportInfo.rankId = options_.rankId;
    auto &nic = transportManager_->GetNic();
    if (nic.size() >= sizeof(exportInfo.nic)) {
        BM_LOG_ERROR("transport get nic(" << nic << ") too long.");
        return BM_ERROR;
    }
    std::copy_n(nic.c_str(), nic.size(), exportInfo.nic);
    auto ret = LiteralExInfoTranslater<EntityExportInfo>{}.Serialize(exportInfo, info);
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

int32_t MemEntityDefault::ExportExchangeInfo(hybm_mem_slice_t slice, hybm_exchange_info &desc, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    std::string info;
    auto realSlice = segment_->GetMemSlice(slice);
    if (realSlice == nullptr) {
        return BM_INVALID_PARAM;
    }

    auto ret = segment_->Export(realSlice, info);
    if (ret != 0) {
        BM_LOG_ERROR("export to string failed: " << ret);
        return ret;
    }

    TransportExtraInfo transportInfo;
    transportInfo.rankId = options_.rankId;
    ret = transportManager_->QueryMemoryKey(realSlice->vAddress_, transportInfo.memKey);
    if (ret != BM_OK) {
        BM_LOG_ERROR("query memory key for slice: " << realSlice->index_ << " failed: " << ret);
        return ret;
    }

    if (info.size() + sizeof(transportInfo) > sizeof(desc.desc)) {
        BM_LOG_ERROR("export to string wrong size: " << info.size() << ", the correct size is: " << sizeof(desc.desc));
        return BM_ERROR;
    }

    std::copy_n(info.data(), info.size(), desc.desc);
    std::copy_n(reinterpret_cast<uint8_t *>(&transportInfo), sizeof(transportInfo), desc.desc + info.size());
    desc.descLen = info.size() + sizeof(transportInfo);
    return BM_OK;
}

int32_t MemEntityDefault::ImportExchangeInfo(const hybm_exchange_info *desc, uint32_t count, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    auto ret = DlAclApi::AclrtSetDevice(HybmGetInitDeviceId());
    if (ret != BM_OK) {
        BM_LOG_ERROR("set device id to be " << HybmGetInitDeviceId() << " failed: " << ret);
        return BM_ERROR;
    }

    if (desc == nullptr) {
        BM_LOG_ERROR("the input desc is nullptr.");
        return BM_ERROR;
    }

    const TransportExtraInfo *extraInfo;
    std::unordered_map<uint32_t, std::vector<transport::TransportMemoryKey>> transportInfo;
    std::vector<std::string> infos;
    for (auto i = 0U; i < count; i++) {
        if (desc[i].descLen <= sizeof(TransportExtraInfo)) {
            BM_LOG_ERROR("import slice infos[" << i << "] size too short: " << desc[i].descLen);
            return BM_INVALID_PARAM;
        }

        extraInfo =
            (const TransportExtraInfo *)(const void *)(desc[i].desc + (desc[i].descLen - sizeof(TransportExtraInfo)));
        transportInfo[extraInfo->rankId].emplace_back(extraInfo->memKey);
        infos.emplace_back((const char *)desc[i].desc, desc[i].descLen - sizeof(TransportExtraInfo));
    }

    ret = segment_->Import(infos);
    if (ret != BM_OK) {
        BM_LOG_ERROR("segment import infos failed: " << ret);
        return ret;
    }

    for (auto &e : transportInfo) {
        importedMemories_[e.first].insert(importedMemories_[e.first].end(), e.second.begin(), e.second.end());
    }
    return BM_OK;
}

int32_t MemEntityDefault::ImportEntityExchangeInfo(const hybm_exchange_info *desc, uint32_t count,
                                                   uint32_t flags) noexcept
{
    BM_ASSERT_RETURN(initialized, BM_NOT_INITIALIZED);
    BM_ASSERT_RETURN(desc != nullptr, BM_INVALID_PARAM);

    LiteralExInfoTranslater<EntityExportInfo> translator;
    std::vector<EntityExportInfo> deserializedInfos(count);
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
    if (transportPrepared) {
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

    transportPrepared = true;
    return BM_OK;
}

int32_t MemEntityDefault::SetExtraContext(const void *context, uint32_t size) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    BM_ASSERT_RETURN(context != nullptr, BM_INVALID_PARAM);
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

void MemEntityDefault::Unmap() noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return;
    }

    segment_->Unmap();
}

int32_t MemEntityDefault::Mmap() noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    return segment_->Mmap();
}

int32_t MemEntityDefault::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    return segment_->RemoveImported(ranks);
}

int32_t MemEntityDefault::CopyData(const void *src, void *dest, uint64_t length, hybm_data_copy_direction direction,
                                   void *stream, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    ExtOptions options{};
    options.flags = flags;
    segment_->GetRankIdByAddr(src, length, options.srcRankId);
    segment_->GetRankIdByAddr(dest, length, options.destRankId);
    options.stream = stream;
    return dataOperator_->DataCopy(src, dest, length, direction, options);
}

int32_t MemEntityDefault::CopyData2d(const void *src, uint64_t spitch, void *dest, uint64_t dpitch, uint64_t width,
                                     uint64_t height, hybm_data_copy_direction direction, void *stream,
                                     uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }
    ExtOptions options{};
    options.flags = flags;
    options.stream = stream;
    segment_->GetRankIdByAddr(src, spitch * height, options.srcRankId);
    segment_->GetRankIdByAddr(dest, dpitch * height, options.destRankId);
    return dataOperator_->DataCopy2d(src, spitch, dest, dpitch, width, height, direction, options);
}

bool MemEntityDefault::CheckAddressInEntity(const void *ptr, uint64_t length) const noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return false;
    }

    return segment_->MemoryInRange(ptr, length);
}

int MemEntityDefault::CheckOptions(const hybm_options *options) noexcept
{
    if (options == nullptr) {
        BM_LOG_ERROR("initialize with nullptr.");
        return BM_INVALID_PARAM;
    }

    if (options->rankId >= options->rankCount) {
        BM_LOG_ERROR("local rank id: " << options->rankId << " invalid, total is " << options->rankCount);
        return BM_INVALID_PARAM;
    }

    if (options->singleRankVASpace == 0UL || (options->singleRankVASpace % DEVICE_LARGE_PAGE_SIZE) != 0UL) {
        BM_LOG_ERROR("invalid local memory size(" << options->singleRankVASpace << ") should be times of "
                                                  << DEVICE_LARGE_PAGE_SIZE);
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

int MemEntityDefault::UpdateHybmDeviceInfo(uint32_t extCtxSize) noexcept
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

void MemEntityDefault::SetHybmDeviceInfo(HybmDeviceMeta &info)
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

Result MemEntityDefault::InitSegment()
{
    switch (options_.bmType) {
        case HYBM_TYPE_HBM_AI_CORE_INITIATE:
        case HYBM_TYPE_HBM_HOST_INITIATE:
            return InitHbmSegment();
        case HYBM_TYPE_DRAM_HOST_INITIATE:
            return InitDramSegment();
        default:
            return BM_INVALID_PARAM;
    }
}

Result MemEntityDefault::InitHbmSegment()
{
    if (options_.singleRankVASpace == 0) {
        BM_LOG_INFO("Hbm rank space is zero.");
        return BM_OK;
    }

    MemSegmentOptions segmentOptions;
    segmentOptions.size = options_.singleRankVASpace;
    segmentOptions.segId = HybmGetInitDeviceId();
    segmentOptions.segType = HYBM_MST_HBM;
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;
    segment_ = MemSegment::Create(segmentOptions, id_);
    if (segment_ == nullptr) {
        BM_LOG_ERROR("Failed to create hbm segment");
        return BM_ERROR;
    }

    return MemSegmentDevice::GetDeviceId(HybmGetInitDeviceId());
}

Result MemEntityDefault::InitDramSegment()
{
    if (options_.singleRankVASpace == 0) {
        BM_LOG_INFO("Dram rank space is zero.");
        return BM_OK;
    }

    MemSegmentOptions segmentOptions;
    segmentOptions.size = options_.singleRankVASpace;
    segmentOptions.segId = HybmGetInitDeviceId();
    segmentOptions.segType = HYBM_MST_DRAM;
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;
    segment_ = MemSegment::Create(segmentOptions, id_);
    if (segment_ == nullptr) {
        BM_LOG_ERROR("Failed to create dram segment");
        return BM_ERROR;
    }

    return BM_OK;
}

Result MemEntityDefault::InitTransManager()
{
    if (options_.bmDataOpType == HYBM_DOP_TYPE_SDMA) {
        transportManager_ = std::make_shared<transport::DefaultTransportManager>();
        return BM_OK;
    }
    switch (options_.bmType) {
        case HYBM_TYPE_HBM_AI_CORE_INITIATE:
        case HYBM_TYPE_HBM_HOST_INITIATE:
            transportManager_ = transport::TransportManager::Create(transport::TT_HCCP);
            break;
        case HYBM_TYPE_DRAM_HOST_INITIATE:
            transportManager_ = transport::TransportManager::Create(transport::TT_HCOM);
            break;
        case HYBM_TYPE_HBM_DRAM_HOST_INITIATE:
            transportManager_ = transport::TransportManager::Create(transport::TT_COMPOSE);
            break;
        default:
            return BM_INVALID_PARAM;
    }
    transport::TransportOptions options;
    options.rankId = options_.rankId;
    options.rankCount = options_.rankCount;
    options.nic = options_.nic;
    options.protocol = options_.bmDataOpType;
    auto ret = transportManager_->OpenDevice(options);
    if (ret != 0) {
        BM_LOG_ERROR("Failed to open device, ret: " << ret);
        transportManager_ = nullptr;
    }
    return ret;
}

Result MemEntityDefault::InitDataOperator()
{
    // AI_CORE驱动不走这里的dateOperator
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

bool MemEntityDefault::SdmaReaches(uint32_t remoteRank) const noexcept
{
    if (segment_ == nullptr) {
        BM_LOG_ERROR("SdmaReaches() segment is null");
        return false;
    }
    
    return segment_->CheckSmdaReaches(remoteRank);
}
}
}