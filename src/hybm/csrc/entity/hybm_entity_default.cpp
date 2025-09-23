/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <algorithm>
#include "hybm_logger.h"
#include "dl_api.h"
#include "dl_acl_api.h"
#include "hybm_device_mem_segment.h"
#include "hybm_data_operator_sdma.h"
#include "hybm_data_operator_rdma.h"
#include "hybm_dp_device_rdma.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_entity_default.h"

using namespace ock::mf::transport;

namespace ock {
namespace mf {

thread_local bool MemEntityDefault::isSetDevice_ = false;

MemEntityDefault::MemEntityDefault(int id) noexcept : id_(id), initialized(false) {}

MemEntityDefault::~MemEntityDefault()
{
    ReleaseResources();
}

int32_t MemEntityDefault::Initialize(const hybm_options *options) noexcept
{
    if (initialized) {
        BM_LOG_WARN("The MemEntity has already been initialized, no action needs.");
        return BM_OK;
    }
    BM_VALIDATE_RETURN((id_ >= 0 && (uint32_t)(id_) < HYBM_ENTITY_NUM_MAX),
                       "input entity id is invalid, input: " << id_ << " must be less than: " << HYBM_ENTITY_NUM_MAX,
                       BM_INVALID_PARAM);

    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(CheckOptions(options), "check options failed.");

    options_ = *options;

    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(LoadExtendLibrary(), "LoadExtendLibrary failed.");
    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(InitSegment(), "InitSegment failed.");

    auto ret = InitTransManager();
    if (ret != BM_OK) {
        BM_LOG_ERROR("init transport manager failed");
        return ret;
    }

    if (options_.bmType != HYBM_TYPE_AI_CORE_INITIATE) {
        ret = InitDataOperator();
        if (ret != 0) {
            return ret;
        }
    }

    initialized = true;
    return BM_OK;
}

int32_t MemEntityDefault::SetThreadAclDevice()
{
    if (isSetDevice_) {
        return BM_OK;
    }
    auto ret = DlAclApi::AclrtSetDevice(HybmGetInitDeviceId());
    if (ret != BM_OK) {
        BM_LOG_ERROR("Set device id to be " << HybmGetInitDeviceId() << " failed: " << ret);
        return ret;
    }
    isSetDevice_ = true;
    BM_LOG_DEBUG("Set device id to be " << HybmGetInitDeviceId() << " success.");
    return BM_OK;
}

void MemEntityDefault::UnInitialize() noexcept
{
    ReleaseResources();
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
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    return segment_->UnreserveMemorySpace();
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
    info.flags =
        segment_->GetMemoryType() == HYBM_MEM_TYPE_DEVICE ? transport::REG_MR_FLAG_HBM : transport::REG_MR_FLAG_DRAM;
    if (transportManager_ != nullptr) {
        ret = transportManager_->RegisterMemoryRegion(info);
        if (ret != 0) {
            BM_LOG_ERROR("register memory region allocate failed: " << ret << ", info: " << info);
            return ret;
        }
    }

    return UpdateHybmDeviceInfo(0);
}

int32_t MemEntityDefault::RegisterLocalMemory(const void *ptr, uint64_t size, uint32_t flags,
                                              hybm_mem_slice_t &slice) noexcept
{
    if (ptr == nullptr || size == 0) {
        BM_LOG_ERROR("input ptr or size(" << size << ") is invalid");
        return BM_INVALID_PARAM;
    }

    std::shared_ptr<MemSlice> realSlice;
    auto ret = segment_->RegisterMemory(ptr, size, realSlice);
    if (ret != 0) {
        BM_LOG_ERROR("segment register slice with size: " << size << " failed: " << ret);
        return ret;
    }

    if (transportManager_ != nullptr) {
        transport::TransportMemoryRegion mr;
        mr.addr = (uint64_t)(ptrdiff_t)ptr;
        mr.size = size;
        ret = transportManager_->RegisterMemoryRegion(mr);
        if (ret != 0) {
            BM_LOG_ERROR("register MR: " << mr << " to transport failed: " << ret);
            return ret;
        }
    }

    slice = realSlice->ConvertToId();
    return BM_OK;
}

int32_t MemEntityDefault::FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_INVALID_PARAM;
    }

    auto memSlice = segment_->GetMemSlice(slice);
    if (memSlice == nullptr) {
        BM_LOG_ERROR("GetMemSlice failed, please check input slice.");
        return BM_INVALID_PARAM;
    }
    if (transportManager_ != nullptr) {
        auto ret = transportManager_->UnregisterMemoryRegion(memSlice->vAddress_);
        if (ret != BM_OK) {
            BM_LOG_ERROR("UnregisterMemoryRegion failed, please check input slice.");
        }
    }
    return segment_->ReleaseSliceMemory(memSlice);
}

