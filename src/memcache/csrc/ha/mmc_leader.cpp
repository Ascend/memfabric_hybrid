/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "mmc_logger.h"


extern "C" {
void mmc_logger(int level, const char *msg)
{
    switch (level) {
        case ock::mmc::DEBUG_LEVEL: MMC_LOG_DEBUG(msg); break;
        case ock::mmc::INFO_LEVEL: MMC_LOG_INFO(msg); break;
        case ock::mmc::WARN_LEVEL: MMC_LOG_WARN(msg); break;
        case ock::mmc::ERROR_LEVEL: MMC_LOG_ERROR(msg); break;
        default: break;
    }
}
}