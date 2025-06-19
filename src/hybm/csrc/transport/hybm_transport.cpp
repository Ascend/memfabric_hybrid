/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include "hybm_logger.h"
#include "hybm_trans_manager.h"
#include "dl_acl_api.h"
#include "hybm_transport.h"

using namespace ock::mf;

namespace {
uint32_t rankId_ = 0;
TransportManagerPtr instance_;
TransHandlePtr transHandle_;
}

HYBM_API int hybm_transport_init(uint32_t rankId, uint32_t rankCount)
{
    instance_ = TransportManager::Create(TransType::TT_HCCP_RDMA);
    if (instance_ == nullptr) {
        BM_LOG_ERROR("create transport manager failed.");
        return BM_ERROR;
    }

    TransDeviceOptions options{};
    options.rankId = rankId;
    options.rankCount = rankCount;
    auto handle = instance_->OpenDevice(options);
    if (handle == nullptr) {
        BM_LOG_ERROR("transport manager open device failed.");
        instance_ = nullptr;
        return BM_ERROR;
    }

    rankId_ = rankId;
    transHandle_ = handle;
    return BM_OK;
}

HYBM_API int hybm_transport_register_mr(uint64_t address, uint64_t size, uint32_t *lkey, uint32_t *rkey)
{
    if (address == 0 || size == 0) {
        BM_LOG_ERROR("input address " << address << ", size " << size << " invalid.");
        return BM_INVALID_PARAM;
    }

    if (lkey == nullptr || rkey == nullptr) {
        BM_LOG_ERROR("input lkey or rkey is null");
        return BM_INVALID_PARAM;
    }

    if (instance_ == nullptr) {
        BM_LOG_ERROR("transport not initialize.");
        return BM_ERROR;
    }

    TransMemRegInput input;
    TransMemRegOutput output;
    input.addr = (void *)(ptrdiff_t)address;
    input.size = size;
    input.access = RA_ACCESS_LOCAL_WRITE | RA_ACCESS_REMOTE_WRITE | RA_ACCESS_REMOTE_READ;
    auto ret = instance_->RegMemToDevice(transHandle_, input, output);
    if (ret != 0) {
        BM_LOG_ERROR("transport register memory region failed: " << ret);
        return ret;
    }

    *lkey = output.lkey;
    *rkey = output.rkey;

    BM_LOG_INFO("register address: " << input.addr << ", size: " << size << ", lkey=" << output.lkey
                                     << ", rkey=" << output.rkey);
    return BM_OK;
}

HYBM_API int hybm_transport_set_mrs(const struct hybm_transport_mr_info mrs[], uint32_t count)
{
    if (mrs == nullptr) {
        BM_LOG_ERROR("input mrs is null");
        return BM_INVALID_PARAM;
    }

    if (instance_ == nullptr) {
        BM_LOG_ERROR("transport not initialize.");
        return BM_ERROR;
    }

    std::vector<RdmaMemRegionInfo> mrv;
    for (auto i = 0U; i < count; i++) {
        RdmaMemRegionInfo mr;
        mr.size = mrs[i].size;
        mr.addr = mrs[i].addr;
        mr.lkey = mrs[i].lkey;
        mr.rkey = mrs[i].rkey;
        mrv.push_back(mr);
    }
    auto ret = instance_->SetGlobalRegisterMrInfo(mrv);
    if (ret != 0) {
        BM_LOG_ERROR("set global register mr failed: " << ret);
        return ret;
    }

    return BM_OK;
}

HYBM_API int hybm_transport_get_address(uint64_t *address)
{
    if (address == nullptr) {
        BM_LOG_ERROR("input address is null");
        return BM_INVALID_PARAM;
    }

    if (instance_ == nullptr) {
        BM_LOG_ERROR("transport not initialize.");
        return BM_ERROR;
    }

    *address = instance_->GetTransportId();
    return BM_OK;
}

HYBM_API int hybm_transport_set_addresses(uint64_t addresses[], uint32_t count)
{
    if (addresses == nullptr) {
        BM_LOG_ERROR("input address is null");
        return BM_INVALID_PARAM;
    }

    if (instance_ == nullptr) {
        BM_LOG_ERROR("transport not initialize.");
        return BM_ERROR;
    }

    std::vector<uint64_t> deviceAddress;
    for (auto i = 0U; i < count; i++) {
        deviceAddress.push_back(addresses[i]);
    }

    instance_->SetTransportIds(deviceAddress);
    return BM_OK;
}

HYBM_API int hybm_transport_make_connections()
{
    if (instance_ == nullptr) {
        BM_LOG_ERROR("transport not initialize.");
        return BM_ERROR;
    }

    TransPrepareOptions prepareOptions;
    auto ret = instance_->PrepareDataConn(prepareOptions);
    if (ret != BM_OK) {
        BM_LOG_ERROR("transport socket server prepare failed: " << ret);
        return ret;
    }

    TransDataConnOptions connOptions;
    ret = instance_->CreateDataConn(connOptions);
    if (ret != BM_OK) {
        BM_LOG_ERROR("transport socket client connect failed: " << ret);
        return ret;
    }

    ret = instance_->WaitingReady(std::chrono::minutes(1));
    if (ret != BM_OK) {
        BM_LOG_ERROR("transport wait connection failed: " << ret);
        return ret;
    }

    auto connectInfo = instance_->GetDataConnAddrInfo();
    if (connectInfo.address == nullptr) {
        BM_LOG_ERROR("transport connection qp info get failed.");
        return BM_ERROR;
    }

    return BM_OK;
}

HYBM_API int hybm_transport_ai_qp_info_address(void **address)
{
    if (address == nullptr) {
        BM_LOG_ERROR("input address is null");
        return BM_INVALID_PARAM;
    }

    if (instance_ == nullptr) {
        BM_LOG_ERROR("transport not initialize.");
        return BM_ERROR;
    }

    auto connectInfo = instance_->GetDataConnAddrInfo();
    if (connectInfo.address == nullptr) {
        BM_LOG_ERROR("transport connection qp info get failed.");
        return BM_ERROR;
    }

    *address = connectInfo.address;
    auto offset = offsetof(HybmDeviceMeta, qpInfoAddress);
    auto deviceAddr =
        HYBM_DEVICE_META_ADDR + HYBM_DEVICE_GLOBAL_META_SIZE + rankId_ * HYBM_DEVICE_PRE_META_SIZE + offset;
    auto ret = DlAclApi::AclrtMemcpy((void *)deviceAddr, DEVICE_LARGE_PAGE_SIZE, &connectInfo.address, sizeof(uint64_t),
                                     ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy qp info address from host to device failed: " << ret);
        return BM_ERROR;
    }
    return BM_OK;
}