int32_t MemEntityDefault::ExportExchangeInfo(ExchangeInfoWriter &desc, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    std::string info;
    EntityExportInfo exportInfo;
    exportInfo.version = EXPORT_INFO_VERSION;
    exportInfo.rankId = options_.rankId;
    exportInfo.role = static_cast<uint16_t>(options_.role);
    if (transportManager_ != nullptr) {
        auto &nic = transportManager_->GetNic();
        if (nic.size() >= sizeof(exportInfo.nic)) {
            BM_LOG_ERROR("transport get nic(" << nic << ") too long.");
            return BM_ERROR;
        }
        size_t copyLen = std::min(nic.size(), sizeof(exportInfo.nic));
        std::copy_n(nic.c_str(), copyLen, exportInfo.nic);
        auto ret = LiteralExInfoTranslater<EntityExportInfo>{}.Serialize(exportInfo, info);
        if (ret != BM_OK) {
            BM_LOG_ERROR("export info failed: " << ret);
            return BM_ERROR;
        }
    }

    auto ret = desc.Append(info.data(), info.size());
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
    if (slice == nullptr) {
        return ExportWithoutSlice(desc, flags);
    }

    return ExportWithSlice(slice, desc, flags);
}

int32_t MemEntityDefault::ImportExchangeInfo(const ExchangeInfoReader desc[], uint32_t count, void *addresses[],
                                             uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    auto ret = SetThreadAclDevice();
    if (ret != BM_OK) {
        return BM_ERROR;
    }

    if (desc == nullptr) {
        BM_LOG_ERROR("the input desc is nullptr.");
        return BM_ERROR;
    }

    ret = ImportForTransport(desc, count, flags);
    if (ret != BM_OK) {
        BM_LOG_ERROR("import for transport failed: " << ret);
        return ret;
    }

    for (auto i = 0U; i < count; i++) {
        if (desc[i].LeftBytes() == 0) {
            return BM_OK;
        }
    }

    std::vector<std::string> infos;
    for (auto i = 0U; i < count; i++) {
        infos.emplace_back(desc[i].LeftToString());
    }

    ret = segment_->Import(infos, addresses);
    if (ret != BM_OK) {
        BM_LOG_ERROR("segment import infos failed: " << ret);
        return ret;
    }

    return BM_OK;
}

