/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_entity_default.h"

#include <algorithm>

#include "dl_acl_api.h"
#include "hybm_data_operator_rdma.h"
#include "hybm_device_mem_segment.h"
#include "hybm_data_operator_sdma.h"
#include "hybm_dp_device_rdma.h"
#include "hybm_device_mem_segment.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_logger.h"

namespace ock {
namespace mf {

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

    if (options_.bmType != HYBM_TYPE_AI_CORE_INITIATE) {
        ret = InitDataOperator();
        if (ret != 0) {
            DlAclApi::AclrtDestroyStream(stream_);
            stream_ = nullptr;
            return ret;
        }
    }

    initialized = true;
    return BM_OK;
}

void MemEntityDefault::UnInitialize() noexcept
{
    if (!initialized) {
        return;
    }

    hbmSegment_.reset();
    dramSegment_.reset();
    sdmaDataOperator_.reset();
    devRdmaDataOperator_.reset();
    hostRdmaDataOperator_.reset();
    if (transportManager_ != nullptr) {
        transportManager_->CloseDevice();
        transportManager_.reset();
    }
    DlAclApi::AclrtDestroyStream(stream_);
    stream_ = nullptr;
    initialized = false;
}

int32_t MemEntityDefault::ReserveMemorySpace() noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    if (hbmSegment_ != nullptr) {
        auto ret = hbmSegment_->ReserveMemorySpace(&hbmGva_);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to reserver HBM memory space ret: " << ret);
            return ret;
        }
    }

    if (dramSegment_ != nullptr) {
        auto ret = dramSegment_->ReserveMemorySpace(&dramGva_);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to reserver DRAM memory space ret: " << ret);
            return ret;
        }
    }

    return BM_OK;
}

int32_t MemEntityDefault::UnReserveMemorySpace() noexcept
{
    return BM_OK;
}

int32_t MemEntityDefault::AllocLocalMemory(uint64_t size, hybm_mem_type mType, uint32_t flags,
                                           hybm_mem_slice_t &slice) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    if ((size % DEVICE_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("allocate memory size: " << size << " invalid, page size is: " << DEVICE_LARGE_PAGE_SIZE);
        return BM_INVALID_PARAM;
    }

    auto segment = mType == HYBM_MEM_TYPE_DEVICE ? hbmSegment_ : dramSegment_;
    if (segment == nullptr) {
        BM_LOG_ERROR("allocate memory with mType: " << mType << ", no segment.");
        return BM_INVALID_PARAM;
    }

    std::shared_ptr<MemSlice> realSlice;
    auto ret = segment->AllocLocalMemory(size, realSlice);
    if (ret != 0) {
        BM_LOG_ERROR("segment allocate slice with size: " << size << " failed: " << ret);
        return ret;
    }

    slice = realSlice->ConvertToId();
    transport::TransportMemoryRegion info;
    info.size = realSlice->size_;
    info.addr = realSlice->vAddress_;
    info.access = transport::REG_MR_ACCESS_FLAG_BOTH_READ_WRITE;
    info.flags =
        segment->GetMemoryType() == HYBM_MEM_TYPE_DEVICE ? transport::REG_MR_FLAG_HBM : transport::REG_MR_FLAG_DRAM;
    if (transportManager_ != nullptr) {
        ret = transportManager_->RegisterMemoryRegion(info);
        if (ret != 0) {
            BM_LOG_ERROR("register memory region allocate failed: " << ret << ", info: " << info);
            return ret;
        }
    }

    return UpdateHybmDeviceInfo(0);
}

void MemEntityDefault::FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept {}

int32_t MemEntityDefault::ExportExchangeInfo(ExchangeInfoWriter &desc, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    std::string info;
    EntityExportInfo exportInfo;
    exportInfo.rankId = options_.rankId;
    if (transportManager_ != nullptr) {
        auto &nic = transportManager_->GetNic();
        if (nic.size() >= sizeof(exportInfo.nic)) {
            BM_LOG_ERROR("transport get nic(" << nic << ") too long.");
            return BM_ERROR;
        }
        size_t copyLen = std::min(nic.size(), sizeof(exportInfo.nic));
        std::copy_n(nic.c_str(), copyLen, exportInfo.nic);
    }

    auto ret = LiteralExInfoTranslater<EntityExportInfo>{}.Serialize(exportInfo, info);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        return BM_ERROR;
    }

    ret = desc.Append(info.data(), info.size());
    if (ret != 0) {
        BM_LOG_ERROR("export to string wrong size: " << info.size());
        return BM_ERROR;
    }

    return BM_OK;
}

