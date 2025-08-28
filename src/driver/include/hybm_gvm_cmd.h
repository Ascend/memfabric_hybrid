/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_GVM_CMD_H
#define MF_HYBRID_HYBM_GVM_CMD_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define DAVINCI_GVM_SUB_MODULE_NAME "GVM" // device name
#define HYBM_GVM_CMD_MAGIC          'G'

struct hybm_gvm_proc_create_para {
    uint64_t start;  // in
    uint64_t end;    // in
    uint32_t devid;  // in
    uint32_t svspid; // out
    uint32_t sdid;   // out
};

struct hybm_gvm_mem_alloc_para {
    uint64_t addr;     // in
    uint64_t size;     // in
    uint32_t dma_flag; // in
};

struct hybm_gvm_mem_fetch_para {
    uint64_t addr; // in
    uint64_t size; // in
    uint32_t sdid; // in
};

struct hybm_gvm_mem_free_para {
    uint64_t addr; // in
};

struct hybm_gvm_get_key_para {
    uint64_t addr; // in
    uint64_t key;  // out
};

struct hybm_gvm_set_whitelist_para {
    uint64_t key;  // in
    uint32_t sdid; // in
};

struct hybm_gvm_mem_open_para {
    uint64_t addr; // in
    uint64_t key;  // in
};

struct hybm_gvm_mem_close_para {
    uint64_t addr; // in
};

struct hybm_gvm_ioctl_arg {
    union {
        struct hybm_gvm_proc_create_para proc_create_para;
        struct hybm_gvm_mem_alloc_para mem_alloc_para;
        struct hybm_gvm_mem_free_para mem_free_para;
        struct hybm_gvm_mem_fetch_para mem_fetch_para;
        struct hybm_gvm_get_key_para get_key_para;
        struct hybm_gvm_set_whitelist_para set_wl_para;
        struct hybm_gvm_mem_open_para mem_open_para;
        struct hybm_gvm_mem_close_para mem_close_para;
    } data;
};

#define HYBM_GVM_CMD_PROC_CREATE _IOWR(HYBM_GVM_CMD_MAGIC, 1, struct hybm_gvm_ioctl_arg)
#define HYBM_GVM_CMD_MEM_ALLOC   _IOW(HYBM_GVM_CMD_MAGIC, 2, struct hybm_gvm_ioctl_arg)
#define HYBM_GVM_CMD_MEM_FREE    _IOW(HYBM_GVM_CMD_MAGIC, 3, struct hybm_gvm_ioctl_arg)
#define HYBM_GVM_CMD_GET_KEY     _IOWR(HYBM_GVM_CMD_MAGIC, 4, struct hybm_gvm_ioctl_arg)
#define HYBM_GVM_CMD_SET_WL      _IOW(HYBM_GVM_CMD_MAGIC, 5, struct hybm_gvm_ioctl_arg)
#define HYBM_GVM_CMD_MEM_OPEN    _IOW(HYBM_GVM_CMD_MAGIC, 6, struct hybm_gvm_ioctl_arg)
#define HYBM_GVM_CMD_MEM_CLOSE   _IOW(HYBM_GVM_CMD_MAGIC, 7, struct hybm_gvm_ioctl_arg)
#define HYBM_GVM_CMD_MEM_FETCH   _IOW(HYBM_GVM_CMD_MAGIC, 8, struct hybm_gvm_ioctl_arg)
#define HYBM_GVM_CMD_MAX_CMD     9 /* max cmd id */

#endif // MF_HYBRID_HYBM_GVM_CMD_H
