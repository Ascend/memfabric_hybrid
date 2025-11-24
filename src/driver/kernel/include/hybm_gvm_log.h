/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_GVM_LOG_H
#define MF_HYBRID_GVM_LOG_H

#include <linux/kernel.h>
#include <linux/sched.h>

#define hybm_gvm_printk(level, fmt, ...) \
    (void)printk(level "[GVM][%s:%d][%d]" fmt "\n", __func__, __LINE__, current->tgid, ##__VA_ARGS__)

#define hybm_gvm_err(fmt, ...)   hybm_gvm_printk(KERN_ERR, "[ERR] " fmt, ##__VA_ARGS__)
#define hybm_gvm_warn(fmt, ...)  hybm_gvm_printk(KERN_WARNING, "[WARN] " fmt, ##__VA_ARGS__)
#define hybm_gvm_info(fmt, ...)  hybm_gvm_printk(KERN_INFO, "[INFO] " fmt, ##__VA_ARGS__)
#define hybm_gvm_debug(fmt, ...) hybm_gvm_printk(KERN_DEBUG, "[DEBUG] " fmt, ##__VA_ARGS__)
#define hybm_gvm_event(fmt, ...) hybm_gvm_printk(KERN_NOTICE, fmt, ##__VA_ARGS__)

#endif // SMEM_GVM_LOG_H
