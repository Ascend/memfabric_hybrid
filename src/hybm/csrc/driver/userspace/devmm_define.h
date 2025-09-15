/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_DEVMM_DEFINE_H
#define MEM_FABRIC_HYBRID_DEVMM_DEFINE_H

#include <cstdint>

#define DEVMM_MAP_ALIGN_SIZE 0x200000U /* 2M */
#define DEVMM_HEAP_SIZE (1UL << 30UL) /* 1G */
#define DEVMM_SVM_MEM_SIZE (1UL << 43UL) /* 8T */
#define DEVMM_SVM_MEM_START 0x100000000000ULL

#define DV_ADVISE_HBM 0x0002
#define DV_ADVISE_HUGEPAGE 0x0004
#define DV_ADVISE_POPULATE 0x0008
#define DV_ADVISE_LOCK_DEV 0x0080
#define DV_ADVISE_MODULE_ID_BIT 24
#define DV_ADVISE_MODULE_ID_MASK 0xff
#define HCCL_HAL_MODULE_ID 3
#define APP_MODULE_ID 33

#define DEVMM_DDR_MEM 1
#define DEVMM_HEAP_HUGE_PAGE (0xEFEF0002UL)
#define DEVMM_HEAP_CHUNK_PAGE (0xEFEF0003UL)   // page_size is exchanged svm page_size

#define DEVMM_MAX_PHY_DEVICE_NUM 64

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(val, al) ((val) & ~((al) - 1))
#endif

#ifndef ALIGN_UP
#define ALIGN_UP(val, al) (((val) + ((al) - 1)) & ~((al) - 1))
#endif

#define MEM_SVM_VAL            0X0
#define MEM_DEV_VAL            0X1
#define MEM_HOST_VAL           0X2
#define MEM_DVPP_VAL           0X3
#define MEM_HOST_AGENT_VAL     0X4
#define MEM_RESERVE_VAL        0X5
#define MEM_MAX_VAL            0X6

enum devmm_heap_sub_type {
    SUB_SVM_TYPE = 0x0,     /* user mode page is same as kernel page, huge or chunk. the same as MEM_SVM_VAL */
    SUB_DEVICE_TYPE = 0x1,  /* user mode page is same as kernel page, just huge. the same as MEM_DEV_VAL */
    SUB_HOST_TYPE = 0x2,   /* user mode page is same as kernel page just chunk. the same as MEM_HOST_VAL */
    SUB_DVPP_TYPE = 0x3,   /* kernel page is huge, user mode page is chunk. the same as MEM_DVPP_VAL */
    SUB_READ_ONLY_TYPE = 0x4,  /* kernel page is huge, user mode page is chunk. MEM_DEV_VAL */
    SUB_RESERVE_TYPE = 0X5,    /* For halMemAddressReserve */
    SUB_DEV_READ_ONLY_TYPE = 0x6,  /* kernel page is huge, user mode page is chunk. MEM_DEV_VAL */
    SUB_MAX_TYPE
};

enum devmm_page_type {
    DEVMM_NORMAL_PAGE_TYPE = 0x0,
    DEVMM_HUGE_PAGE_TYPE,
    DEVMM_PAGE_TYPE_MAX
};

enum devmm_heap_list_type {
    SVM_LIST,
    HOST_LIST,
    DEVICE_AGENT0_LIST,
    DEVICE_AGENT63_LIST = DEVICE_AGENT0_LIST + DEVMM_MAX_PHY_DEVICE_NUM - 1,
    HOST_AGENT_LIST,
    RESERVE_LIST,
    HEAP_MAX_LIST,
};

struct devmm_virt_heap_type {
    uint32_t heap_type;
    uint32_t heap_list_type;
    uint32_t heap_sub_type;
    uint32_t heap_mem_type; /* A heap belongs to only one physical memory type. --devmm_mem_type */
};

struct svm_mem_stats_type {
    uint32_t mem_val;
    uint32_t page_type;
    uint32_t phy_memtype;
};

enum devmm_memtype {
    DEVMM_MEM_NORMAL = 0,
    DEVMM_MEM_RDONLY,
    DEVMM_MEMTYPE_MAX,
};

struct devmm_virt_com_heap;

struct devmm_com_heap_ops {
    unsigned long (*heap_alloc)(struct devmm_virt_com_heap *heap, unsigned long va, size_t size, uint32_t advise);
    int32_t (*heap_free)(struct devmm_virt_com_heap *heap, unsigned long ptr);
};

