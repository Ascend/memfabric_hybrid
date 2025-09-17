/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#ifndef MEMCACHE_STUBS_H
#define MEMCACHE_STUBS_H

// smem
const uint64_t MAX_BUF_SIZE = 327680UL;
void MockSmemH(void);
void MockSmemBmH(void);
void MockSmemShmH(void);
void MockSmemAll(void);

#endif // MEMCACHE_STUBS_H