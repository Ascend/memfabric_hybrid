/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "securec.h"
#include "hybm_logger.h"
#include "runtime_api.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_devide_mem_segment.h"
#include "hybm_data_operator_sdma.h"
#include "hybm_entity_default.h"

namespace ock {
namespace mf {

MemEntityDefault::MemEntityDefault(int id) noexcept : id_(id) {}

MemEntityDefault::~MemEntityDefault() = default;

int32_t MemEntityDefault::Initialize(const hybm_options *options) noexcept
{
    if (id_ >= HYBM_ENTITY_NUM_MAX) {
        BM_LOG_ERROR("input entity id is invalid, input: " << id_ << " must be less than: " << HYBM_ENTITY_NUM_MAX);
        return BM_INVALID_PARAM;
    }

    auto ret = CheckOptions(options);
    if (ret != BM_OK) {
        return ret;
    }

    ret = RuntimeApi::AclrtCreateStream(&stream_);
    if (ret != 0) {
        BM_LOG_ERROR("create stream failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    options_ = *options;
    MemSegmentOptions segmentOptions;
    segmentOptions.size = options_.singleRankVASpace;
    segmentOptions.id = HybmGetInitDeviceId();
    segment_ = MemSegment::Create(HyBM_MST_HBM, segmentOptions, id_);
    ret = MemSegmentDevice::GetDeviceId(segmentOptions.id);
    if (ret != 0) {
        RuntimeApi::AclrtDestroyStream(stream_);
        stream_ = nullptr;
        return ret;
    }

    dataOperator_ = std::make_shared<HostDataOpSDMA>(stream_);
    return BM_OK;
}

void MemEntityDefault::UnInitialize() noexcept
{
    segment_.reset();
    dataOperator_.reset();
    RuntimeApi::AclrtDestroyStream(stream_);
    stream_ = nullptr;
}

int32_t MemEntityDefault::ReserveMemorySpace(void **reservedMem) noexcept
{
    return segment_->PrepareVirtualMemory(options_.rankId, options_.rankCount, reservedMem);
}

int32_t MemEntityDefault::UnReserveMemorySpace() noexcept
{
    return BM_OK;
}

int32_t MemEntityDefault::AllocLocalMemory(uint64_t size, uint32_t flags, hybm_mem_slice_t &slice) noexcept
{
    if ((size % DEVICE_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("allocate memory size: " << size << " invalid, page size is: " << DEVICE_LARGE_PAGE_SIZE);
        return BM_INVALID_PARAM;
    }

    std::shared_ptr<MemSlice> realSlice;
    auto ret = segment_->AllocMemory(size, realSlice);
    if (ret != 0) {
        BM_LOG_ERROR("segment allocate slice with size: " << size << " failed: " << ret);
        return ret;
    }

    slice = realSlice->ConvertToId();
    return BM_OK;
}

void MemEntityDefault::FreeLocalMemory(hybm_mem_slice_t slice, uint32_t flags) noexcept {}

int32_t MemEntityDefault::ExportExchangeInfo(hybm_exchange_info &desc, uint32_t flags) noexcept
{
    std::string info;
    auto ret = segment_->Export(info);
    if (ret != 0) {
        BM_LOG_ERROR("export to string failed: " << ret);
        return ret;
    }

    if (info.size() > sizeof(desc.desc)) {
        BM_LOG_ERROR("export to string too long size : " << info.size());
        return -1;
    }

    ret = memcpy_s(desc.desc, sizeof(desc.desc), info.data(), info.size());
    if (ret != EOK) {
        BM_LOG_ERROR("copy export info desc failed: " << ret);
        return -1;
    }

    return BM_OK;
}

int32_t MemEntityDefault::ExportExchangeInfo(hybm_mem_slice_t slice, hybm_exchange_info &desc, uint32_t flags) noexcept
{
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

    if (info.size() > sizeof(desc.desc)) {
        BM_LOG_ERROR("export to string too long size : " << info.size());
        return -1;
    }

    ret = memcpy_s(desc.desc, sizeof(desc.desc), info.data(), info.size());
    if (ret != EOK) {
        BM_LOG_ERROR("copy export info desc failed: " << ret);
        return -1;
    }

    desc.descLen = info.size();
    return BM_OK;
}

int32_t MemEntityDefault::ImportExchangeInfo(const hybm_exchange_info *desc, uint32_t count, uint32_t flags) noexcept
{
    auto ret = RuntimeApi::AclrtSetDevice(HybmGetInitDeviceId());
    if (ret != BM_OK) {
        BM_LOG_ERROR("set device id to be " << HybmGetInitDeviceId() << " failed: " << ret);
        return BM_ERROR;
    }

    std::vector<std::string> infos;
    for (auto i = 0U; i < count; i++) {
        infos.emplace_back((const char *)desc[i].desc, desc[i].descLen);
    }

    return segment_->Import(infos);
}

int32_t MemEntityDefault::SetExtraContext(const void *context, uint32_t size) noexcept
{
    BM_ASSERT_RETURN(context != nullptr, BM_INVALID_PARAM);
    if (size > HYBM_DEVICE_USER_CONTEXT_PRE_SIZE) {
        BM_LOG_ERROR("set extra context failed, context size is too large: " << size << " limit: "
                                                                             << HYBM_DEVICE_USER_CONTEXT_PRE_SIZE);
        return BM_INVALID_PARAM;
    }

    uint64_t addr = HYBM_DEVICE_USER_CONTEXT_ADDR + id_ * HYBM_DEVICE_USER_CONTEXT_PRE_SIZE;
    auto ret = RuntimeApi::AclrtMemcpy((void *)addr, HYBM_DEVICE_USER_CONTEXT_PRE_SIZE, context, size,
                                       ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("memcpy user context failed, ret: " << ret);
        return BM_ERROR;
    }

    HybmDeviceMeta info;
    SetHybmDeviceInfo(info);
    info.extraContextSize = size;
    addr = HYBM_DEVICE_META_ADDR + HYBM_DEVICE_GLOBAL_META_SIZE + id_ * HYBM_DEVICE_PRE_META_SIZE;
    ret = RuntimeApi::AclrtMemcpy((void *)addr, DEVICE_LARGE_PAGE_SIZE, &info, sizeof(HybmDeviceMeta),
                                  ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("update hybm info memory failed, ret: " << ret);
        return BM_ERROR;
    }
    return BM_OK;
}

int32_t MemEntityDefault::Start(uint32_t flags) noexcept
{
    HybmDeviceMeta info;
    SetHybmDeviceInfo(info);

    uint64_t addr = HYBM_DEVICE_META_ADDR + HYBM_DEVICE_GLOBAL_META_SIZE + id_ * HYBM_DEVICE_PRE_META_SIZE;
    auto ret = RuntimeApi::AclrtMemcpy((void *)addr, DEVICE_LARGE_PAGE_SIZE, &info, sizeof(HybmDeviceMeta),
                                       ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("memcpy hybm info memory failed, ret: " << ret);
        return BM_ERROR;
    }

    return segment_->Start();
}

void MemEntityDefault::Stop() noexcept
{
    segment_->Stop();
}

int32_t MemEntityDefault::Mmap() noexcept
{
    return segment_->Mmap();
}

int32_t MemEntityDefault::Join(uint32_t rank) noexcept
{
    if (rank >= options_.rankCount) {
        BM_LOG_ERROR("input rank is invalid! rank:" << rank << " rankSize:" << options_.rankCount);
        return BM_INVALID_PARAM;
    }

    return segment_->Join(rank);
}

int32_t MemEntityDefault::Leave(uint32_t rank) noexcept
{
    if (rank >= options_.rankCount) {
        BM_LOG_ERROR("input rank is invalid! rank:" << rank << " rankSize:" << options_.rankCount);
        return BM_INVALID_PARAM;
    }

    return segment_->Leave(rank);
}

int32_t MemEntityDefault::CopyData(const void *src, void *dest, uint64_t length, hybm_data_copy_direction direction,
                                   uint32_t flags) noexcept
{
    if (dataOperator_ == nullptr) {
        BM_LOG_ERROR("memory entity not initialized.");
        return BM_ERROR;
    }

    return dataOperator_->DataCopy(src, dest, length, static_cast<DataOpDirection>(direction), flags);
}

bool MemEntityDefault::CheckAddressInEntity(const void *ptr, uint64_t length) const noexcept
{
    if (segment_ == nullptr) {
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

void MemEntityDefault::SetHybmDeviceInfo(HybmDeviceMeta &info)
{
    info.entityId = id_;
    info.rankId = options_.rankId;
    info.rankSize = options_.rankCount;
    info.symmetricSize = options_.singleRankVASpace;
    info.extraContextSize = 0;
}

}
}