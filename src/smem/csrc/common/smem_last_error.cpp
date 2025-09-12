/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "smem_last_error.h"

namespace ock {
namespace smem {
thread_local bool SmLastError::have_ = false;
thread_local std::string SmLastError::msg_;
}  // namespace smem
}  // namespace ock