int32_t MemEntityDefault::ExportExchangeInfo(hybm_mem_slice_t slice, ExchangeInfoWriter &desc, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    uint64_t exportMagic = 0;
    std::string info;
    std::shared_ptr<MemSlice> realSlice;
    std::shared_ptr<MemSegment> currentSegment;
    if (hbmSegment_ != nullptr) {
        realSlice = hbmSegment_->GetMemSlice(slice);
        currentSegment = hbmSegment_;
        exportMagic = HBM_SLICE_EXPORT_INFO_MAGIC;
    }
    if (realSlice == nullptr && dramSegment_ != nullptr) {
        realSlice = dramSegment_->GetMemSlice(slice);
        currentSegment = dramSegment_;
        exportMagic = DRAM_SLICE_EXPORT_INFO_MAGIC;
    }
    if (realSlice == nullptr) {
        BM_LOG_ERROR("cannot find input slice for export.");
        return BM_INVALID_PARAM;
    }

    auto ret = currentSegment->Export(realSlice, info);
    if (ret != 0) {
        BM_LOG_ERROR("export to string failed: " << ret);
        return ret;
    }

    SliceExportTransportKey transportKey{exportMagic, options_.rankId, realSlice->vAddress_};
    if (transportManager_ != nullptr) {
        ret = transportManager_->QueryMemoryKey(realSlice->vAddress_, transportKey.key);
        if (ret != 0) {
            BM_LOG_ERROR("query memory key when export slice failed: " << ret);
            return ret;
        }
    }

    ret = desc.Append(transportKey);
    if (ret != 0) {
        BM_LOG_ERROR("append transport key failed: " << ret);
        return ret;
    }

    ret = desc.Append(info.data(), info.size());
    if (ret != 0) {
        BM_LOG_ERROR("export to string wrong size: " << info.size());
        return BM_ERROR;
    }

    return BM_OK;
}

int32_t MemEntityDefault::ImportExchangeInfo(const ExchangeInfoReader desc[], uint32_t count, uint32_t flags) noexcept
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

    std::unordered_map<uint32_t, std::vector<transport::TransportMemoryKey>> tempKeyMap;
    ret = ImportForTransport(desc, count);
    if (ret != BM_OK) {
        return ret;
    }

    uint64_t magic;
    if (desc[0].Test(magic) < 0) {
        BM_LOG_ERROR("left import data no magic size.");
        return BM_OK;
    }

    auto currentSegment = magic == DRAM_SLICE_EXPORT_INFO_MAGIC ? dramSegment_ : hbmSegment_;
    std::vector<std::string> infos;
    for (auto i = 0U; i < count; i++) {
        infos.emplace_back(desc[i].LeftToString());
    }

    ret = currentSegment->Import(infos);
    if (ret != BM_OK) {
        BM_LOG_ERROR("segment import infos failed: " << ret);
        return ret;
    }

    return BM_OK;
}

int32_t MemEntityDefault::ImportEntityExchangeInfo(const ExchangeInfoReader desc[], uint32_t count,
                                                   uint32_t flags) noexcept
{
    BM_ASSERT_RETURN(initialized, BM_NOT_INITIALIZED);
    BM_ASSERT_RETURN(desc != nullptr, BM_INVALID_PARAM);

    if (transportManager_ == nullptr) {
        BM_LOG_INFO("no transport, no need import.");
        return BM_OK;
    }

    std::vector<EntityExportInfo> deserializedInfos(count);
    for (auto i = 0U; i < count; i++) {
        auto ret = desc[i].Read(deserializedInfos[i]);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
    }

    transport::HybmTransPrepareOptions prepareOptions;
    for (auto i = 0U; i < deserializedInfos.size(); i++) {
        auto &info = deserializedInfos[i];
        if (deserializedInfos[i].magic != ENTITY_EXPORT_INFO_MAGIC) {
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

    if (hbmSegment_ != nullptr) {
        hbmSegment_->Unmap();
    }
    if (dramSegment_ != nullptr) {
        dramSegment_->Unmap();
    }
}

int32_t MemEntityDefault::Mmap() noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }


    if (hbmSegment_ != nullptr) {
        auto ret = hbmSegment_->Mmap();
        if (ret != BM_OK) {
            return ret;
        }
    }

    if (dramSegment_ != nullptr) {
        auto ret = dramSegment_->Mmap();
        if (ret != BM_OK) {
            return ret;
        }
    }

    return BM_OK;
}

