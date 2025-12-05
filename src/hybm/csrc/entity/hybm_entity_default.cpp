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
#include "hybm_entity_default.h"

#include <algorithm>

#include "dl_api.h"
#include "dl_acl_api.h"
#include "dl_hal_api.h"
#include "hybm_data_op_host_rdma.h"
#include "hybm_dev_legacy_segment.h"
#include "hybm_data_op_sdma.h"
#include "hybm_data_op_device_rdma.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_gva.h"
#include "hybm_logger.h"
#include "hybm_stream_manager.h"

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

void MemEntityDefault::UnInitialize() noexcept
{
    ReleaseResources();
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
        if (sdmaDataOperator_) {
            sdmaDataOperator_->UpdateGvaSpace(HYBM_MEM_TYPE_DEVICE, (uint64_t)hbmGva_, options_.deviceVASpace,
                                              options_.rankCount);
        }
        if (devRdmaDataOperator_) {
            devRdmaDataOperator_->UpdateGvaSpace(HYBM_MEM_TYPE_DEVICE, (uint64_t)hbmGva_, options_.deviceVASpace,
                                                 options_.rankCount);
        }
        if (hostRdmaDataOperator_) {
            hostRdmaDataOperator_->UpdateGvaSpace(HYBM_MEM_TYPE_DEVICE, (uint64_t)hbmGva_, options_.deviceVASpace,
                                                  options_.rankCount);
        }
    }

    if (dramSegment_ != nullptr) {
        auto ret = dramSegment_->ReserveMemorySpace(&dramGva_);
        if (ret != BM_OK) {
            UnReserveMemorySpace();
            BM_LOG_ERROR("Failed to reserver DRAM memory space ret: " << ret);
            return ret;
        }
        if (sdmaDataOperator_) {
            sdmaDataOperator_->UpdateGvaSpace(HYBM_MEM_TYPE_HOST, (uint64_t)dramGva_, options_.hostVASpace,
                                              options_.rankCount);
        }
        if (devRdmaDataOperator_) {
            devRdmaDataOperator_->UpdateGvaSpace(HYBM_MEM_TYPE_HOST, (uint64_t)dramGva_, options_.hostVASpace,
                                                 options_.rankCount);
        }
        if (hostRdmaDataOperator_) {
            hostRdmaDataOperator_->UpdateGvaSpace(HYBM_MEM_TYPE_HOST, (uint64_t)dramGva_, options_.hostVASpace,
                                                  options_.rankCount);
        }
    }

    return BM_OK;
}

int32_t MemEntityDefault::UnReserveMemorySpace() noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    if (hbmSegment_ != nullptr) {
        hbmSegment_->UnReserveMemorySpace();
    }
    if (dramSegment_ != nullptr) {
        dramSegment_->UnReserveMemorySpace();
    }
    return BM_OK;
}

