/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "smem_bm_entry_manager.h"

namespace ock {
namespace smem {
SmemBmEntryManager &SmemBmEntryManager::Instance()
{
    static SmemBmEntryManager instance;
    return instance;
}
}  // namespace smem
}  // namespace ock
