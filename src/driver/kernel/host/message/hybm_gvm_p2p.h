/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_GVM_P2P_H
#define MF_HYBRID_HYBM_GVM_P2P_H

#include <linux/types.h>

#include "hybm_gvm_proc_info.h"

#define SET_WL_RET_DUPLICATIVE 666

int hybm_gvm_p2p_msg_register(void);
void hybm_gvm_p2p_msg_unregister(void);

int hybm_gvm_p2p_set_wl(u32 src_sdid, u32 src_devid, u32 dst_sdid, struct gvm_node *node, void **remote);
int hybm_gvm_p2p_get_addr(struct hybm_gvm_process *proc, u32 src_sdid, u32 src_devid, u32 dst_sdid,
                          struct gvm_node *node);
void hybm_gvm_p2p_unset_wl(u32 src_sdid, u32 src_devid, u32 dst_sdid, u64 key, void *remote);
int hybm_gvm_p2p_fetch(u32 src_sdid, u32 src_devid, u32 dst_sdid, u64 va, u64 *pa_list);

#endif // MF_HYBRID_HYBM_GVM_P2P_H
