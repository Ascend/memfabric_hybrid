/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_GVM_S2A_H
#define MF_HYBRID_HYBM_GVM_S2A_H

#include <linux/types.h>

int hybm_gvm_to_agent_mmap(u32 devid, u32 pasid, u64 va, u64 size, u64 *pa_list, u32 num);
int hybm_gvm_to_agent_unmap(u32 devid, u32 pasid, u64 va, u64 size, u64 page_size);
int hybm_gvm_to_agent_init(u32 devid, u32 *pasid, u32 *svspid);
int hybm_gvm_to_agent_fetch(u32 devid, u32 pasid, u64 va, u64 size, u32 *pg_size, u64 *pa_list, u32 num);
int hybm_gvm_to_agent_register(u32 devid, u32 pasid, u64 va, u64 size, u64 new_va, u32 *pg_size);

#endif // MF_HYBRID_HYBM_GVM_S2A_H