int32_t MemEntityDefault::GetExportSliceInfoSize(size_t &size) noexcept
{
    size_t exportSize = 0;
    auto ret = segment_->GetExportSliceSize(exportSize);
    if (ret != 0) {
        BM_LOG_ERROR("GetExportSliceSize for segment failed: " << ret);
        return ret;
    }

    if (transportManager_ != nullptr) {
        exportSize += sizeof(SliceExportTransportKey);
    }
    size = exportSize;
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

int32_t MemEntityDefault::CopyData(hybm_copy_params &params, hybm_data_copy_direction direction, void *stream,
                                   uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }
    auto ret = SetThreadAclDevice();
    if (ret != BM_OK) {
        return ret;
    }
    ExtOptions options{};
    options.flags = flags;
    options.stream = stream;
    segment_->GetRankIdByAddr(params.src, params.dataSize, options.srcRankId);
    segment_->GetRankIdByAddr(params.dest, params.dataSize, options.destRankId);

    params.src = Valid48BitsAddress(params.src);
    params.dest = Valid48BitsAddress(params.dest);

    auto remoteRankId = options.srcRankId == options_.rankId ? options.destRankId : options.srcRankId;
    if (SdmaReaches(remoteRankId) && sdmaDataOperator_ != nullptr) {
        ret = sdmaDataOperator_->DataCopy(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }

        BM_LOG_ERROR("SDMA data copy direction: " << direction << ", failed : " << ret);
    }

    if (devRdmaDataOperator_ != nullptr) {
        ret = devRdmaDataOperator_->DataCopy(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Device RDMA data copy direction: " << direction << ", failed : " << ret);
    }

    if (hostRdmaDataOperator_ != nullptr) {
        ret = hostRdmaDataOperator_->DataCopy(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Host RDMA data copy direction: " << direction << ", failed : " << ret);
    }
    return ret;
}

int32_t MemEntityDefault::BatchCopyData(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                        void *stream, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }
    auto ret = SetThreadAclDevice();
    if (ret != BM_OK) {
        return ret;
    }
    ExtOptions options{};
    options.flags = flags;
    options.stream = stream;
    if (sdmaDataOperator_ != nullptr) {
        BM_LOG_DEBUG("SDMA data copy excute.");
        ret = sdmaDataOperator_->BatchDataCopy(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("SDMA data copy direction: " << direction << ", failed : " << ret);
    } else if (devRdmaDataOperator_ != nullptr) {
        BM_LOG_DEBUG("Device RDMA data copy excute.");
        ret = devRdmaDataOperator_->BatchDataCopy(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Device RDMA data copy direction: " << direction << ", failed : " << ret);
    } else if (hostRdmaDataOperator_ != nullptr) {
        ret = hostRdmaDataOperator_->BatchDataCopy(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Host RDMA data copy direction: " << direction << ", failed : " << ret);
    } else {
        BM_LOG_ERROR("No DataOperator specified, please check.");
        return BM_INVALID_PARAM;
    }
    return ret;
}

int32_t MemEntityDefault::CopyData2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction, void *stream,
                                     uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }
    auto ret = SetThreadAclDevice();
    if (ret != BM_OK) {
        return ret;
    }
    ExtOptions options{};
    options.flags = flags;
    options.stream = stream;
    segment_->GetRankIdByAddr(params.src, params.spitch * params.height, options.srcRankId);
    segment_->GetRankIdByAddr(params.dest, params.dpitch * params.height, options.destRankId);
    params.src = Valid48BitsAddress(params.src);
    params.dest = Valid48BitsAddress(params.dest);

    auto remoteRankId = options.srcRankId == options_.rankId ? options.destRankId : options.srcRankId;
    if (SdmaReaches(remoteRankId) && sdmaDataOperator_ != nullptr) {
        ret = sdmaDataOperator_->DataCopy2d(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }

        BM_LOG_ERROR("SDMA data copy direction: " << direction << ", failed : " << ret);
    }

    if (devRdmaDataOperator_ != nullptr) {
        ret = devRdmaDataOperator_->DataCopy2d(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Device RDMA data copy direction: " << direction << ", failed : " << ret);
    }

    if (hostRdmaDataOperator_ != nullptr) {
        ret = hostRdmaDataOperator_->DataCopy2d(params, direction, options);
        if (ret == BM_OK) {
            return BM_OK;
        }
        BM_LOG_ERROR("Host RDMA data copy direction: " << direction << ", failed : " << ret);
    }
    return ret;
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

    if (!options->globalUniqueAddress) {
        return BM_OK;
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

int MemEntityDefault::LoadExtendLibrary() noexcept
{
    if (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        auto ret = DlApi::LoadExtendLibrary(DL_EXT_LIB_DEVICE_RDMA);
        if (ret != 0) {
            BM_LOG_ERROR("LoadExtendLibrary for DEVICE RDMA failed: " << ret);
            return ret;
        }
    }

    if (options_.bmDataOpType & (HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_TCP)) {
        auto ret = DlApi::LoadExtendLibrary(DL_EXT_LIB_HOST_RDMA);
        if (ret != 0) {
            BM_LOG_ERROR("LoadExtendLibrary for DEVICE RDMA failed: " << ret);
            return ret;
        }
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

int32_t MemEntityDefault::ExportWithSlice(hybm_mem_slice_t slice, ExchangeInfoWriter &desc, uint32_t flags) noexcept
{
    auto realSlice = segment_->GetMemSlice(slice);
    if (realSlice == nullptr) {
        BM_LOG_ERROR("import with invalid slice.");
        return BM_INVALID_PARAM;
    }

    if (transportManager_ != nullptr) {
        SliceExportTransportKey transportKey{options_.rankId, realSlice->vAddress_};
        auto ret = transportManager_->QueryMemoryKey(realSlice->vAddress_, transportKey.key);
        if (ret != 0) {
            BM_LOG_ERROR("query memory key when export slice failed: " << ret);
            return ret;
        }

        ret = desc.Append(transportKey);
        if (ret != 0) {
            BM_LOG_ERROR("append transport key failed: " << ret);
            return ret;
        }
    }

    std::string info;
    auto ret = segment_->Export(realSlice, info);
    if (ret != 0) {
        BM_LOG_ERROR("export to string failed: " << ret);
        return ret;
    }

    ret = desc.Append(info.data(), info.length());
    if (ret != 0) {
        BM_LOG_ERROR("append slice export info failed: " << ret);
        return ret;
    }
    return BM_OK;
}

int32_t MemEntityDefault::ExportWithoutSlice(ExchangeInfoWriter &desc, uint32_t flags) noexcept
{
    std::string info;
    auto ret = segment_->Export(info);
    if (ret != BM_OK && ret != BM_NOT_SUPPORTED) {
        BM_LOG_ERROR("export to string failed: " << ret);
        return ret;
    }

    ret = ExportExchangeInfo(desc, flags);
    if (ret != 0) {
        BM_LOG_ERROR("ExportExchangeInfo failed: " << ret);
        return ret;
    }

    ret = desc.Append(info.data(), info.size());
    if (ret != 0) {
        BM_LOG_ERROR("add segment export info failed.");
        return BM_ERROR;
    }

    return BM_OK;
}

int32_t MemEntityDefault::ImportForTransportPrecheck(const ExchangeInfoReader desc[],
                                                     uint32_t &count,
                                                     bool &importInfoEntity)
{
    int ret = BM_OK;
    uint64_t magic;
    EntityExportInfo entityExportInfo;
    SliceExportTransportKey transportKey;

    for (auto i = 0U; i < count; i++) {
        ret = desc[i].Test(magic);
        if (ret != 0) {
            BM_LOG_ERROR("read magic from import : " << i << " failed.");
            return BM_ERROR;
        }

        if (magic == EXPORT_INFO_MAGIC) {
            ret = desc[i].Read(entityExportInfo);
            if (ret == 0) {
                importedRanks_[entityExportInfo.rankId] = entityExportInfo;
                importInfoEntity = true;
            }
        } else if (magic == EXPORT_SLICE_MAGIC) {
            ret = desc[i].Read(transportKey);
            if (ret == 0) {
                std::unique_lock<std::mutex> uniqueLock{importMutex_};
                importedMemories_[transportKey.rankId][transportKey.address] = transportKey.key;
            }
        } else {
            BM_LOG_ERROR("magic(" << std::hex << magic << ") invalid");
            ret = BM_ERROR;
        }

        if (ret != 0) {
            BM_LOG_ERROR("read info for transport failed: " << ret);
            return ret;
        }
    }
    return BM_OK;
}

int32_t MemEntityDefault::ImportForTransport(const ExchangeInfoReader desc[], uint32_t count, uint32_t flags) noexcept
{
    if (transportManager_ == nullptr) {
        return BM_OK;
    }

    int ret = BM_OK;
    bool importInfoEntity = false;
    ret = ImportForTransportPrecheck(desc, count, importInfoEntity);
    if (ret != BM_OK) {
        return ret;
    }

    transport::HybmTransPrepareOptions transOptions;
    std::unique_lock<std::mutex> uniqueLock{importMutex_};
    for (auto &rank : importedRanks_) {
        if (options_.role != HYBM_ROLE_PEER && static_cast<hybm_role_type>(rank.second.role) == options_.role) {
            continue;
        }

        transOptions.options[rank.first].role = static_cast<hybm_role_type>(rank.second.role);
        transOptions.options[rank.first].nic = rank.second.nic;
    }
    for (auto &mr : importedMemories_) {
        auto pos = transOptions.options.find(mr.first);
        if (pos != transOptions.options.end()) {
            for (auto &key : mr.second) {
                pos->second.memKeys.emplace_back(key.second);
            }
        }
    }
    uniqueLock.unlock();

    if (options_.role != HYBM_ROLE_PEER || importInfoEntity) {
        ret = transportManager_->ConnectWithOptions(transOptions);
        if (ret != 0) {
            BM_LOG_ERROR("Transport Manager ConnectWithOptions failed: " << ret);
            return ret;
        }
        if (importInfoEntity) {
            return UpdateHybmDeviceInfo(0);
        }
    }

    return BM_OK;
}

Result MemEntityDefault::InitSegment()
{
    switch (options_.memType) {
        case HYBM_MEM_TYPE_DEVICE: return InitHbmSegment();
        case HYBM_MEM_TYPE_HOST: return InitDramSegment();
        default: return BM_INVALID_PARAM;
    }
}

Result MemEntityDefault::InitHbmSegment()
{
    MemSegmentOptions segmentOptions;
    if (options_.globalUniqueAddress) {
        segmentOptions.size = options_.singleRankVASpace;
        segmentOptions.segType = HYBM_MST_HBM;
        BM_LOG_INFO("create entity global unified memory space.");
    } else {
        segmentOptions.segType = HYBM_MST_HBM_USER;
        BM_LOG_INFO("create entity user defined memory space.");
    }
    if (options_.globalUniqueAddress && options_.singleRankVASpace == 0) {
        BM_LOG_INFO("Hbm rank space is zero.");
        return BM_OK;
    }

    segmentOptions.devId = HybmGetInitDeviceId();
    segmentOptions.role = options_.role;
    segmentOptions.dataOpType = options_.bmDataOpType;
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;
    segment_ = MemSegment::Create(segmentOptions, id_);
    BM_VALIDATE_RETURN(segment_ != nullptr, "create segment failed", BM_INVALID_PARAM);

    return MemSegmentDevice::SetDeviceInfo(HybmGetInitDeviceId());
}

Result MemEntityDefault::InitDramSegment()
{
    if (options_.singleRankVASpace == 0) {
        BM_LOG_INFO("Dram rank space is zero.");
        return BM_OK;
    }

    MemSegmentOptions segmentOptions;
    segmentOptions.size = options_.singleRankVASpace;
    segmentOptions.devId = HybmGetInitDeviceId();
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
    uint32_t hostTransFlags = HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_TCP;
    uint32_t composeTransFlags = HYBM_DOP_TYPE_DEVICE_RDMA | hostTransFlags;
    if ((options_.bmDataOpType & composeTransFlags) == 0) {
        BM_LOG_DEBUG("NO RDMA Data Operator transport skip init.");
        return BM_OK;
    }

    if ((options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) && (options_.bmDataOpType & hostTransFlags)) {
        transportManager_ = transport::TransportManager::Create(TransportType::TT_COMPOSE);
    } else if (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        transportManager_ = transport::TransportManager::Create(TransportType::TT_HCCP);
    } else {
        transportManager_ = transport::TransportManager::Create(TransportType::TT_HCOM);
    }

    transport::TransportOptions options;
    options.rankId = options_.rankId;
    options.rankCount = options_.rankCount;
    options.protocol = options_.bmDataOpType;
    options.role = options_.role;
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
    if (options_.bmType == HYBM_TYPE_AI_CORE_INITIATE) {
        return BM_OK;
    }

    if (options_.bmDataOpType & HYBM_DOP_TYPE_SDMA) {
        sdmaDataOperator_ = std::make_shared<HostDataOpSDMA>();
        auto ret = sdmaDataOperator_->Initialize();
        if (ret != BM_OK) {
            BM_LOG_ERROR("SDMA data operator init failed, ret:" << ret);
            return ret;
        }
    }

    if (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        devRdmaDataOperator_ = std::make_shared<DataOpDeviceRDMA>(options_.rankId, transportManager_);
        auto ret = devRdmaDataOperator_->Initialize();
        if (ret != BM_OK) {
            BM_LOG_ERROR("Device RDMA data operator init failed, ret:" << ret);
            return ret;
        }
    }
    if (options_.bmDataOpType & (HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_TCP)) {
        hostRdmaDataOperator_ = std::make_shared<HostDataOpRDMA>(options_.rankId, transportManager_);
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
    if (segment_ == nullptr) {
        BM_LOG_ERROR("SdmaReaches segment is null");
        return false;
    }

    return segment_->CheckSmdaReaches(remoteRank);
}

hybm_data_op_type MemEntityDefault::CanReachDataOperators(uint32_t remoteRank) const noexcept
{
    uint32_t supportDataOp = 0U;
    bool sdmaReach = SdmaReaches(remoteRank);   // SDMA reaches mean MTE reaches too
    if (sdmaReach) {
        supportDataOp |= HYBM_DOP_TYPE_MTE;
    }

    if (sdmaDataOperator_ != nullptr && sdmaReach) {
        supportDataOp |= HYBM_DOP_TYPE_SDMA;
    }

    if (devRdmaDataOperator_ != nullptr || (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) != 0) {
        supportDataOp |= HYBM_DOP_TYPE_DEVICE_RDMA;
    }

    if (hostRdmaDataOperator_ != nullptr) {
        supportDataOp |= HYBM_DOP_TYPE_HOST_RDMA;
    }

    return static_cast<hybm_data_op_type>(supportDataOp);
}

void MemEntityDefault::ReleaseResources()
{
    if (!initialized) {
        return;
    }
    if (transportManager_) {
        transportManager_->CloseDevice();
        transportManager_ = nullptr;
    }
    segment_.reset();
    sdmaDataOperator_.reset();
    devRdmaDataOperator_.reset();
    hostRdmaDataOperator_.reset();
    initialized = false;
}

}  // namespace mf
}  // namespace ock