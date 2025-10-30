/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MF_HYBM_CORE_HYBM_DATA_OP_H
#define MF_HYBM_CORE_HYBM_DATA_OP_H

#include <stddef.h>
#include <stdint.h>
#include "hybm_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief copies <i>count</i> bytes from memory area <i>src</i> to memory area <i>dest</i>.
 * @param e                [in] entity created by hybm_create_entity
 * @param params.src              [in] pointer to copy source memory area.
 * @param params.dest             [in] pointer to copy destination memory area.
 * @param params.dataSize         [in] copy memory size in bytes.
 * @param direction        [in] copy direction.
 * @param stream           [in] copy used stream (use default stream if stream == NULL)
 * @param flags            [in] optional flags, default value 0.
 * @return 0 if successful
 */
int32_t hybm_data_copy(hybm_entity_t e, hybm_copy_params *params,
                       hybm_data_copy_direction direction, void *stream, uint32_t flags);

/**
 * @brief batch copy data bytes from memory area <i>sources</i> to memory area <i>destinations</i>.
 * @param e                       [in] entity created by hybm_create_entity
 * @param params.sources          [in] array of pointers to copy sources memory area.
 * @param params.destinations     [in] array of pointer to copy destinations memory area.
 * @param params.dataSizes        [in] array of copy memory sizes in bytes.
 * @param params.batchSize        [in] array size for <i>sources</>, <i>destinations</i> and <i>counts</>.
 * @param direction               [in] copy direction.
 * @param stream                  [in] copy used stream (use default stream if stream == NULL)
 * @param flags                   [in] optional flags, default value 0.
 * @return 0 if successful
 */
int32_t hybm_data_batch_copy(hybm_entity_t e, hybm_batch_copy_params* params,
                             hybm_data_copy_direction direction, void *stream, uint32_t flags);

/**
 * @brief copies <i>count</i> bytes from memory area <i>src</i> to memory area <i>dest</i>.
 * @param e                [in] entity created by hybm_create_entity
 * @param params.src              [in] pointer to copy source memory area.
 * @param params.spitch           [in] pitch of source memory
 * @param params.dest             [in] pointer to copy destination memory area.
 * @param params.dpitch           [in] pitch of destination memory
 * @param params.width            [in] width of matrix transfer
 * @param params.height           [in] height of matrix transfer
 * @param direction        [in] copy direction.
 * @param stream           [in] copy used stream (use default stream if stream == NULL)
 * @param flags            [in] optional flags, default value 0.
 * @return 0 if successful
 */
int32_t hybm_data_copy_2d(hybm_entity_t e, hybm_copy_2d_params *params,
                          hybm_data_copy_direction direction, void *stream, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif  // MF_HYBM_CORE_HYBM_DATA_OP_H
