/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "hybm_gvm_cmd.h"
#include "hybm_gvm_ioctl.h"
#include "hybm_gvm_log.h"
#include "hybm_gvm_p2p.h"
#include "hybm_gvm_proc.h"
#include "hybm_gvm_symbol_get.h"

static int hybm_gvm_open(struct inode *inode, struct file *file)
{
    struct gvm_private_data *priv = NULL;

    priv = kzalloc(sizeof(struct gvm_private_data), GFP_KERNEL | __GFP_ACCOUNT);
    if (priv == NULL) {
        hybm_gvm_err("Kzalloc gvm_private_data fail.");
        return -ENOMEM;
    }

    atomic_set(&priv->init_flag, 0);
    file->private_data = priv;
    hybm_gvm_debug("open success.");
    return 0;
}

static int hybm_gvm_release(struct inode *inode, struct file *file)
{
    if (file->private_data == NULL) {
        hybm_gvm_info("Private_data is NULL.");
        return 0;
    }
    if (((struct gvm_private_data *)file->private_data)->process == NULL) {
        hybm_gvm_info("Process is NULL.");
    } else {
        hybm_gvm_proc_destroy(file->private_data);
    }

    kfree(file->private_data);
    file->private_data = NULL;
    hybm_gvm_debug("release success.");
    return 0;
}

static long hybm_gvm_ioctl(struct file *file, u32 cmd, unsigned long arg)
{
    struct hybm_gvm_ioctl_arg buffer = {0};
    u32 cmd_id = _IOC_NR(cmd);
    int ret;

    if ((file == NULL) || (file->private_data == NULL) || (arg == 0)) {
        hybm_gvm_err("File is NULL, check svm init. (cmd=0x%x; cmd_id=0x%x)", cmd, cmd_id);
        return -EINVAL;
    }

    if (cmd_id >= HYBM_GVM_CMD_MAX_CMD) {
        hybm_gvm_err("Cmd not support. (cmd=0x%x; cmd_id=0x%x)", cmd, cmd_id);
        return -EINVAL;
    }

    if ((_IOC_DIR(cmd) & _IOC_WRITE) != 0) {
        if (copy_from_user(&buffer, (void __user *)(uintptr_t)arg, sizeof(struct hybm_gvm_ioctl_arg)) != 0) {
            hybm_gvm_err("Copy_from_user fail. (cmd=0x%x; cmd_id=0x%x)", cmd, cmd_id);
            return -EINVAL;
        }
    }

    hybm_gvm_debug("do ioctl begin (cmd=0x%x; cmd_id=0x%x)", cmd, cmd_id);
    ret = hybm_gvm_dispatch_ioctl(file, cmd, &buffer);
    hybm_gvm_debug("do ioctl end (cmd=0x%x; cmd_id=0x%x; ret:%d)", cmd, cmd_id, ret);
    if (ret != 0) {
        return ret;
    }

    if ((_IOC_DIR(cmd) & _IOC_READ) != 0) {
        if (copy_to_user((void __user *)(uintptr_t)arg, &buffer, sizeof(struct hybm_gvm_ioctl_arg)) != 0) {
            hybm_gvm_err("Copy_to_user fail. (cmd=0x%x; cmd_id=0x%x)", cmd, cmd_id);
            return -EINVAL;
        }
    }

    return 0;
}

static int hybm_gvm_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct hybm_gvm_process *gvm_proc = NULL;
    if ((file == NULL) || (file->private_data == NULL)) {
        hybm_gvm_err("File is NULL, check svm init.");
        return -EINVAL;
    }

    if (vma == NULL) {
        hybm_gvm_err("vma is null!");
        return -EINVAL;
    }

    gvm_proc = (struct hybm_gvm_process *)(((struct gvm_private_data *)file->private_data)->process);
    if (gvm_proc == NULL || gvm_proc->initialized != true) {
        hybm_gvm_err("gvm_proc has not been initialized.");
        return -EINVAL;
    }

    if (gvm_proc->vma != NULL) {
        hybm_gvm_err("gvm_proc has mapped. size:0x%lx", vma->vm_end - vma->vm_start);
        return -EINVAL;
    }

    if (vma->vm_start != gvm_proc->va_start || vma->vm_end != gvm_proc->va_end) {
        hybm_gvm_err("mapped addr is different with gvm inited addr.");
        return -EINVAL;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP | VM_DONTCOPY | VM_PFNMAP | VM_LOCKED | VM_WRITE | VM_IO);
#else
    vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP | VM_DONTCOPY | VM_PFNMAP | VM_LOCKED | VM_WRITE | VM_IO;
#endif
    gvm_proc->vma = vma;
    gvm_proc->mm = vma->vm_mm;
    hybm_gvm_info("gvm_proc map success. size:0x%lx", vma->vm_end - vma->vm_start);
    return 0;
}

static struct file_operations gvm_davinci_fops = {
    .owner = THIS_MODULE,
    .open = hybm_gvm_open,
    .release = hybm_gvm_release,
    .mmap = hybm_gvm_mmap,
    .unlocked_ioctl = hybm_gvm_ioctl,
};

int gvm_davinci_module_init(const struct file_operations *ops)
{
    int ret;

    ret = drv_davinci_register_sub_module(DAVINCI_GVM_SUB_MODULE_NAME, ops);
    if (ret != 0) {
        hybm_gvm_err("Register sub module failed. (ret=%d)", ret);
        return -ENODEV;
    }
    return 0;
}

void gvm_davinci_module_uninit(void)
{
    int ret;

    ret = drv_ascend_unregister_sub_module(DAVINCI_GVM_SUB_MODULE_NAME);
    if (ret != 0) {
        hybm_gvm_err("Unregister sub module failed. (ret=%d)", ret);
        return;
    }
    return;
}

static int __init gvm_driver_init(void)
{
    int ret;

    hybm_gvm_info("gvm model init.");
    ret = gvm_symbol_get();
    if (ret != 0) {
        hybm_gvm_err("gvm_symbol_get fail. ret:%d", ret);
        return ret;
    }

    ret = gvm_davinci_module_init(&gvm_davinci_fops);
    if (ret != 0) {
        hybm_gvm_err("gvm_davinci_module_init fail. ret:%d", ret);
        goto davinci_register_failed;
    }


    ret = hybm_gvm_p2p_msg_register();
    if (ret != 0) {
        hybm_gvm_err("hybm_gvm_p2p_msg_register fail. ret:%d", ret);
        goto p2p_init_failed;
    }

    hybm_gvm_info("gvm model init success.");
    return 0;

p2p_init_failed:
    gvm_davinci_module_uninit();
davinci_register_failed:
    gvm_symbol_put();
    return ret;
}

static void __exit gvm_driver_exit(void)
{
    hybm_gvm_info("gvm model exit.");
    hybm_gvm_p2p_msg_unregister();
    gvm_davinci_module_uninit();
    gvm_symbol_put();
}

module_init(gvm_driver_init);
module_exit(gvm_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("global virtual memory manager for Userland applications");
