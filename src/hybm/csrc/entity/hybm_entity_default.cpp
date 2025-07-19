/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <algorithm>
#include "hybm_logger.h"
#include "dl_acl_api.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_devide_mem_segment.h"
#include "hybm_data_operator_sdma.h"
#include "hybm_entity_default.h"

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
    MemSegmentOptions segmentOptions;
    segmentOptions.size = options_.singleRankVASpace;
    segmentOptions.segId = HybmGetInitDeviceId();
    segmentOptions.segType = HYBM_MST_HBM;
    segmentOptions.rankId = options_.rankId;
    segmentOptions.rankCnt = options_.rankCount;

    segment_ = MemSegment::Create(segmentOptions, id_);
    if (segment_ == nullptr) {
        DlAclApi::AclrtDestroyStream(stream_);
        stream_ = nullptr;
        return BM_INVALID_PARAM;
    }

    ret = MemSegmentDevice::GetDeviceId(segmentOptions.segId);
    if (ret != 0) {
        DlAclApi::AclrtDestroyStream(stream_);
        stream_ = nullptr;
        return ret;
    }

    static HybmTransType transTypeMap[HYBM_TYPE_BUTT + 1][HYBM_DOP_TYPE_BUTT + 1] = {
        {TT_BUTT, TT_HCCP_RDMA, TT_BUTT, TT_BUTT},
        {TT_BUTT, TT_HCOM_RDMA, TT_BUTT, TT_BUTT},
        {TT_BUTT, TT_HCOM_RDMA, TT_BUTT, TT_BUTT}
    };
    transportManager_ = HybmTransManager::Create(transTypeMap[options_.bmType][options_.bmDataOpType]);

    dataOperator_ = std::make_shared<HostDataOpSDMA>(stream_);
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
    auto ret = segment_->Export(info);
    if (ret != 0) {
        BM_LOG_ERROR("export to string failed: " << ret);
        return ret;
    }

    if (info.size() != sizeof(desc.desc)) {
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

    if (info.size() > sizeof(desc.desc)) {
        BM_LOG_ERROR("export to string wrong size: " << info.size() << ", the correct size is: " << sizeof(desc.desc));
        return BM_ERROR;
    }

    std::copy_n(info.data(), info.size(), desc.desc);
    desc.descLen = info.size();
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

    std::vector<std::string> infos;
    for (auto i = 0U; i < count; i++) {
        infos.emplace_back((const char *)desc[i].desc, desc[i].descLen);
    }

    return segment_->Import(infos);
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

    return dataOperator_->DataCopy(src, dest, length, direction, stream, flags);
}

int32_t MemEntityDefault::CopyData2d(const void *src, uint64_t spitch, void *dest, uint64_t dpitch, uint64_t width,
                                     uint64_t height,  hybm_data_copy_direction direction,
                                     void *stream, uint32_t flags) noexcept
{
    if (!initialized) {
        BM_LOG_ERROR("the object is not initialized, please check whether Initialize is called.");
        return BM_NOT_INITIALIZED;
    }

    return dataOperator_->DataCopy2d(src, spitch, dest, dpitch, width, height,
                                     direction, stream, flags);
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
    HybmDeviceMeta backupInfo;
    auto addr = HYBM_DEVICE_META_ADDR + HYBM_DEVICE_GLOBAL_META_SIZE + id_ * HYBM_DEVICE_PRE_META_SIZE;
    auto ret = DlAclApi::AclrtMemcpy(&backupInfo, sizeof(HybmDeviceMeta), (void *)addr, sizeof(HybmDeviceMeta),
                                     ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != BM_OK) {
        BM_LOG_ERROR("backup hybm info memory failed, ret: " << ret);
        return BM_ERROR;
    }

    SetHybmDeviceInfo(info);
    info.extraContextSize = extCtxSize;
    info.qpInfoAddress = backupInfo.qpInfoAddress;
    ret = DlAclApi::AclrtMemcpy((void *)addr, DEVICE_LARGE_PAGE_SIZE, &info, sizeof(HybmDeviceMeta),
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
}

int32_t MemEntityDefault::TransportInit(uint32_t rankId, const std::string &nic) noexcept
{
    BM_ASSERT_RETURN(transportManager_ != nullptr, BM_ERROR);
    HybmTransOptions transOptions{};
    transOptions.rankId = rankId;
    transOptions.nic = nic;
    transOptions.transType = TT_HCOM_RDMA;
    auto ret = transportManager_->OpenDevice(transOptions);
    if (ret != BM_OK) {
        BM_LOG_ERROR("transport manager open device failed, rankId: " << rankId << " nic: " << nic << " ret: " << ret);
        return ret;
    }
    return BM_OK;
}

int32_t MemEntityDefault::TransportRegisterMr(uint64_t address, uint64_t size,
                                              hybm_mr_key *lkey, hybm_mr_key *rkey) noexcept
{
    BM_ASSERT_RETURN(transportManager_ != nullptr, BM_ERROR);
    HybmTransMemReg memInfo{};
    memInfo.addr = address;
    memInfo.size = size;
    HybmTransKey transKey{};
    auto ret = transportManager_->RegisterMemoryRegion(memInfo, transKey);
    if (ret != BM_OK) {
        BM_LOG_ERROR("transport manager register mem failed, addr: " << address
                     << " size: " << size << " ret: " << ret);
        return ret;
    }
    std::copy_n(transKey.keys, sizeof(HybmTransKey), lkey->key);
    return BM_OK;
}

int32_t MemEntityDefault::TransportSetMr(const std::vector<hybm_transport_mr_info> &mrs) noexcept
{
    BM_ASSERT_RETURN(transportManager_ != nullptr, BM_ERROR);
    return BM_OK;
}

int32_t MemEntityDefault::TransportGetAddress(std::string &nic) noexcept
{
    BM_ASSERT_RETURN(transportManager_ != nullptr, BM_ERROR);
    return BM_OK;
}

int32_t MemEntityDefault::TransportSetAddress(const std::vector<std::string> &nics) noexcept
{
    BM_ASSERT_RETURN(transportManager_ != nullptr, BM_ERROR);
    return BM_OK;
}

int32_t MemEntityDefault::TransportMakeConnect() noexcept
{
    BM_ASSERT_RETURN(transportManager_ != nullptr, BM_ERROR);
    return BM_OK;
}

int32_t MemEntityDefault::TransportAiQPInfoAddress(uint32_t shmId, void **address) noexcept
{
    BM_ASSERT_RETURN(transportManager_ != nullptr, BM_ERROR);
    return BM_OK;
}
}
}