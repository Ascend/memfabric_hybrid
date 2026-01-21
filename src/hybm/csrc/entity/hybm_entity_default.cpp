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
#include "host_hcom_common.h"
#include "hybm_dev_legacy_segment.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_gva.h"
#include "hybm_logger.h"
#include "hybm_stream_manager.h"
#include "hybm_va_manager.h"
#include "hybm_compose_data_op.h"

namespace ock {
namespace mf {

thread_local bool MemEntityDefault::isSetDevice_ = false;

MemEntityDefault::MemEntityDefault(int id) noexcept : id_(id), initialized_(false) {}

MemEntityDefault::~MemEntityDefault()
{
    ReleaseResources();
}

Result MemEntityDefault::InitTagManager()
{
    if (options_.scene == HYBM_SCENE_TRANS) {
        return BM_OK;
    }
    const static std::string defaultTag = "HYBM_DEFAULT_TAG_FOR_EMPTY";
    BM_ASSERT_RETURN(tagManager_ == nullptr, BM_OK);
    std::string localTag = options_.tag;
    if (localTag.empty()) {
        localTag = defaultTag;
        std::copy_n(localTag.c_str(), localTag.size(), options_.tag);
        if (sizeof(options_.tag) > defaultTag.size()) {
            options_.tag[defaultTag.size()] = '\0';
        }
    }
    tagManager_ = std::make_shared<HybmEntityTagInfo>();
    BM_ASSERT_LOG_AND_RETURN(tagManager_->TagInfoInit(options_) == BM_OK, "Failed to init tagManager",
                             BM_INVALID_PARAM);
    // same tag use options_.bmDataOpType
    std::ostringstream compatibleInfo;
    if (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        compatibleInfo << localTag << ":" << HybmEntityTagInfo::GetOpTypeStr(HYBM_DOP_TYPE_DEVICE_RDMA) << ":"
                       << localTag << ",";
    }
    if (options_.bmDataOpType & HYBM_DOP_TYPE_SDMA) {
        compatibleInfo << localTag << ":" << HybmEntityTagInfo::GetOpTypeStr(HYBM_DOP_TYPE_SDMA) << ":" << localTag
                       << ",";
    }
    if (options_.bmDataOpType & HYBM_DOP_TYPE_HOST_RDMA) {
        compatibleInfo << localTag << ":" << HybmEntityTagInfo::GetOpTypeStr(HYBM_DOP_TYPE_HOST_RDMA) << ":" << localTag
                       << ",";
    }
    if (options_.bmDataOpType & HYBM_DOP_TYPE_HOST_TCP) {
        compatibleInfo << localTag << ":" << HybmEntityTagInfo::GetOpTypeStr(HYBM_DOP_TYPE_HOST_TCP) << ":" << localTag
                       << ",";
    }
    if (options_.bmDataOpType & HYBM_DOP_TYPE_HOST_URMA) {
        compatibleInfo << localTag << ":" << HybmEntityTagInfo::GetOpTypeStr(HYBM_DOP_TYPE_HOST_URMA) << ":" << localTag
                       << ",";
    }
    BM_ASSERT_LOG_AND_RETURN(tagManager_->AddTagOpInfo(compatibleInfo.str()) == BM_OK,
                             "Failed to add tagOpInfo:" << compatibleInfo.str(), BM_INVALID_PARAM);
    options_.bmDataOpType = static_cast<hybm_data_op_type>(tagManager_->GetAllOpType());
    BM_LOG_INFO("Success to init tag manager data op type:" << options_.bmDataOpType);
    return BM_OK;
}

int32_t MemEntityDefault::Initialize(const hybm_options *options) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(!initialized_, "the object is initialized.", BM_OK);
    BM_ASSERT_LOG_AND_RETURN((id_ >= 0 && (uint32_t)(id_) < HYBM_ENTITY_NUM_MAX),
                             "input entity id is invalid, input: " << id_
                                                                   << " must be less than: " << HYBM_ENTITY_NUM_MAX,
                             BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(CheckOptions(options) == BM_OK, "options is invalid.", BM_INVALID_PARAM);
    options_ = *options;
    // init tag info
    BM_ASSERT_LOG_AND_RETURN(InitTagManager() == BM_OK, "Failed to init tag manager.", BM_ERROR);
    // load dlopen lib
    BM_ASSERT_LOG_AND_RETURN(LoadExtendLibrary() == BM_OK, "Load extend library failed.", BM_ERROR);
    // init segment
    BM_ASSERT_LOG_AND_RETURN(InitSegment() == BM_OK, "Failed to initSegment.", BM_ERROR);
    // init transManager
    BM_ASSERT_LOG_AND_RETURN(InitTransManager() == BM_OK, "Failed to InitTransManager", BM_ERROR);
    // init dataOperator
    BM_ASSERT_LOG_AND_RETURN(InitDataOperator() == BM_OK, "Failed to InitTransManager", BM_ERROR);

    initialized_ = true;
    return BM_OK;
}

void MemEntityDefault::UnInitialize() noexcept
{
    ReleaseResources();
}

int32_t MemEntityDefault::ReserveMemorySpace() noexcept
{
    if (!initialized_) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    if (hbmSegment_ != nullptr) {
        auto ret = hbmSegment_->ReserveMemorySpace(&hbmGva_);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to reserver HBM memory space ret: " << ret);
            return ret;
        }
        if (dataOperator_) {
            dataOperator_->UpdateGvaSpace(HYBM_MEM_TYPE_DEVICE, (uint64_t)hbmGva_, options_.maxHBMSize,
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
        if (dataOperator_) {
            dataOperator_->UpdateGvaSpace(HYBM_MEM_TYPE_HOST, (uint64_t)dramGva_, options_.maxDRAMSize,
                                          options_.rankCount);
        }
    }

    return BM_OK;
}

int32_t MemEntityDefault::UnReserveMemorySpace() noexcept
{
    if (!initialized_) {
        BM_LOG_WARN("the object is not initialized, please check whether Initialize is called.");
        return BM_OK;
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
    if (!initialized_) {
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

    MemSlicePtr realSlice;
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
    if (transportManager_ != nullptr && size > 0) {
        info.flags |= transport::REG_MR_FLAG_SELF;
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

    auto addr = static_cast<uint64_t>(reinterpret_cast<ptrdiff_t>(ptr));
    bool isHbm = (addr >= HYBM_HBM_START_ADDR && addr < HYBM_HBM_END_ADDR);
    BM_LOG_INFO("Hbm: " << isHbm << std::hex << ", addrs: 0x" << addr << ", start: 0x" << HYBM_HBM_START_ADDR
                        << ", end: 0x" << HYBM_HBM_END_ADDR);
    std::shared_ptr<MemSegment> segment = nullptr;
    // 只有trans场景才需要走hbmSegment_，bm场景优先走dramSegment_
    if (options_.scene == HYBM_SCENE_TRANS || dramSegment_ == nullptr) {
        segment = hbmSegment_;
    } else {
        segment = dramSegment_;
    }
    BM_VALIDATE_RETURN(segment != nullptr, "address for segment is null.", BM_INVALID_PARAM);

    MemSlicePtr realSlice;
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
    if (!initialized_) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_INVALID_PARAM;
    }
    HybmVaManager::GetInstance().DumpReservedGvaInfo();
    HybmVaManager::GetInstance().DumpAllocatedGvaInfo();
    MemSlicePtr memSlice;
    if (hbmSegment_ != nullptr && (memSlice = hbmSegment_->GetMemSlice(slice, true)) != nullptr) {
        hbmSegment_->ReleaseSliceMemory(memSlice);
    } else if (dramSegment_ != nullptr && (memSlice = dramSegment_->GetMemSlice(slice)) != nullptr) {
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

void *MemEntityDefault::GetSliceVa(hybm_mem_slice_t slice)
{
    std::shared_ptr<MemSlice> memSlice;
    if (hbmSegment_ != nullptr && (memSlice = hbmSegment_->GetMemSlice(slice, true)) != nullptr) {
        return reinterpret_cast<void *>(memSlice->vAddress_);
    } else if (dramSegment_ != nullptr && (memSlice = dramSegment_->GetMemSlice(slice)) != nullptr) {
        return reinterpret_cast<void *>(memSlice->vAddress_);
    }

    BM_LOG_ERROR("failed to get slice va, invalid slice:" << slice);
    return nullptr;
}

int32_t MemEntityDefault::ExportExchangeInfo(ExchangeInfoWriter &desc, uint32_t flags) noexcept
{
    if (!initialized_) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    std::string info;
    EntityExportInfo exportInfo{};
    exportInfo.version = EXPORT_INFO_VERSION;
    exportInfo.rankId = options_.rankId;
    exportInfo.role = static_cast<uint16_t>(options_.role);
    std::copy_n(options_.tag, strlen(options_.tag), exportInfo.tag);
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

    if (flags & HYBM_FLAG_EXPORT_ENTITY) {
        ret = ExportExchangeInfo(desc, flags);
        if (ret != 0) {
            BM_LOG_ERROR("ExportExchangeInfo failed: " << ret);
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

int32_t MemEntityDefault::ExportExchangeInfo(hybm_mem_slice_t slice, ExchangeInfoWriter &desc, uint32_t flags) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(initialized_, "the entity is not initialized", BM_NOT_INITIALIZED);
    if (slice == nullptr) {
        return ExportWithoutSlice(desc, flags);
    }

    uint64_t exportMagic = 0;
    std::string info;
    MemSlicePtr realSlice;
    std::shared_ptr<MemSegment> currentSegment;
    if (hbmSegment_ != nullptr) {
        realSlice = hbmSegment_->GetMemSlice(slice, true);
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
        if (realSlice->size_ > 0) {
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
    }

    if (options_.scene != HYBM_SCENE_TRANS) {
        BM_LOG_INFO("Success to export slice rankId:" << transportKey.rankId << " addr:" << transportKey.address <<
                    " key:" << transportKey.key);
    } else {
        BM_LOG_INFO("Success to export slice rankId:" << options_.rankId << " addr:" << realSlice->vAddress_);
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
    if (!initialized_) {
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

    if (transportManager_ != nullptr) {
        std::unordered_map<uint32_t, std::vector<transport::TransportMemoryKey>> tempKeyMap;
        ret = ImportForTransport(desc, count);
        if (ret != BM_OK) {
            return ret;
        }
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

    auto currentSegment = IsDramSlice(magic) ? dramSegment_ : hbmSegment_;
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

int32_t MemEntityDefault::ImportForTagManager()
{
    if (options_.scene == HYBM_SCENE_TRANS) {
        return BM_OK;
    }
    for (const auto &item : importedRanks_) {
        auto rankId = item.first;
        auto &info = item.second;
        auto ret = tagManager_->AddRankTag(rankId, info.tag);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to add rankTag rank:" << rankId << " tag:" << info.tag);
            return ret;
        }
    }
    return BM_OK;
}

int32_t MemEntityDefault::ImportForTransportManager()
{
    if (transportManager_ == nullptr) {
        BM_LOG_INFO("no transport, no need import.");
        return BM_OK;
    }
    int32_t ret = BM_ERROR;
    transport::HybmTransPrepareOptions prepareOptions;
    for (const auto &item : importedRanks_) {
        auto &info = item.second;
        transport::TransportRankPrepareInfo prepareInfo;
        prepareInfo.nic = info.nic;
        prepareInfo.role = static_cast<hybm_role_type>(info.role);
        prepareOptions.options.emplace(info.rankId, prepareInfo);
    }

    if (transportPrepared_) {
        ret = transportManager_->UpdateRankOptions(prepareOptions);
    } else {
        ret = transportManager_->Prepare(prepareOptions);
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to prepare transport connect data, ret: " << ret);
            return ret;
        }
        ret = transportManager_->Connect();
        if (ret != BM_OK) {
            BM_LOG_ERROR("Failed to prepare transport connect, ret: " << ret);
            return ret;
        }
    }

    BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "Failed to Connect transport: " << ret, ret);
    transportPrepared_ = true;
    return 0;
}

int32_t MemEntityDefault::ImportEntityExchangeInfo(const ExchangeInfoReader desc[], uint32_t count,
                                                   uint32_t flags) noexcept
{
    BM_ASSERT_RETURN(initialized_, BM_NOT_INITIALIZED);
    BM_ASSERT_RETURN(desc != nullptr, BM_INVALID_PARAM);

    std::vector<EntityExportInfo> deserializedInfos(count);
    for (auto i = 0U; i < count; i++) {
        auto ret = desc[i].Read(deserializedInfos[i]);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
    }
    {
        std::unique_lock<std::mutex> uniqueLock{importMutex_};
        for (auto &deserializedInfo : deserializedInfos) {
            importedRanks_[deserializedInfo.rankId] = deserializedInfo;
        }
    }
    BM_ASSERT_LOG_AND_RETURN(ImportForTagManager() == BM_OK, "Failed import for tag manager", BM_ERROR);
    BM_ASSERT_LOG_AND_RETURN(ImportForTransportManager() == BM_OK, "Failed import for transport manager", BM_ERROR);
    return BM_OK;
}

int32_t MemEntityDefault::ImportExchangeInfo(const hybm_exchange_info allExInfo[], uint32_t count, void *addresses[],
                                             uint32_t flags) noexcept
{
    if (!initialized_) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    std::vector<ExchangeInfoReader> readers(count);
    for (auto i = 0U; i < count; i++) {
        readers[i].Reset(allExInfo + i);
    }
    auto desc = readers.data();
    if (desc == nullptr) {
        BM_LOG_ERROR("the input desc is nullptr.");
        return BM_ERROR;
    }

    return ImportExchangeInfo(desc, count, addresses, flags);
}

int32_t MemEntityDefault::SetExtraContext(const void *context, uint32_t size) noexcept
{
    if (!initialized_) {
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
    if (!initialized_) {
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
    if (!initialized_) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    // in trans scene, two segement work togather, but hbm do not need to mmap, and wrongly mmap will make dram mmap fail
    if (hbmSegment_ != nullptr && options_.scene != HYBM_SCENE_TRANS) {
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
    if (!initialized_) {
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

    if (dataOperator_ != nullptr) {
        dataOperator_->CleanUp();
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
    if (!initialized_) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }
    BM_ASSERT_RETURN(SetThreadAclDevice() == BM_OK, BM_ERROR);

    int32_t ret = BM_OK;
    ExtOptions options{};
    options.flags = flags;
    options.stream = stream;
    GenCopyExtOption(params.src, params.dest, params.dataSize, options);

    if (options_.scene != HYBM_SCENE_TRANS) {
        params.src = Valid48BitsAddress(params.src);
        params.dest = Valid48BitsAddress(params.dest);
    }

    ret = dataOperator_->DataCopy(params, direction, options);
    if (ret != BM_OK) {
        BM_LOG_ERROR("failed to copy data ret:" << ret);
    }
    return ret;
}

int32_t MemEntityDefault::BatchCopyData(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                        void *stream, uint32_t flags) noexcept
{
    if (!initialized_) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }
    BM_ASSERT_RETURN(SetThreadAclDevice() == BM_OK, BM_ERROR);

    int32_t ret = BM_ERROR;
    // 0. sdma 不需要rankId
    ExtOptions sOptions{};
    sOptions.stream = stream;
    sOptions.flags = flags;
    // 1. 分组
    std::unordered_map<ExtOptions, std::vector<uint32_t>, ExtOptionsHash> groupMap;
    // 2. 遍历所有元素，收集分组
    for (uint32_t i = 0; i < params.batchSize; ++i) {
        ExtOptions options{};
        options.flags = flags;
        options.stream = stream;
        GenCopyExtOption(params.sources[i], params.destinations[i], params.dataSizes[i], options);
        BM_LOG_DEBUG("==========" << options << " " << (uint64_t)params.sources[i] << " " << params.destinations[i]
                                  << " " << (uint64_t)params.dataSizes[i]);
        groupMap[options].push_back(i);
    }
    // 3. 为每组调用batch_copy
    for (auto &[options, indices] : groupMap) {
        uint32_t groupSize = indices.size();
        // 为当前组构建临时参数
        std::vector<void *> sources_group(groupSize);
        std::vector<void *> destinations_group(groupSize);
        std::vector<size_t> dataSizes_group(groupSize);
        // 填充组内参数
        for (uint32_t j = 0; j < groupSize; ++j) {
            uint32_t idx = indices[j];
            sources_group[j] = params.sources[idx];
            destinations_group[j] = params.destinations[idx];
            dataSizes_group[j] = params.dataSizes[idx];
        }
        hybm_batch_copy_params copyParams = {sources_group.data(), destinations_group.data(), dataSizes_group.data(),
                                             groupSize};
        if (dataOperator_ != nullptr) {
            ret = dataOperator_->BatchDataCopy(copyParams, direction, options);
            if (ret == BM_OK) {
                continue;
            }
            BM_LOG_ERROR("Data copy direction: " << direction << ", failed : " << ret);
            return ret;
        }
    }

    return ret;
}

int32_t MemEntityDefault::Wait() noexcept
{
    if (!initialized_) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }
    return dataOperator_->Wait(0);
}

bool MemEntityDefault::CheckAddressInEntity(const void *ptr, uint64_t length) const noexcept
{
    if (!initialized_) {
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
    if (options_.bmDataOpType & (HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA | HYBM_DOP_TYPE_HOST_TCP)) {
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
            if (ret != BM_OK) {
                BM_LOG_ERROR("read info for transport failed: " << ret);
                return ret;
            }
            std::unique_lock<std::mutex> uniqueLock{importMutex_};
            if (options_.globalUniqueAddress) {
                // smem_bm每一次import都加载全量信息
                importedMemories_[transportKey.rankId].clear();
            }
            importedMemories_[transportKey.rankId].emplace_back(transportKey.key);
            BM_LOG_INFO("Success to import slice rankId:" << transportKey.rankId << " addr:" << std::hex
                                                          << transportKey.address);
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
    if (importInfoEntity) {
        BM_ASSERT_LOG_AND_RETURN(ImportForTagManager() == BM_OK, "Failed import for tag manager", BM_ERROR);
    }
    if (transportManager_ != nullptr) {
        ret = transportManager_->ConnectWithOptions(transOptions);
        if (ret != 0) {
            BM_LOG_ERROR("Transport Manager ConnectWithOptions failed: " << ret);
            return ret;
        }
    }
    if (importInfoEntity) {
        return UpdateHybmDeviceInfo(0);
    }
    return BM_OK;
}

void MemEntityDefault::GenCopyExtOption(void *&src, void *&dest, uint64_t length, ExtOptions &options) noexcept
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

        if (options_.scene == HYBM_SCENE_TRANS) {
            void *_{nullptr};
            auto ret = dramSegment_->ReserveMemorySpace(&_);
            if (ret != BM_OK) {
                BM_LOG_ERROR("Failed to reserve dram va for user malloc");
                return ret;
            }
        }
    }

    return BM_OK;
}

Result MemEntityDefault::InitHbmSegment()
{
    if (options_.scene != HYBM_SCENE_TRANS && options_.maxHBMSize == 0) {
        BM_LOG_INFO("Hbm rank space is zero.");
        return BM_OK;
    }

    MemSegmentOptions segmentOptions;
    if (options_.scene != HYBM_SCENE_TRANS) {
        segmentOptions.size = options_.deviceVASpace;
        segmentOptions.maxSize = options_.maxHBMSize;
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
    segmentOptions.flags = options_.flags;
    hbmSegment_ = MemSegment::Create(segmentOptions, id_);
    if (hbmSegment_ == nullptr) {
        BM_LOG_ERROR("Failed to create hbm segment");
        return BM_ERROR;
    }
    return BM_OK;
}

Result MemEntityDefault::InitDramSegment()
{
    if (options_.maxDRAMSize == 0) {
        BM_LOG_INFO("Dram rank space is zero.");
        return BM_OK;
    }

    MemSegmentOptions segmentOptions;
    segmentOptions.size = options_.hostVASpace;
    segmentOptions.maxSize = options_.maxDRAMSize;
    segmentOptions.devId = HybmGetInitDeviceId();
    segmentOptions.segType = HYBM_MST_DRAM;
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;
    segmentOptions.dataOpType = options_.bmDataOpType;
    segmentOptions.flags = options_.flags;
    if (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        segmentOptions.shared = false;
    }
    dramSegment_ = MemSegment::Create(segmentOptions, id_);
    if (dramSegment_ == nullptr) {
        BM_LOG_ERROR("Failed to create dram segment");
        return BM_ERROR;
    }
    return BM_OK;
}

Result MemEntityDefault::InitTransManager()
{
    if (options_.rankCount <= 1) {
        BM_LOG_INFO("rank total count : " << options_.rankCount << ", no transport.");
        return BM_OK;
    }

    auto hostTransFlags = HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA | HYBM_DOP_TYPE_HOST_TCP;
    auto composeTransFlags = HYBM_DOP_TYPE_DEVICE_RDMA | hostTransFlags;
    if ((options_.bmDataOpType & composeTransFlags) == 0) {
        BM_LOG_DEBUG("NO RDMA Data Operator transport skip init.");
        return BM_OK;
    }

    transportManager_ = transport::TransportManager::Create(transport::TT_COMPOSE, tagManager_);
    BM_ASSERT_LOG_AND_RETURN(transportManager_ != nullptr, "Failed to create transportManager.", BM_ERROR);

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
        BM_LOG_INFO("Type is ai core, not need init data operator.");
        return BM_OK;
    }

    // use composeDataOperator
    dataOperator_ = std::make_shared<HostComposeDataOp>(options_, transportManager_, tagManager_);
    auto ret = dataOperator_->Initialize();
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to init data operator ret:" << ret);
        return ret;
    }
    return BM_OK;
}

bool MemEntityDefault::SdmaReaches(uint32_t remoteRank) const noexcept
{
    if (hbmSegment_ != nullptr) {
        return hbmSegment_->CheckSdmaReaches(remoteRank);
    }

    if (dramSegment_ != nullptr) {
        return dramSegment_->CheckSdmaReaches(remoteRank);
    }

    return false;
}

hybm_data_op_type MemEntityDefault::CanReachDataOperators(uint32_t remoteRank) const noexcept
{
    uint32_t supportDataOp = 0U;
    if ((options_.bmDataOpType & HYBM_DOP_TYPE_SDMA) && SdmaReaches(remoteRank)) {
        supportDataOp |= (HYBM_DOP_TYPE_MTE | HYBM_DOP_TYPE_SDMA);
    }

    if (options_.bmDataOpType & HYBM_DOP_TYPE_DEVICE_RDMA) {
        supportDataOp |= HYBM_DOP_TYPE_DEVICE_RDMA;
    }

    if (options_.bmDataOpType & HYBM_DOP_TYPE_HOST_RDMA) {
        supportDataOp |= HYBM_DOP_TYPE_HOST_RDMA;
    }

    if (options_.bmDataOpType & HYBM_DOP_TYPE_HOST_URMA) {
        supportDataOp |= HYBM_DOP_TYPE_HOST_URMA;
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
#if !defined(ASCEND_NPU)
    return BM_OK;
#endif
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
    if (!initialized_) {
        return;
    }
    hbmSegment_.reset();
    dramSegment_.reset();
    dataOperator_.reset();
    if (transportManager_ != nullptr) {
        transportManager_->CloseDevice();
        transportManager_.reset();
    }
    initialized_ = false;
}
} // namespace mf
} // namespace ock