int32_t MemEntityDefault::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    if (hbmSegment_ != nullptr) {
        auto ret = hbmSegment_->RemoveImported(ranks);
        if (ret != BM_OK) {
            return ret;
        }
    }

    if (dramSegment_ != nullptr) {
        auto ret = dramSegment_->RemoveImported(ranks);
        if (ret != BM_OK) {
            return ret;
        }
    }

    if (transportManager_ != nullptr) {
        auto ret = transportManager_->RemoveRanks(ranks);
        if (ret != BM_OK) {
            BM_LOG_WARN("transport remove ranks failed: " << ret);
        }
    }

    return BM_OK;
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
    options.stream = stream;
    auto ret = GenCopyExtOption(src, dest, length, options);
    if (ret != BM_OK) {
        BM_LOG_ERROR("generate copy ext options failed: " << ret);
        return ret;
    }

    if ((options_.bmDataOpType & HYBM_DOP_TYPE_SDMA) != 0 && sdmaDataOperator_ != nullptr) {
        ret = sdmaDataOperator_->DataCopy(src, dest, length, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }

        BM_LOG_ERROR("SDMA data copy direction: " << direction << ", failed : " << ret);
    }

    if (devRdmaDataOperator_ != nullptr) {
        ret = devRdmaDataOperator_->DataCopy(src, dest, length, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Device RDMA data copy direction: " << direction << ", failed : " << ret);
    }

    if (hostRdmaDataOperator_ != nullptr) {
        ret = hostRdmaDataOperator_->DataCopy(src, dest, length, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Host RDMA data copy direction: " << direction << ", failed : " << ret);
    }

    BM_LOG_ERROR("copy data failed.");
    return BM_ERROR;
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
    auto ret = GenCopyExtOption(src, dest, spitch * height, options);
    if (ret != BM_OK) {
        BM_LOG_ERROR("generate copy ext options failed: " << ret);
        return ret;
    }

    if ((options_.bmDataOpType & HYBM_DOP_TYPE_SDMA) != 0 && sdmaDataOperator_ != nullptr) {
        ret = sdmaDataOperator_->DataCopy2d(src, spitch, dest,  dpitch, width, height, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }

        BM_LOG_ERROR("SDMA data copy direction: " << direction << ", failed : " << ret);
    }

    if (devRdmaDataOperator_ != nullptr) {
        ret = devRdmaDataOperator_->DataCopy2d(src, spitch, dest,  dpitch, width, height, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Device RDMA data copy direction: " << direction << ", failed : " << ret);
    }

    if (hostRdmaDataOperator_ != nullptr) {
        ret = hostRdmaDataOperator_->DataCopy2d(src, spitch, dest,  dpitch, width, height, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Host RDMA data copy direction: " << direction << ", failed : " << ret);
    }
    BM_LOG_ERROR("copy data failed.");
    return BM_ERROR;
}

int32_t MemEntityDefault::BatchCopyData(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                        void *stream, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }
    int32_t ret = BM_ERROR;
    ExtOptions options{};
    options.flags = flags;
    options.stream = stream;

    if ((options_.bmDataOpType & HYBM_DOP_TYPE_SDMA) != 0 && sdmaDataOperator_ != nullptr) {
        ret = sdmaDataOperator_->BatchDataCopy(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }

        BM_LOG_ERROR("SDMA data copy direction: " << direction << ", failed : " << ret);
        return ret;
    }

    if (devRdmaDataOperator_ != nullptr) {
        ret = devRdmaDataOperator_->BatchDataCopy(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Device RDMA data copy direction: " << direction << ", failed : " << ret);
    }

    if (hostRdmaDataOperator_ != nullptr) {
        ret = hostRdmaDataOperator_->BatchDataCopy(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Host RDMA data copy direction: " << direction << ", failed : " << ret);
    }
    return ret;
}

int32_t MemEntityDefault::Wait() noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }
    if ((options_.bmDataOpType & HYBM_DOP_TYPE_SDMA) != 0 && sdmaDataOperator_ != nullptr) {
        return sdmaDataOperator_->Wait(0);
    }
    if (hostRdmaDataOperator_ != nullptr) {
        return hostRdmaDataOperator_->Wait(0);
    }
    BM_LOG_ERROR("Not wait type:" << options_.bmDataOpType);
    return BM_ERROR;
}

bool MemEntityDefault::CheckAddressInEntity(const void *ptr, uint64_t length) const noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return false;
    }

    if (hbmSegment_ != nullptr && hbmSegment_->MemoryInRange(ptr, length)) {
        return true;
    }

    if (dramSegment_ != nullptr && dramSegment_->MemoryInRange(ptr, length)) {
        return true;
    }

    return false;
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
    if (options_.bmType != HYBM_TYPE_AI_CORE_INITIATE) {
        return BM_OK;
    }

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

int MemEntityDefault::ImportForTransport(const ExchangeInfoReader desc[], uint32_t count) noexcept
{
    SliceExportTransportKey transportKey;
    for (auto i = 0U; i < count; i++) {
        auto ret = desc[i].Read(transportKey);
        if (ret != 0) {
            BM_LOG_ERROR("read trans failed, left: " << desc[i].LeftBytes() << ", need: " << sizeof(transportKey));
            return BM_ERROR;
        }
        auto pos = importedMemories_.emplace(transportKey.rankId, std::vector<transport::TransportMemoryKey>()).first;
        pos->second.emplace_back(transportKey.key);
    }

    return BM_OK;
}

int MemEntityDefault::GenCopyExtOption(const void *src, void *dest, uint64_t length, ExtOptions &options) noexcept
{
    if ((uint64_t)(ptrdiff_t)src >= HYBM_HOST_GVA_START_ADDR) {
        if (dramSegment_ == nullptr) {
            options.srcRankId = options_.rankId;
        } else {
            options.srcRankId = dramSegment_->GetRankIdByAddr(src, length);
        }
    } else {
        if (hbmSegment_ == nullptr) {
            options.srcRankId = options_.rankId;
        } else {
            options.srcRankId = hbmSegment_->GetRankIdByAddr(src, length);
        }
    }
    if (options.srcRankId == UINT32_MAX) {
        options.srcRankId = options_.rankId;
    }

    if ((uint64_t)(ptrdiff_t)dest >= HYBM_HOST_GVA_START_ADDR) {
        if (dramSegment_ == nullptr) {
            options.destRankId = options_.rankId;
        } else {
            options.destRankId = dramSegment_->GetRankIdByAddr(dest, length);
        }
    } else {
        if (hbmSegment_ == nullptr) {
            options.destRankId = options_.rankId;
        } else {
            options.destRankId = hbmSegment_->GetRankIdByAddr(dest, length);
        }
    }
    if (options.destRankId == UINT32_MAX) {
        options.destRankId = options_.rankId;
    }

    return BM_OK;
}

Result MemEntityDefault::InitSegment()
{
    BM_LOG_DEBUG("Initialize segment with type: " << std::hex << options_.memType);
    if (options_.memType & HYBM_MEM_TYPE_DEVICE) {
        auto ret = InitHbmSegment();
        if (ret != BM_OK) {
            BM_LOG_ERROR("InitHbmSegment() failed: " << ret);
            return ret;
        }
    }

    if (options_.memType & HYBM_MEM_TYPE_HOST) {
        auto ret = InitDramSegment();
        if (ret != BM_OK) {
            BM_LOG_ERROR("InitDramSegment() failed: " << ret);
            return ret;
        }
    }

    return BM_OK;
}

Result MemEntityDefault::InitHbmSegment()
{
    if (options_.deviceVASpace == 0) {
        BM_LOG_INFO("Hbm rank space is zero.");
        return BM_OK;
    }

    MemSegmentOptions segmentOptions;
    segmentOptions.size = options_.deviceVASpace;
    segmentOptions.segId = HybmGetInitDeviceId();
    segmentOptions.segType = HYBM_MST_HBM;
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;
    hbmSegment_ = MemSegment::Create(segmentOptions, id_);
    if (hbmSegment_ == nullptr) {
        BM_LOG_ERROR("Failed to create hbm segment");
        return BM_ERROR;
    }

    return MemSegmentDevice::GetDeviceId(HybmGetInitDeviceId());
}

Result MemEntityDefault::InitDramSegment()
{
    if (options_.hostVASpace == 0) {
        BM_LOG_INFO("Dram rank space is zero.");
        return BM_OK;
    }

    MemSegmentOptions segmentOptions;
    segmentOptions.size = options_.hostVASpace;
    segmentOptions.segId = HybmGetInitDeviceId();
    segmentOptions.segType = HYBM_MST_DRAM;
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;
    dramSegment_ = MemSegment::Create(segmentOptions, id_);
    if (dramSegment_ == nullptr) {
        BM_LOG_ERROR("Failed to create dram segment");
        return BM_ERROR;
    }

    return MemSegmentDevice::GetDeviceId(HybmGetInitDeviceId());
}

Result MemEntityDefault::InitTransManager()
{
    if (options_.rankCount <= 1) {
        BM_LOG_INFO("rank total count : " << options_.rankCount << ", no transport.");
        return BM_OK;
    }

    auto hostTransFlags = HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_TCP;
    auto composeTransFlags = HYBM_DOP_TYPE_DEVICE_RDMA | hostTransFlags;
    if ((options_.bmDataOpType & composeTransFlags) == 0) {
        BM_LOG_DEBUG("NO RDMA Data Operator transport skip init.");
        return BM_OK;
    }

    if ((options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) && (options_.bmDataOpType & hostTransFlags)) {
        transportManager_ = transport::TransportManager::Create(transport::TT_COMPOSE);
    } else if (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        transportManager_ = transport::TransportManager::Create(transport::TT_HCCP);
    } else {
        transportManager_ = transport::TransportManager::Create(transport::TT_HCOM);
    }

    transport::TransportOptions options;
    options.rankId = options_.rankId;
    options.rankCount = options_.rankCount;
    options.protocol = options_.bmDataOpType;
    options.role = options_.role;
    options.initialType = options_.bmType;
    options.nic = options_.nic;
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
    if (options_.bmType == HYBM_TYPE_AI_CORE_INITIATE) {
        return BM_OK;
    }

    if (options_.bmDataOpType & HYBM_DOP_TYPE_SDMA) {
        sdmaDataOperator_ = std::make_shared<HostDataOpSDMA>(stream_);
        auto ret = sdmaDataOperator_->Initialize();
        if (ret != BM_OK) {
            BM_LOG_ERROR("SDMA data operator init failed, ret:" << ret);
            return ret;
        }
    }

    if (options_.rankCount > 1) {
        if (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
            devRdmaDataOperator_ = std::make_shared<DataOpDeviceRDMA>(options_.rankId, transportManager_);
            auto ret = devRdmaDataOperator_->Initialize();
            if (ret != BM_OK) {
                BM_LOG_ERROR("Device RDMA data operator init failed, ret:" << ret);
                return ret;
            }
        }
    }
    if (options_.bmDataOpType & (HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_TCP)) {
        hostRdmaDataOperator_ = std::make_shared<HostDataOpRDMA>(options_.rankId, stream_, transportManager_);
        auto ret = hostRdmaDataOperator_->Initialize();
        if (ret != BM_OK) {
            BM_LOG_ERROR("Host RDMA data operator init failed, ret:" << ret);
            return ret;
        }
    }

    return BM_OK;
}

bool MemEntityDefault::SdmaReaches(uint32_t remoteRank) const noexcept
{
    if (hbmSegment_ != nullptr) {
        return hbmSegment_->CheckSmdaReaches(remoteRank);
    }

    if (dramSegment_ != nullptr) {
        return dramSegment_->CheckSmdaReaches(remoteRank);
    }

    return false;
}

hybm_data_op_type MemEntityDefault::CanReachDataOperators(uint32_t remoteRank) const noexcept
{
    uint32_t supportDataOp = 0U;
    if (sdmaDataOperator_ != nullptr && SdmaReaches(remoteRank)) {
        supportDataOp |= (HYBM_DOP_TYPE_MTE | HYBM_DOP_TYPE_SDMA);
    }

    if (devRdmaDataOperator_ != nullptr) {
        supportDataOp |= HYBM_DOP_TYPE_DEVICE_RDMA;
    }

    if (hostRdmaDataOperator_ != nullptr) {
        supportDataOp |= HYBM_DOP_TYPE_HOST_RDMA;
    }

    return static_cast<hybm_data_op_type>(supportDataOp);
}

void *MemEntityDefault::GetReservedMemoryPtr(hybm_mem_type memType) noexcept
{
    if (memType == HYBM_MEM_TYPE_DEVICE) {
        return hbmGva_;
    }

    if (memType == HYBM_MEM_TYPE_HOST) {
        return dramGva_;
    }

    return nullptr;
}
}  // namespace mf
}  // namespace ock
