/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_GVM_PROC_H
#define MF_HYBRID_HYBM_GVM_PROC_H

#include <linux/fs.h>
#include <linux/types.h>

#include "hybm_gvm_proc_info.h"

void hybm_gvm_proc_destroy(struct gvm_private_data *priv);

int hybm_gvm_proc_get_pa(u32 sdid, u64 va, u64 size, u64 *arr, u64 max_num);
int hybm_gvm_insert_remote(u32 sdid, u64 key, u64 va, u64 size, struct hybm_gvm_process **rp);
int hybm_gvm_proc_set_pa(struct hybm_gvm_process *proc, u32 sdid, struct gvm_node *node, u64 *arr, u64 num);
int hybm_gvm_unset_remote(u32 sdid, u64 key, struct hybm_gvm_process *rp);
int hybm_gvm_fetch_remote(u32 sdid, u64 va, u64 *pa_list);

#endif // MF_HYBRID_HYBM_GVM_PROC_H
