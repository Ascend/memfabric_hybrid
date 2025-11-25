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
