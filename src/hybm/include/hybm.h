/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_H
#define MEM_FABRIC_HYBRID_HYBM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYBM_INIT_GVM_FLAG (1ULL << 1ULL) // Init the GVM module, enable to use Host DRAM

/**
 * @brief Initialize hybrid big memory library
 *
 * @param deviceId         [in] npu device id
 * @param flags            [in] optional flags
 * @return 0 if successful
 */
int32_t hybm_init(uint16_t deviceId, uint64_t flags);

/**
 * @brief UnInitialize hybrid big memory library
 */
void hybm_uninit(void);

/**
 * @brief Set external log function, if not set, log message will be instdout
 *
 * @param logger           [in] logger function
 * @return 0 if successful
 */
void hybm_set_extern_logger(void (*logger)(int level, const char *msg));

/**
 * @brief Set log print level
 *
 * @param level           [in] log level, 0:debug 1:info 2:warn 3:error
 * @return 0 if successful
 */
int32_t hybm_set_log_level(int level);

/**
 * @brief Get error message by error code
 *
 * @param errCode          [in] error number returned by other functions
 * @return error string if the error code exists, null if the error is invalid
 */
const char *hybm_get_error_string(int32_t errCode);

#ifdef __cplusplus
}
#endif

#endif // MEM_FABRIC_HYBRID_HYBM_H
