/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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
 * @param src              [in] pointer to copy source memory area.
 * @param dest             [out] pointer to copy destination memory area.
 * @param count            [in] copy memory size in bytes.
 * @param direction        [in] copy direction.
 * @param flags            [in] optional flags, default value 0.
 * @return 0 if reserved successful
 */
int32_t hybm_data_copy(hybm_entity_t e, const void *src, void *dest, size_t count, hybm_data_copy_direction direction,
                       uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif  // MF_HYBM_CORE_HYBM_DATA_OP_H
