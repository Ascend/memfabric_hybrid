/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include "hybm_transport_manager.h"

#include "hybm_logger.h"
#include "host_hcom_transport_manager.h"
#include "device_rdma_transport_manager.h"

using namespace ock::mf::transport;

std::shared_ptr<TransportManager> TransportManager::Create(TransportType type)
{
    switch (type) {
        case TransportType::TT_HCOM:
            return host::HcomTransportManager::GetInstance();
        case TransportType::TT_HCCP:
            return std::make_shared<device::RdmaTransportManager>();
        default:
            BM_LOG_ERROR("Invalid trans type: " << static_cast<int>(type));
            return nullptr;
    }
}

const void *TransportManager::GetQpInfo() const
{
    BM_LOG_DEBUG("Not Implement GetQpInfo()");
    return nullptr;
}
