/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
 * @param params.count            [in] copy memory size in bytes.
 * @param direction        [in] copy direction.
 * @param stream           [in] copy used stream (use default stream if stream == NULL)
 * @param flags            [in] optional flags, default value 0.
 * @return 0 if reserved successful
 */
int32_t hybm_data_copy(hybm_entity_t e, hybm_copy_params *params,
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
 * @return 0 if reserved successful
 */
int32_t hybm_data_copy_2d(hybm_entity_t e, hybm_copy_2d_params *params,
                          hybm_data_copy_direction direction, void *stream, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif  // MF_HYBM_CORE_HYBM_DATA_OP_H
