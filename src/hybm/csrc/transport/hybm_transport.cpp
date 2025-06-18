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
}

int hybm_transport_init(uint32_t rankId, uint32_t rankCount)
{
    instance_ = TransportManager::Create(TransType::TT_HCCP_RDMA);
    if (instance_ == nullptr) {
        BM_LOG_ERROR("create transport manager failed.");
        return BM_ERROR;
    }

    TransDeviceOptions options{};
    options.rankId = rankCount;
    options.rankCount = rankCount;
    auto handle = instance_->OpenDevice(options);
    if (handle == nullptr) {
        BM_LOG_ERROR("transport manager open device failed.");
        instance_ = nullptr;
        return BM_ERROR;
    }

    rankId_ = rankId;
    return BM_OK;
}

int hybm_transport_get_address(uint64_t *address)
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

int hybm_transport_set_addresses(uint64_t addresses[], uint32_t count)
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

int hybm_transport_make_connections()
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

int hybm_transport_ai_qp_info_address(void **address)
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
    auto deviceAddr = HYBM_DEVICE_META_ADDR + HYBM_DEVICE_GLOBAL_META_SIZE + rankId_ * HYBM_DEVICE_PRE_META_SIZE + offset;
    auto ret = DlAclApi::AclrtMemcpy((void *)deviceAddr, DEVICE_LARGE_PAGE_SIZE, &connectInfo.address, sizeof(uint64_t),
                                ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        BM_LOG_ERROR("copy qp info address from host to device failed: " << ret);
        return BM_ERROR;
    }
    return BM_OK;
}