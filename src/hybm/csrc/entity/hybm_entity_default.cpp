/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <algorithm>
#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "hybm_mem_segment.h"
#include "hybm_data_operator_sdma.h"
#include "hybm_entity_default.h"

namespace ock {
namespace mf {

thread_local bool MemEntityDefault::isSetDevice_ = false;

MemEntityDefault::MemEntityDefault(int id) noexcept : id_(id), initialized(false) {}

MemEntityDefault::~MemEntityDefault()
{
    ReleaseResources();
}

int32_t MemEntityDefault::CreateDataOperator()
{
    switch (options_.bmType) {
        case HYBM_TYPE_HBM_HOST_INITIATE:
            dataOperator_ = std::make_shared<HostDataOpSDMA>();
            break;
        case HYBM_TYPE_HBM_AI_CORE_INITIATE:
            return BM_OK;
        default:
            BM_LOG_ERROR("Invalid memory seg type " << int(options_.bmType));
            return BM_INVALID_PARAM;
    }

    if (dataOperator_ == nullptr) {
        BM_LOG_ERROR("create data operator failed");
        return BM_INVALID_PARAM;
    }

    auto ret = dataOperator_->Initialize();
    if (ret != 0) {
        BM_LOG_ERROR("data operator init failed, ret:" << ret);
        dataOperator_ = nullptr;
        return ret;
    }

    return BM_OK;
}

int32_t MemEntityDefault::Initialize(const hybm_options *options) noexcept
{
    if (initialized) {
        BM_LOG_WARN("The MemEntity has already been initialized, no action needs.");
        return BM_OK;
    }

    BM_VALIDATE_RETURN((id_ >= 0 && (uint32_t)(id_) < HYBM_ENTITY_NUM_MAX), "input entity id is invalid, input: "
                     << id_ << " must be less than: " << HYBM_ENTITY_NUM_MAX, BM_INVALID_PARAM);

    BM_LOG_ERROR_RETURN_IT_IF_NOT_OK(CheckOptions(options), "check options failed.");

    options_ = *options;
    MemSegmentOptions segmentOptions;
    segmentOptions.devId = HybmGetInitDeviceId();
    if (options->globalUniqueAddress) {
        segmentOptions.size = options_.singleRankVASpace;
        segmentOptions.segType = HYBM_MST_HBM;
        BM_LOG_DEBUG("create entity global unified memory space.");
    } else {
        segmentOptions.segType = HYBM_MST_HBM_USER;
        BM_LOG_DEBUG("create entity user defined memory space.");
    }
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;

    segment_ = MemSegment::Create(segmentOptions, id_);
    BM_VALIDATE_RETURN(segment_ != nullptr, "create segment failed", BM_INVALID_PARAM);

    auto ret = CreateDataOperator();
    if (ret != 0) {
        segment_ = nullptr;
        return BM_INVALID_PARAM;
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

    HybmDeviceMeta info = {};
    SetHybmDeviceInfo(info);

    uint64_t addr = HYBM_DEVICE_META_ADDR + HYBM_DEVICE_GLOBAL_META_SIZE + id_ * HYBM_DEVICE_PRE_META_SIZE;
    ret = DlAclApi::AclrtMemcpy((void *)addr, DEVICE_LARGE_PAGE_SIZE, &info, sizeof(HybmDeviceMeta),
                                ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("memcpy hybm info memory failed, ret: " << ret);
        return BM_ERROR;
    }
    return BM_OK;
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
    return segment_->ReleaseSliceMemory(memSlice);
}

int32_t MemEntityDefault::ExportExchangeInfo(hybm_exchange_info &desc, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    std::string info;
    auto ret = segment_->Export(info);
    if (ret != 0) {
        BM_LOG_ERROR("export to string failed: " << ret);
        return ret;
    }

    if (info.size() > sizeof(desc.desc)) {
        BM_LOG_ERROR("info size: " << info.size() << " is too long than " << sizeof(desc.desc));
        return BM_ERROR;
    }

    std::copy_n(info.data(), info.size(), desc.desc);
    desc.descLen = info.size();
    return BM_OK;
}

int32_t MemEntityDefault::ExportExchangeInfo(hybm_mem_slice_t slice, hybm_exchange_info &desc, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    int ret = BM_ERROR;
    std::string info;
    if (slice == nullptr) {
        ret = segment_->Export(info);
    } else {
        auto realSlice = segment_->GetMemSlice(slice);
        if (realSlice == nullptr) {
            return BM_INVALID_PARAM;
        }
        ret = segment_->Export(realSlice, info);
    }

    if (ret != 0) {
        BM_LOG_ERROR("export to string failed: " << ret);
        return ret;
    }

    if (info.size() > sizeof(desc.desc)) {
        BM_LOG_ERROR("info size: " << info.size() << " is too long than " << sizeof(desc.desc));
        return BM_ERROR;
    }

    std::copy_n(info.data(), info.size(), desc.desc);
    desc.descLen = info.size();
    return BM_OK;
}

int32_t MemEntityDefault::ImportExchangeInfo(const hybm_exchange_info *desc, uint32_t count, void *addresses[],
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

    std::vector<std::string> infos;
    for (auto i = 0U; i < count; i++) {
        infos.emplace_back((const char *)desc[i].desc, desc[i].descLen);
    }

    return segment_->Import(infos, addresses);
}

int32_t MemEntityDefault::GetExportSliceInfoSize(size_t &size) noexcept
{
    return segment_->GetExportSliceSize(size);
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

    HybmDeviceMeta info{};
    SetHybmDeviceInfo(info);
    info.extraContextSize = size;
    addr = HYBM_DEVICE_META_ADDR + HYBM_DEVICE_GLOBAL_META_SIZE + id_ * HYBM_DEVICE_PRE_META_SIZE;
    ret = DlAclApi::AclrtMemcpy((void *)addr, HYBM_DEVICE_PRE_META_SIZE, &info, sizeof(HybmDeviceMeta),
                                ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("update hybm info memory failed, ret: " << ret);
        return BM_ERROR;
    }
    return BM_OK;
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

int32_t MemEntityDefault::CopyData(hybm_copy_params &params, hybm_data_copy_direction direction,
                                   void *stream, uint32_t flags) noexcept
{
    if (!initialized || dataOperator_ == nullptr) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    return dataOperator_->DataCopy(params, direction, stream, flags);
}

int32_t MemEntityDefault::CopyData2d(hybm_copy_2d_params &params, hybm_data_copy_direction direction, void *stream,
                                     uint32_t flags) noexcept
{
    if (!initialized || dataOperator_ == nullptr) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    return dataOperator_->DataCopy2d(params, direction, stream, flags);
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

void MemEntityDefault::SetHybmDeviceInfo(HybmDeviceMeta &info)
{
    info.entityId = id_;
    info.rankId = options_.rankId;
    info.rankSize = options_.rankCount;
    info.symmetricSize = options_.singleRankVASpace;
    info.extraContextSize = 0;
}

void MemEntityDefault::ReleaseResources()
{
    if (!initialized) {
        return;
    }
    segment_.reset();
    dataOperator_.reset();
    initialized = false;
}

}
}