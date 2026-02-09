/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* MemFabric_Hybrid is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint64_t dma_addr_t;
typedef size_t size_t;
typedef uint64_t __u64;
typedef uint32_t __u32;
typedef uint16_t __u16;
typedef uint8_t __u8;
typedef int64_t __s64;
typedef int32_t __s32;
typedef int16_t __s16;
typedef int8_t __s8;

#define __init
#define __exit
#define KERN_ERR "ERROR"
#define KERN_INFO "INFO"
#define GFP_KERNEL 0
#define THIS_MODULE 0
#define EXPORT_SYMBOL(x)
#define MODULE_LICENSE(x)
#define module_init(x) static void *__mock_init_ptr_##x = (void *)&(x)
#define module_exit(x) static void *__mock_exit_ptr_##x = (void *)&(x)
#define DMA_MAPPING_ERROR (~(dma_addr_t)0)
#define MAX_ERRNO 4095
#define IS_ERR_VALUE(x) ((unsigned long)(void *)(x) >= (unsigned long)-MAX_ERRNO)

static inline void *ERR_PTR(long error)
{
    return (void *)error;
}
static inline long PTR_ERR(const void *ptr)
{
    return (long)ptr;
}
static inline long IS_ERR(const void *ptr)
{
    return IS_ERR_VALUE((unsigned long)ptr);
}

struct mutex {
    int locked;
};
static inline void mutex_init(struct mutex *lock)
{
    if (lock) {
        lock->locked = 0;
    }
}
static inline void mutex_lock(struct mutex *lock)
{
    if (lock) {
        lock->locked = 1;
    }
}
static inline void mutex_unlock(struct mutex *lock)
{
    if (lock) {
        lock->locked = 0;
    }
}
struct rw_semaphore {
    int read_locked;
    int write_locked;
};
struct device {};
struct task_struct {
    int tgid;
    int pid;
};
struct p2p_page_table {
    u32 page_num;
    u64 page_size;
    struct {
        u64 pa;
    } *pages_info;
};
struct scatterlist {
    dma_addr_t dma_address;
    unsigned int length;
    unsigned int offset;
    unsigned long page_link;
};
struct sg_table {
    struct scatterlist *sgl;
    unsigned int nents;
    unsigned int orig_nents;
};
struct kprobe {
    const char *symbol_name;
    void *addr;
};

typedef int (*acquire_fn_t)(unsigned long, size_t, void *, char, void *);
typedef int (*get_pages_fn_t)(unsigned long, size_t, int, int, struct sg_table *, void *, u64);
typedef int (*dma_map_fn_t)(struct sg_table *, void *, struct device *, int, int *);
typedef int (*dma_unmap_fn_t)(struct sg_table *, void *, struct device *);
typedef void (*put_pages_fn_t)(struct sg_table *, void *);
typedef unsigned long (*get_page_size_fn_t)(void);
typedef void (*release_fn_t)(void *);
typedef int (*devmm_get_mem_pa_list_t)(void *, unsigned long, size_t, void *, int);  // 简化，实际参数是 &process_id 等

struct peer_memory_client {
    const char *name;
    const char *version;
    acquire_fn_t acquire;
    get_pages_fn_t get_pages;
    dma_map_fn_t dma_map;
    dma_unmap_fn_t dma_unmap;
    put_pages_fn_t put_pages;
    get_page_size_fn_t get_page_size;
    release_fn_t release;
};

extern struct task_struct *current;
extern struct rw_semaphore svm_context_sem;
extern bool g_fail_dma_map;
extern int g_dma_map_fail_index;
extern bool g_fail_kzalloc;
extern int g_hal_get_pages_ret_val;
extern int g_kfree_call_count;
extern int g_devmm_get_mem_pa_list_ret_val;
extern int g_devmm_call_count;
extern bool g_hal_get_pages_should_fail;
extern int g_mock_hal_get_pages_call_count;
extern int g_dma_map_call_count;
extern int g_dma_unmap_call_count;
extern bool g_fail_sg_alloc_table;
extern bool g_fail_kcalloc;
extern bool g_hal_put_pages_should_fail;
extern int g_hal_put_pages_ret_val;
extern int g_mock_hal_put_pages_call_count;

extern "C" {
int printk(const char *fmt, ...);
void *kzalloc(size_t size, int flags);
void *kcalloc(size_t n, size_t size, int flags);
void kfree(const void *objp);
void init_rwsem(struct rw_semaphore *sem);
void down_read(struct rw_semaphore *sem);
void up_read(struct rw_semaphore *sem);
void down_write(struct rw_semaphore *sem);
void up_write(struct rw_semaphore *sem);
int sg_alloc_table(struct sg_table *table, unsigned int nents, int gfp_mask);
void sg_free_table(struct sg_table *table);
dma_addr_t dma_map_resource(struct device *dev, u64 phys_addr, size_t size, int dir, unsigned long attrs);
void dma_unmap_resource(struct device *dev, dma_addr_t addr, size_t size, int dir, unsigned long attrs);
int dma_mapping_error(struct device *dev, dma_addr_t dma_addr);
void __module_get(void *module);
void module_put(void *module);
int register_kprobe(struct kprobe *p);
void unregister_kprobe(struct kprobe *p);
void *ib_register_peer_memory_client(const struct peer_memory_client *client, void *cb);
void ib_unregister_peer_memory_client(void *reg_handle);
int memset_s(void *dest, size_t destMax, int c, size_t count);
int mock_hal_get_pages(unsigned long addr, unsigned long len, void (*cb)(void *), void *data,
                       struct p2p_page_table **pt);
int mock_hal_put_pages(struct p2p_page_table *pt);
}

#define for_each_sg(sgl, sg, nents, i) for ((i) = 0, (sg) = (sgl); (i) < (nents); (i)++, (sg)++)
#define sg_dma_address(sg) ((sg)->dma_address)
#define sg_dma_len(sg) ((sg)->length)
#define sg_set_page(sg, page, len, offset)       \
    do {                                         \
        (sg)->page_link = (unsigned long)(page); \
        (sg)->length    = (len);                 \
        (sg)->offset    = (offset);              \
    } while (0)
#define WARN_ON(x) (x)
#define WARN_ONCE(x, ...) (x)
#define dev_err(...)
#define dev_name(x) "mock_dev"

#ifndef container_of
#define container_of(ptr, type, member)                        \
    ({                                                         \
        const __typeof__(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member));     \
    })
#endif