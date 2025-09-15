/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef SMEM_HYBM_GVM_PROC_INFO_H
#define SMEM_HYBM_GVM_PROC_INFO_H

#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/mutex.h>
#include <linux/kref.h>

#define HYBM_GVM_PAGE_SIZE   (1024UL * 1024UL * 1024UL)                 // 1G
#define HYBM_HPAGE_SIZE      (2UL * 1024UL * 1024UL)                    // 2M
#define HYBM_GVM_PAGE_NUM    (HYBM_GVM_PAGE_SIZE / HYBM_HPAGE_SIZE)     // 512
#define HYBM_GVM_ALLOC_MAX_SIZE   (512UL * 1024UL * 1024UL * 1024UL)    // 512G
#define HYBM_MAX_DEVICE_NUM  64

#define HYBM_SVM_START  0x100000000000ULL
#define HYBM_SVM_END    0x180000000000ULL

#define HYBM_SVSP_START 0x800000000000ULL
#define HYBM_SVSP_END   0xD00000000000ULL

#ifndef GVM_ALIGN_DOWN
#define GVM_ALIGN_DOWN(val, al) ((val) & ~((al) - 1))
#endif
#ifndef GVM_ALIGN_UP
#define GVM_ALIGN_UP(val, al) (((val) + ((al) - 1)) & ~((al) - 1))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef UINT64_MAX
#define UINT64_MAX (~(u64)0)
#endif

struct gvm_private_data {
    void *process;
    atomic_t init_flag;
};

struct gvm_pa_node {
    u64 pa;
    u64 *dma_list;
};

struct gvm_rbtree {
    struct rb_root tree;
    struct mutex lock;
};

struct hybm_gvm_process {
    int initialized;

    u32 devid; // 每个devid仅有一个proc,暂不考虑多个进程关联同一个dev
    u32 sdid;
    u32 pasid;
    u32 svspid;
    u32 key_idx; // 考虑回绕，考虑新旧进程id重叠，考虑节点重启等场景
    u64 va_start;
    u64 va_end;
    u64 va_page_num;

    struct gvm_rbtree va_tree;
    struct gvm_rbtree key_tree;
    struct gvm_rbtree fetch_tree;
    struct kref ref;

    struct vm_area_struct *vma;
    struct mm_struct *mm;
};

struct gvm_wlist_node {
    u32 sdid;    // remote server id
    void *rproc; // remote proc
    struct list_head node;
};

struct gvm_va_node_head {
    u64 va;
    u64 size;
    struct kref ref;
    struct rb_node va_node;
};

// head is same with struct gvm_va_node_head
struct gvm_node {
    u64 va;
    u64 size;
    struct kref ref;
    struct rb_node va_node;

    u64 shm_key;
    u32 pa_num;
    u32 pad;
    u64 flag;
    struct mutex lock;
    struct list_head wlist_head;
    struct rb_node key_node;
    struct gvm_pa_node pmem[0];
};

// head is same with struct gvm_va_node_head
struct gvm_dev_mem_node {
    u64 va;
    u64 size;
    struct kref ref;
    struct rb_node va_node;

    u32 page_size;
    u32 pa_num;
    struct list_head node;
    u64 pa[HYBM_GVM_PAGE_NUM];
};

#endif // SMEM_HYBM_GVM_PROC_INFO_H
