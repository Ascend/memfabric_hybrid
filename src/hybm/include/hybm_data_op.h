/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#ifndef MF_HYBM_CORE_HYBM_DATA_OP_H
#define MF_HYBM_CORE_HYBM_DATA_OP_H

#include <stddef.h>
#include <stdint.h>
#include "hybm_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASYNC_COPY_FLAG (1UL << (0))
/**
 * @brief copies <i>count</i> bytes from memory area <i>src</i> to memory area <i>dest</i>.
 * @param e                [in] entity created by hybm_create_entity
 * @param params.src              [in] pointer to copy source memory area.
 * @param params.dest             [in] pointer to copy destination memory area.
 * @param params.dataSize         [in] copy memory size in bytes.
 * @param direction        [in] copy direction.
 * @param stream           [in] copy used stream (use default stream if stream == NULL)
 * @param flags            [in] optional flags, default value 0.
 * @return 0 if reserved successful
 */
int32_t hybm_data_copy(hybm_entity_t e, hybm_copy_params *params, hybm_data_copy_direction direction, void *stream,
                       uint32_t flags);

/**
 * @brief batch copy data bytes from memory area <i>sources</i> to memory area <i>destinations</i>.
 * @param e                [in] entity created by hybm_create_entity
 * @param params           [in] batch data copy parameters.
 * @param direction        [in] copy direction.
 * @param stream           [in] copy used stream (use default stream if stream == NULL)
 * @param flags            [in] optional flags, default value 0.
 * @return 0 if successful
 */
int32_t hybm_data_batch_copy(hybm_entity_t e, hybm_batch_copy_params *params, hybm_data_copy_direction direction,
                             void *stream, uint32_t flags);

/**
 * @brief wait data_copy_op_async finish .
 * @param e                [in] entity created by hybm_create_entity
 * @return 0 if reserved successful
 */
int32_t hybm_wait(hybm_entity_t e);

#ifdef __cplusplus
}
#endif

#endif // MF_HYBM_CORE_HYBM_DATA_OP_H
