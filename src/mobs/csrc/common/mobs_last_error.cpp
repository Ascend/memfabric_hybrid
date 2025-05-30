/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mobs_last_error.h"

namespace ock {
namespace smem {
thread_local bool MOLastError::have_ = false;
thread_local std::string MOLastError::msg_;
}  // namespace smem
}  // namespace ock