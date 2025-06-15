/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "dl_acl_api.h"
#include "hybm_logger.h"
#include "hybm_trans_manager.h"
#include "hybm_rdma_trans_manager.h"

namespace ock {
namespace mf {
TransportManagerPtr TransportManager::Create(TransType t)
{
    int32_t deviceId = -1;
    auto ret = DlAclApi::AclrtGetDevice(&deviceId);
    if (ret != 0 || deviceId < 0) {
        BM_LOG_ERROR("get device id failed, ret=" << ret << ", deviceId=" << deviceId);
        return nullptr;
    }

    if (t != TransType::TT_HCCP_RDMA) {
        BM_LOG_ERROR("Create Transport failed, trans type invalid: " << t);
        return nullptr;
    }

    return std::make_shared<RdmaTransportManager>(deviceId, 10002);
}
}
}