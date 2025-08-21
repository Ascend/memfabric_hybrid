/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef SMEM_HYBM_GVM_PROC_INFO_H
#define SMEM_HYBM_GVM_PROC_INFO_H

#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/rwsem.h>

#define HYBM_GVM_PAGE_SIZE   (1024UL * 1024UL * 1024UL)                 // 1G
#define HYBM_HPAGE_SIZE      (2UL * 1024UL * 1024UL)                    // 2M
#define HYBM_GVM_PAGE_NUM    (HYBM_GVM_PAGE_SIZE / HYBM_HPAGE_SIZE)     // 512
#define HYBM_GVM_MAX_VA_SIZE (16UL * 1024UL * 1024UL * 1024UL * 1024UL) // 2T
#define HYBM_MAX_DEVICE_NUM  64

#ifndef GVM_ALIGN_DOWN
#define GVM_ALIGN_DOWN(val, al) ((val) & ~((al) - 1))
#endif
#ifndef GVM_ALIGN_UP
#define GVM_ALIGN_UP(val, al) (((val) + ((al) - 1)) & ~((al) - 1))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef UINT64_MAX
#define UINT64_MAX (~(u64)0)
#endif

struct gvm_private_data {
    void *process;
    atomic_t init_flag;
};

struct mem_node {
    u64 pa;
    void *head_node;
};

struct hybm_gvm_process {
    int inited;

    u32 devid; // 每个devid仅有一个proc,暂不考虑多个进程关联同一个dev
    u32 sdid;
    u32 pasid;
    u32 svspid;
    u32 key_idx; // TODO: 考虑回绕，考虑新旧进程id重叠，考虑节点重启等场景
    u64 va_start;
    u64 va_end;
    u64 va_page_num;
    struct mem_node *mem_array;
    struct rb_root key_tree;
    struct list_head fetch_head;       // TODO: 使用树记录,可以支持提前删除
    struct list_head dev_alloced_head; // TODO: 使用树记录,可以支持提前删除
    struct rw_semaphore ioctl_rwsem;   // proc所有ioctl都有加锁,避免并发

    struct vm_area_struct *vma;
    struct mm_struct *mm;
};

struct gvm_wlist_node {
    u32 sdid;    // remote server id
    void *rproc; // remote proc
    struct list_head node;
};

struct gvm_node {
    u64 va;
    u64 size;
    u64 shm_key;
    u64 flag;

    struct list_head wlist_head;
    struct rb_node tree_node;
};

struct gvm_dev_mem_node {
    u64 va;
    u64 pa;
    u64 size;
    struct list_head node;
};

#endif // SMEM_HYBM_GVM_PROC_INFO_H
