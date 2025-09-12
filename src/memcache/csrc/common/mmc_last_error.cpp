/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_last_error.h"

namespace ock {
namespace mmc {
thread_local bool MmcLastError::have_ = false;
thread_local std::string MmcLastError::msg_;
}  // namespace smem
}  // namespace ock