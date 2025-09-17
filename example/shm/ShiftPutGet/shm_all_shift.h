/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file constains code of cpu debug and npu code. We read data from bin file
 * and write result to file.
 */
#ifndef SHM_ALL_SHIFT_H
#define SHM_ALL_SHIFT_H

void shm_all_shift_do(void* stream, uint8_t* gva, int64_t *localInput);

#endif // SHM_ALL_SHIFT_H