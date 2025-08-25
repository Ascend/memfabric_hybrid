/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_GVM_IOCTL_H
#define MF_HYBRID_HYBM_GVM_IOCTL_H

#include <linux/fs.h>
#include <linux/types.h>

#include "hybm_gvm_cmd.h"
#include "hybm_gvm_proc_info.h"

#define HYBM_GVM_WLOCK 1
#define HYBM_GVM_RLOCK 2

int hybm_gvm_dispatch_ioctl(struct file *file, u32 cmd, struct hybm_gvm_ioctl_arg *buffer);
void hybm_gvm_ioctl_lock(struct hybm_gvm_process *gvm_proc, u32 lock_flag);
void hybm_gvm_ioctl_unlock(struct hybm_gvm_process *gvm_proc, u32 lock_flag);

struct gvm_ioctl_handlers_st {
    int (*ioctl_handler)(struct file *file, struct hybm_gvm_process *gvm_proc, struct hybm_gvm_ioctl_arg *);
};

extern struct gvm_ioctl_handlers_st gvm_ioctl_handlers[HYBM_GVM_CMD_MAX_CMD];

#endif // MF_HYBRID_HYBM_GVM_IOCTL_H
