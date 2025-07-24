/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_trans_manager.h"
#include "hybm_hcom_trans_manager.h"
#include "hybm_hccp_trans_manager.h"

using namespace ock::mf;

TransManagerPtr HybmTransManager::Create(HybmTransType type)
{
    switch (type) {
        case TT_HCCP:
            return HccpTransManager::GetInstance();
        case TT_HCOM:
            return HcomTransManager::GetInstance();
        default:
            BM_LOG_ERROR("Invalid trans type: " << type);
            return nullptr;
    }
}
