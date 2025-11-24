/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hybm_transport_manager.h"

#include "hybm_logger.h"
#include "host_hcom_transport_manager.h"
#include "device_rdma_transport_manager.h"
#include "compose_transport_manager.h"

using namespace ock::mf;
using namespace ock::mf::transport;

std::shared_ptr<TransportManager> TransportManager::Create(TransportType type)
{
    switch (type) {
        case TT_HCOM:
            return host::HcomTransportManager::GetInstance();
        case TT_HCCP:
            return std::make_shared<device::RdmaTransportManager>();
        case TT_COMPOSE:
            return std::make_shared<ComposeTransportManager>();
        default:
            BM_LOG_ERROR("Invalid trans type: " << type);
            return nullptr;
    }
}

const void *TransportManager::GetQpInfo() const
{
    BM_LOG_DEBUG("Not Implement GetQpInfo()");
    return nullptr;
}

Result TransportManager::ConnectWithOptions(const HybmTransPrepareOptions &options)
{
    BM_LOG_DEBUG("ConnectWithOptions now connected=" << connected_);
    if (!connected_) {
        auto ret = Prepare(options);
        if (ret != BM_OK) {
            BM_LOG_ERROR("prepare connection failed: " << ret);
            return ret;
        }

        ret = Connect();
        if (ret != BM_OK) {
            BM_LOG_ERROR("connect failed: " << ret);
            return ret;
        }

        connected_ = true;
        return BM_OK;
    }

    return UpdateRankOptions(options);
}

Result TransportManager::Remove(const std::vector<uint32_t> &removeList)
{
    BM_LOG_ERROR("TransportManager is parent class, not support Remove by ranks, please use subclass");
    return BM_INVALID_PARAM;
}
