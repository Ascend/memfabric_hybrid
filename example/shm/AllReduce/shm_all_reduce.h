/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file constains code of cpu debug and npu code. We read data from bin file
 * and write result to file.
 */
#ifndef SHM_ALL_REDUCE_H
#define SHM_ALL_REDUCE_H

void shm_all_reduce_do(uint32_t coreDim, void* stream, uint8_t* gva, uint64_t spaceOffset,
    uint64_t flagOffset, uint32_t rankId, uint32_t rankSize);

#endif // SHM_ALL_REDUCE_H