/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "hybm_gvm_ioctl.h"

#include "hybm_gvm_cmd.h"
#include "hybm_gvm_log.h"

void hybm_gvm_ioctl_lock(struct hybm_gvm_process *gvm_proc, u32 lock_flag)
{
    if (gvm_proc == NULL) {
        return;
    }

    if ((lock_flag & HYBM_GVM_WLOCK) == HYBM_GVM_WLOCK) {
        down_write(&gvm_proc->ioctl_rwsem);
    } else {
        if (down_read_trylock(&gvm_proc->ioctl_rwsem) == 0) {
            down_read(&gvm_proc->ioctl_rwsem);
        }
    }
}

void hybm_gvm_ioctl_unlock(struct hybm_gvm_process *gvm_proc, u32 lock_flag)
{
    if (gvm_proc == NULL) {
        return;
    }

    if ((lock_flag & HYBM_GVM_WLOCK) == HYBM_GVM_WLOCK) {
        up_write(&gvm_proc->ioctl_rwsem);
    } else {
        up_read(&gvm_proc->ioctl_rwsem);
    }
}

int hybm_gvm_dispatch_ioctl(struct file *file, u32 cmd, struct hybm_gvm_ioctl_arg *buffer)
{
    struct hybm_gvm_process *gvm_proc = NULL;
    u32 cmd_id = _IOC_NR(cmd);
    int ret;

    if (file->private_data == NULL) {
        hybm_gvm_err("gvm not initialized, private_data is NULL.");
        return -EINVAL;
    }

    if (cmd_id >= HYBM_GVM_CMD_MAX_CMD || gvm_ioctl_handlers[cmd_id].ioctl_handler == NULL) {
        hybm_gvm_err("Cmd not support. (cmd=0x%x; cmd_id=0x%x)", cmd, cmd_id);
        return -EOPNOTSUPP;
    }

    if (cmd_id == _IOC_NR(HYBM_GVM_CMD_PROC_CREATE)) { // 初始化操作未创建gvm_proc,不加锁
        return gvm_ioctl_handlers[cmd_id].ioctl_handler(file, NULL, buffer);
    }

    gvm_proc = (struct hybm_gvm_process *)(((struct gvm_private_data *)file->private_data)->process);
    if (gvm_proc == NULL || gvm_proc->initialized != true) {
        hybm_gvm_err("gvm_proc has not been initialized.");
        return -EINVAL;
    }

    hybm_gvm_ioctl_lock(gvm_proc, HYBM_GVM_WLOCK);
    ret = gvm_ioctl_handlers[cmd_id].ioctl_handler(file, gvm_proc, buffer);
    hybm_gvm_ioctl_unlock(gvm_proc, HYBM_GVM_WLOCK);
    return ret;
}