int32_t MemEntityDefault::AllocLocalMemory(uint64_t size, hybm_mem_type mType, uint32_t flags,
                                           hybm_mem_slice_t &slice) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    if ((size % HYBM_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("allocate memory size: " << size << " invalid, page size is: " << HYBM_LARGE_PAGE_SIZE);
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
    info.flags =
        segment->GetMemoryType() == HYBM_MEM_TYPE_DEVICE ? transport::REG_MR_FLAG_HBM : transport::REG_MR_FLAG_DRAM;
    if (transportManager_ != nullptr) {
        ret = transportManager_->RegisterMemoryRegion(info);
        if (ret != 0) {
            BM_LOG_ERROR("register memory region allocate failed: " << ret << ", info: " << info);
            auto res = segment->ReleaseSliceMemory(realSlice);
            if (res != BM_OK) {
                BM_LOG_ERROR("failed to release slice memory: " << res);
            }
            return ret;
        }
    }

    return UpdateHybmDeviceInfo(0);
}

int32_t MemEntityDefault::RegisterLocalMemory(const void *ptr, uint64_t size, uint32_t flags,
                                              hybm_mem_slice_t &slice) noexcept
{
    if (ptr == nullptr || size == 0 || size > TB) {
        BM_LOG_ERROR("input ptr or size(" << size << ") is invalid");
        return BM_INVALID_PARAM;
    }
    if ((size % HYBM_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("input size: " << size << " invalid, page size is: " << HYBM_LARGE_PAGE_SIZE);
        return BM_INVALID_PARAM;
    }

    auto addr = static_cast<uint64_t>(reinterpret_cast<ptrdiff_t>(ptr));
    bool isHbm = (addr >= HYBM_HBM_START_ADDR && addr < HYBM_HBM_END_ADDR);
    std::shared_ptr<MemSegment> segment = nullptr;
    // 只有trans场景才需要走hbmSegment_，bm场景优先走dramSegment_
    if (!options_.globalUniqueAddress || dramSegment_ == nullptr) {
        segment = hbmSegment_;
    } else {
        segment = dramSegment_;
    }
    BM_VALIDATE_RETURN(segment != nullptr, "address for segment is null.", BM_INVALID_PARAM);

    std::shared_ptr<MemSlice> realSlice;
    auto ret = segment->RegisterMemory(ptr, size, realSlice);
    if (ret != 0) {
        BM_LOG_ERROR("segment register slice with size: " << size << " failed: " << ret);
        return ret;
    }

    if (transportManager_ != nullptr) {
        transport::TransportMemoryRegion mr;
        mr.addr = (uint64_t)(ptrdiff_t)ptr;
        mr.size = size;
        mr.flags = (isHbm ? transport::REG_MR_FLAG_HBM : transport::REG_MR_FLAG_DRAM);
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

    std::shared_ptr<MemSlice> memSlice;
    if (hbmSegment_ != nullptr && (memSlice = hbmSegment_->GetMemSlice(slice)) != nullptr) {
        hbmSegment_->ReleaseSliceMemory(memSlice);
    }
    if (dramSegment_ != nullptr && (memSlice = dramSegment_->GetMemSlice(slice)) != nullptr) {
        dramSegment_->ReleaseSliceMemory(memSlice);
    }

    if (transportManager_ != nullptr && memSlice != nullptr) {
        auto ret = transportManager_->UnregisterMemoryRegion(memSlice->vAddress_);
        if (ret != BM_OK) {
            BM_LOG_ERROR("UnregisterMemoryRegion failed, please check input slice.");
        }
    }
    return BM_OK;
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

int32_t MemEntityDefault::ExportWithoutSlice(ExchangeInfoWriter &desc, uint32_t flags)
{
    std::string info;
    int32_t ret = BM_ERROR;
    if (hbmSegment_ != nullptr) {
        ret = hbmSegment_->Export(info);
    }
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
        BM_LOG_ERROR("export to string wrong size: " << info.size());
        return BM_ERROR;
    }

    return BM_OK;
}

int32_t MemEntityDefault::ExportExchangeInfo(hybm_mem_slice_t slice, ExchangeInfoWriter &desc, uint32_t flags) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized, "the entity is not initialized", BM_NOT_INITIALIZED);
    if (slice == nullptr) {
        return ExportWithoutSlice(desc, flags);
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

    if (transportManager_ != nullptr) {
        SliceExportTransportKey transportKey{exportMagic, options_.rankId, realSlice->vAddress_};
        ret = transportManager_->QueryMemoryKey(realSlice->vAddress_, transportKey.key);
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

    ret = desc.Append(info.data(), info.size());
    if (ret != 0) {
        BM_LOG_ERROR("export to string wrong size: " << info.size());
        return BM_ERROR;
    }

    return BM_OK;
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

    std::unordered_map<uint32_t, std::vector<transport::TransportMemoryKey>> tempKeyMap;
    ret = ImportForTransport(desc, count);
    if (ret != BM_OK) {
        return ret;
    }

    if (desc[0].LeftBytes() == 0) {
        BM_LOG_INFO("no segment need import.");
        return BM_OK;
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

    ret = currentSegment->Import(infos, addresses);
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
        prepareInfo.role = static_cast<hybm_role_type>(info.role);
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

    BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to Connect transport: " << ret, ret);
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
        std::unique_lock<std::mutex> uniqueLock{importMutex_};
        for (auto rank : ranks) {
            importedRanks_.erase(rank);
            importedMemories_.erase(rank);
        }
        uniqueLock.unlock();
        auto ret = transportManager_->RemoveRanks(ranks);
        if (ret != BM_OK) {
            BM_LOG_WARN("transport remove ranks failed: " << ret);
        }
    }

    return BM_OK;
}

int32_t MemEntityDefault::GetExportSliceInfoSize(size_t &size) noexcept
{
    size_t exportSize = 0;
    auto segment = hbmSegment_ == nullptr ? dramSegment_ : hbmSegment_;
    if (segment == nullptr) {
        BM_LOG_ERROR("segment is null.");
        return BM_ERROR;
    }

    auto ret = segment->GetExportSliceSize(exportSize);
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

int32_t MemEntityDefault::CopyData(hybm_copy_params &params, hybm_data_copy_direction direction, void *stream,
                                   uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    int32_t ret = BM_OK;
    if (stream == nullptr) {
        stream = HybmStreamManager::GetThreadAclStream(HybmGetInitDeviceId());
    }

    ExtOptions options{};
    options.flags = flags;
    options.stream = stream;
    GenCopyExtOption(params.src, params.dest, params.dataSize, options);

    if (options_.globalUniqueAddress) {
        params.src = Valid48BitsAddress(params.src);
        params.dest = Valid48BitsAddress(params.dest);
    }

    if ((options_.bmDataOpType & HYBM_DOP_TYPE_SDMA) != 0 && sdmaDataOperator_ != nullptr) {
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
    if (stream == nullptr) {
        stream = HybmStreamManager::GetThreadAclStream(HybmGetInitDeviceId());
    }

    ExtOptions options{};
    options.flags = flags;
    options.stream = stream;
    GenCopyExtOption(params.sources[0], params.destinations[0], params.dataSizes[0], options);

    if (!options_.globalUniqueAddress) {
        for (auto i = 0U; i < params.batchSize; i++) {
            params.sources[i] = Valid48BitsAddress(params.sources[i]);
            params.destinations[i] = Valid48BitsAddress(params.destinations[i]);
        }
    }

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
    if (devRdmaDataOperator_ != nullptr) {
        return devRdmaDataOperator_->Wait(0);
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

    return BM_OK;
}

int MemEntityDefault::LoadExtendLibrary() noexcept
{
    if (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        auto ret = DlApi::LoadExtendLibrary(DlApiExtendLibraryType::DL_EXT_LIB_DEVICE_RDMA);
        if (ret != 0) {
            BM_LOG_ERROR("LoadExtendLibrary for DEVICE RDMA failed: " << ret);
            return ret;
        }
    }

    if (options_.bmDataOpType & (HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_TCP)) {
        auto ret = DlApi::LoadExtendLibrary(DlApiExtendLibraryType::DL_EXT_LIB_HOST_RDMA);
        if (ret != 0) {
            BM_LOG_ERROR("LoadExtendLibrary for HOST RDMA failed: " << ret);
            return ret;
        }
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
    auto ret = DlAclApi::AclrtMemcpy((void *)addr, HYBM_LARGE_PAGE_SIZE, &info, sizeof(HybmDeviceMeta),
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
    info.symmetricSize = options_.deviceVASpace;
    info.extraContextSize = 0;
    if (transportManager_ != nullptr) {
        info.qpInfoAddress = (uint64_t)(ptrdiff_t)transportManager_->GetQpInfo();
    } else {
        info.qpInfoAddress = 0UL;
    }
}

int32_t MemEntityDefault::ImportForTransportPrecheck(const ExchangeInfoReader desc[], uint32_t &count,
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

        if (magic == ENTITY_EXPORT_INFO_MAGIC) {
            ret = desc[i].Read(entityExportInfo);
            if (ret == 0) {
                importedRanks_[entityExportInfo.rankId] = entityExportInfo;
                importInfoEntity = true;
            }
        } else if (magic == DRAM_SLICE_EXPORT_INFO_MAGIC || magic == HBM_SLICE_EXPORT_INFO_MAGIC) {
            ret = desc[i].Read(transportKey);
            if (ret == 0) {
                std::unique_lock<std::mutex> uniqueLock{importMutex_};
                importedMemories_[transportKey.rankId].emplace_back(transportKey.key);
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

int32_t MemEntityDefault::ImportForTransport(const ExchangeInfoReader desc[], uint32_t count) noexcept
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
                pos->second.memKeys.emplace_back(key);
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

void MemEntityDefault::GenCopyExtOption(void* &src, void* &dest, uint64_t length, ExtOptions &options) noexcept
{
    void *real = Valid48BitsAddress(src);
    if (dramSegment_ != nullptr && dramSegment_->GetRankIdByAddr(src, length, options.srcRankId)) {
        // nothing
    } else if (hbmSegment_ != nullptr && hbmSegment_->GetRankIdByAddr(src, length, options.srcRankId)) {
        // nothing
    } else {
        options.srcRankId = options_.rankId;
    }
    src = real;

    real = Valid48BitsAddress(dest);
    if (dramSegment_ != nullptr && dramSegment_->GetRankIdByAddr(dest, length, options.destRankId)) {
        // nothing
    } else if (hbmSegment_ != nullptr && hbmSegment_->GetRankIdByAddr(dest, length, options.destRankId)) {
        // nothing
    } else {
        options.destRankId = options_.rankId;
    }
    dest = real;
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
    if (options_.globalUniqueAddress && options_.deviceVASpace == 0) {
        BM_LOG_INFO("Hbm rank space is zero.");
        return BM_OK;
    }

    MemSegmentOptions segmentOptions;
    if (options_.globalUniqueAddress) {
        segmentOptions.size = options_.deviceVASpace;
        segmentOptions.segType = HYBM_MST_HBM;
        BM_LOG_INFO("create entity global unified memory space.");
    } else {
        segmentOptions.segType = HYBM_MST_HBM_USER;
        BM_LOG_INFO("create entity user defined memory space.");
    }
    segmentOptions.devId = HybmGetInitDeviceId();
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;
    segmentOptions.dataOpType = options_.bmDataOpType;
    hbmSegment_ = MemSegment::Create(segmentOptions, id_);
    if (hbmSegment_ == nullptr) {
        BM_LOG_ERROR("Failed to create hbm segment");
        return BM_ERROR;
    }
    return HybmDevLegacySegment::SetDeviceInfo(HybmGetInitDeviceId());
}

Result MemEntityDefault::InitDramSegment()
{
    if (options_.hostVASpace == 0) {
        BM_LOG_INFO("Dram rank space is zero.");
        return BM_OK;
    }

    MemSegmentOptions segmentOptions;
    segmentOptions.size = options_.hostVASpace;
    segmentOptions.devId = HybmGetInitDeviceId();
    segmentOptions.segType = HYBM_MST_DRAM;
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;
    segmentOptions.dataOpType = options_.bmDataOpType;
    if (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        segmentOptions.shared = false;
    }
    dramSegment_ = MemSegment::Create(segmentOptions, id_);
    if (dramSegment_ == nullptr) {
        BM_LOG_ERROR("Failed to create dram segment");
        return BM_ERROR;
    }

    return HybmDevLegacySegment::SetDeviceInfo(HybmGetInitDeviceId());
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

    transport::TransportOptions options{};
    options.rankId = options_.rankId;
    options.rankCount = options_.rankCount;
    options.protocol = options_.bmDataOpType;
    options.role = options_.role;
    options.initialType = options_.bmType;
    options.nic = options_.transUrl;
    options.tlsOption = options_.tlsOption;
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

void MemEntityDefault::ReleaseResources()
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
    initialized = false;
}
} // namespace mf
} // namespace ock
