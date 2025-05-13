/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "smem_bm_entry.h"

namespace ock {
namespace smem {

void SmemBmEntry::SetConfig(const smem_bm_config_t &config)
{
    extraConfig_ = config;
    SM_LOG_INFO("bmId: " << options_.id << " set_config control_operation_timeout: " <<
        extraConfig_.controlOperationTimeout);
}

Result SmemBmEntry::Join(uint32_t flags, void **localGvaAddress)
{
    SM_ASSERT_RETURN(!inited_, SM_NOT_INITIALIZED);

    return SM_OK;
}

Result SmemBmEntry::Leave(uint32_t flags)
{
    SM_ASSERT_RETURN(!inited_, SM_NOT_INITIALIZED);

    return SM_OK;
}

Result SmemBmEntry::DateCopy(void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags)
{
    SM_ASSERT_RETURN(!inited_, SM_NOT_INITIALIZED);

    return SM_OK;
}

}  // namespace smem
}  // namespace ock
