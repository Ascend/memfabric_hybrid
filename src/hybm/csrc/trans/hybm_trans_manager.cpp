/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_trans_manager.h"
#include "hybm_hcom_rdma_trans_manager.h"

using namespace ock::mf;

TransManagerPtr HybmTransManager::Create(HybmTransType type)
{
    TransManagerPtr res;
    switch (type) {
        case TT_HCOM_RDMA:
            res = HcomRdmaTransManager::GetInstance();
            break;
        default:
            BM_LOG_ERROR("Invalid trans type: " << type);
    }
    return res;
}