struct devmm_virt_list_head {
    struct devmm_virt_list_head *next, *prev;
};

enum devmm_mapped_rbtree_type {
    DEVMM_MAPPED_RW_TREE = 0,
    DEVMM_MAPPED_RDONLY_TREE,
    DEVMM_MAPPED_TREE_TYPE_MAX
};

struct list_node {
    struct list_node *next;
    struct list_node *prev;
};

struct devmm_node_data {
    uint64_t va;
    uint64_t size;
    uint64_t total;
    uint32_t flag;
};

struct rbtree_node {
    unsigned long rbtree_parent_color;
    struct rbtree_node *rbtree_right;
    struct rbtree_node *rbtree_left;
};

struct rbtree_root {
    struct rbtree_node *rbtree_node;
    uint64_t rbtree_len;
};

struct rb_node {
    struct rbtree_node rbtree_node;
    uint64_t key;
};

struct multi_rb_node {
    struct rb_node multi_rbtree_node;
    struct list_node list;
    uint8_t is_list_first;
};

struct devmm_rbtree_node {
    struct multi_rb_node va_node;
    struct multi_rb_node size_node;
    struct multi_rb_node cache_node;
    struct devmm_node_data data;
};

struct devmm_cache_list {
    struct devmm_rbtree_node cache;
    struct list_node list;
    uint8_t is_new;
};

struct devmm_heap_rbtree {
    struct rbtree_root *alloced_tree;
    struct rbtree_root *idle_size_tree;
    struct rbtree_root *idle_va_tree;
    struct rbtree_root *idle_mapped_cache_tree[DEVMM_MAPPED_TREE_TYPE_MAX];
    struct devmm_cache_list *head;
    uint32_t devmm_cache_numsize;
};

struct devmm_heap_list {
    int heap_cnt;
    pthread_rwlock_t list_lock;
    struct devmm_virt_list_head heap_list;
};

struct devmm_virt_com_heap {
    uint32_t inited;
    uint32_t heap_type;
    uint32_t heap_sub_type;
    uint32_t heap_list_type;
    uint32_t heap_mem_type;
    uint32_t heap_idx;
    bool is_base_heap;

    uint64_t cur_cache_mem[DEVMM_MEMTYPE_MAX]; /* current cached mem */
    uint64_t cache_mem_thres[DEVMM_MEMTYPE_MAX]; /* cached mem threshold */
    uint64_t cur_alloc_cache_mem[DEVMM_MEMTYPE_MAX]; /* current alloc can cache total mem */
    uint64_t peak_alloc_cache_mem[DEVMM_MEMTYPE_MAX]; /* peak alloc can cache */
    time_t peak_alloc_cache_time[DEVMM_MEMTYPE_MAX]; /* the time peak alloc can cache */
    uint32_t need_cache_thres[DEVMM_MEMTYPE_MAX]; /* alloc size need to cache threshold */
    bool is_limited;    /* true: this kind of heap is resource-limited, not allowd to be alloced another new heap.
                           The heap's cache will be shrinked forcibly, when it's not enough for nocache's allocation */
    bool is_cache;      /* true: follow the cache rule, devmm_get_free_threshold_by_type, used by normal heap.
                           false: no cache, free the heap immediately, used by specified va alloc. For example,
                                  alloc 2M success, free 2M success, alloc 2G will fail because of cache heap. */
    unsigned long start;
    unsigned long end;

    uint32_t module_id;     /* used for large heap (>=512M) */
    uint32_t side;          /* used for large heap (>=512M) */
    uint32_t devid;         /* used for large heap (>=512M) */
    uint64_t mapped_size;

    uint32_t chunk_size;
    uint32_t kernel_page_size;  /* get from kernel */
    uint32_t map_size;
    uint64_t heap_size;

    struct devmm_com_heap_ops *ops;
    pthread_mutex_t tree_lock;
    pthread_rwlock_t heap_rw_lock;
    uint64_t sys_mem_alloced;
    uint64_t sys_mem_freed;
    uint64_t sys_mem_alloced_num;
    uint64_t sys_mem_freed_num;

    struct devmm_virt_list_head list;       /* associated to base heap's devmm_heap_list */
    struct devmm_heap_rbtree rbtree_queue;
};

#endif // MEM_FABRIC_HYBRID_DEVMM_DEFINE_H