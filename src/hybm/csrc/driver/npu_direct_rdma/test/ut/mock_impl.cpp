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
#include "mock_env.h"

struct task_struct g_current_task = {100, 101};
struct task_struct *current       = &g_current_task;

bool g_fail_dma_map                 = false;
int g_dma_map_fail_index            = -1;
int g_dma_map_call_count            = 0;
int g_dma_unmap_call_count          = 0;
bool g_fail_kzalloc                 = false;
bool g_fail_kcalloc                 = false;
int g_hal_get_pages_ret_val         = 0;
int g_kfree_call_count              = 0;
bool g_hal_get_pages_should_fail    = false;
int g_mock_hal_get_pages_call_count = 0;
bool g_fail_sg_alloc_table          = false;

bool g_hal_put_pages_should_fail    = false;
int g_hal_put_pages_ret_val         = 0;
int g_mock_hal_put_pages_call_count = 0;
#define ENOMEM 12

typedef void (*free_callback_t)(void *);
int g_devmm_get_mem_pa_list_ret_val = 0;
int g_devmm_call_count              = 0;

extern "C" {
int printk(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}

void *kzalloc(size_t size, int flags)
{
    (void)flags;
    if (g_fail_kzalloc) {
        return NULL;
    }
    return calloc(1, size);
}

void *kcalloc(size_t n, size_t size, int flags)
{
    (void)flags;

    if (g_fail_kcalloc) {
        return NULL;
    }

    if (n == 0 || size == 0) {
        return NULL;
    }

    if (size > SIZE_MAX / n) {
        return NULL;
    }

    return calloc(n, size);
}

void kfree(const void *objp)
{
    if (objp) {
        g_kfree_call_count++;
        free((void *)objp);
    }
}

void init_rwsem(struct rw_semaphore *sem)
{
    if (sem) {
        sem->read_locked  = 0;
        sem->write_locked = 0;
    }
}

void down_read(struct rw_semaphore *sem)
{
    (void)sem;
}
void up_read(struct rw_semaphore *sem)
{
    (void)sem;
}
void down_write(struct rw_semaphore *sem)
{
    (void)sem;
}
void up_write(struct rw_semaphore *sem)
{
    (void)sem;
}
int sg_alloc_table(struct sg_table *table, unsigned int nents, int gfp_mask)
{
    (void)gfp_mask;

    if (g_fail_sg_alloc_table) {
        return -ENOMEM;
    }

    if (nents == 0) {
        return -ENOMEM;
    }

    if (nents > SIZE_MAX / sizeof(struct scatterlist)) {
        return -ENOMEM;
    }

    table->nents = nents;
    table->sgl = (struct scatterlist *)calloc(nents, sizeof(struct scatterlist));
    if (!table->sgl) {
        return -ENOMEM;
    }
    return 0;
}
void sg_free_table(struct sg_table *table)
{
    if (table->sgl) {
        free(table->sgl);
    }

    table->sgl = NULL;
}
dma_addr_t dma_map_resource(struct device *dev, u64 phys_addr, size_t size, int dir, unsigned long attrs)
{
    (void)dev;
    (void)size;
    (void)dir;
    (void)attrs;
    if (g_fail_dma_map && g_dma_map_call_count == g_dma_map_fail_index) {
        g_dma_map_call_count++;
        return DMA_MAPPING_ERROR;
    }
    g_dma_map_call_count++;
    return (dma_addr_t)(phys_addr + 0x10000);
}
int dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
    (void)dev;
    return dma_addr == DMA_MAPPING_ERROR;
}
void dma_unmap_resource(struct device *dev, dma_addr_t addr, size_t size, int dir, unsigned long attrs)
{
    (void)dev;
    (void)addr;
    (void)size;
    (void)dir;
    (void)attrs;
    g_dma_unmap_call_count++;
}
void __module_get(void *module)
{
    (void)module;
}
void module_put(void *module)
{
    (void)module;
}
int register_kprobe(struct kprobe *p)
{
    (void)p;
    return 0;
}
void unregister_kprobe(struct kprobe *p)
{
    (void)p;
}
void *ib_register_peer_memory_client(const struct peer_memory_client *client, void *cb)
{
    (void)client;
    (void)cb;
    return (void *)0x1234;
}
void ib_unregister_peer_memory_client(void *reg_handle)
{
    (void)reg_handle;
}
int memset_s(void *dest, size_t destMax, int c, size_t count)
{
    (void)destMax;
    if (!dest) {
        return -1;
    }

    memset(dest, c, count);
    return 0;
}
int mock_hal_get_pages(unsigned long addr, unsigned long len, void (*cb)(void *), void *data,
                       struct p2p_page_table **pt)
{
    (void)addr;
    (void)len;
    (void)cb;
    (void)data;
    g_mock_hal_get_pages_call_count++;

    if (g_hal_get_pages_should_fail) {
        *pt = NULL;
        return g_hal_get_pages_ret_val;
    }

    *pt = (struct p2p_page_table *)calloc(1, sizeof(struct p2p_page_table));
    if (!*pt) {
        return -ENOMEM;
    }

    (*pt)->page_size  = 4096;
    (*pt)->page_num   = len / 4096;
    (*pt)->pages_info = (decltype((*pt)->pages_info))calloc((*pt)->page_num, sizeof(u64));
    for (u32 i = 0; i < (*pt)->page_num; i++) {
        (*pt)->pages_info[i].pa = 0x10000000 + i * 4096;
    }

    return 0;
}
int mock_hal_put_pages(struct p2p_page_table *pt)
{
    g_mock_hal_put_pages_call_count++;

    if (g_hal_put_pages_should_fail) {
        return g_hal_put_pages_ret_val;
    }

    if (pt) {
        if (pt->pages_info) {
            free(pt->pages_info);
        }

        free(pt);
    }
    return 0;
}

int g_devmmGetMemPaList(struct process_id *pid, unsigned long va_start, size_t aligned_size, void *pa_list, int pa_num)
{
    (void)pid;
    (void)va_start;
    (void)aligned_size;
    (void)pa_list;
    (void)pa_num;
    g_devmm_call_count++;
    return g_devmm_get_mem_pa_list_ret_val;
}
}