/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/

#ifndef MEM_FABRIC_HYBRID_DEVMM_DEFINE_H
#define MEM_FABRIC_HYBRID_DEVMM_DEFINE_H

#include <cstdint>

constexpr uint64_t DEVMM_MAP_ALIGN_SIZE = 0x200000U;
constexpr uint64_t DEVMM_HEAP_SIZE = (1UL << 30UL);
constexpr size_t DEVMM_SVM_MEM_SIZE = (1UL << 43UL);
constexpr uint64_t DEVMM_SVM_MEM_START = 0x100000000000ULL;
constexpr uint32_t DEVMM_MAX_HEAP_NUM = (DEVMM_SVM_MEM_SIZE >> 30);

constexpr uint32_t DV_ADVISE_DDR = 0x0001;
constexpr uint32_t DV_ADVISE_HBM = 0x0002;
constexpr uint32_t DV_ADVISE_HUGEPAGE = 0x0004;
constexpr uint32_t DV_ADVISE_GIANTPAGE = 0x8000;
constexpr uint32_t DV_ADVISE_POPULATE = 0x0008;
constexpr uint32_t DV_ADVISE_LOCK_DEV = 0x0080;
constexpr uint32_t DV_ADVISE_MODULE_ID_BIT = 24;
constexpr uint32_t DV_ADVISE_MODULE_ID_MASK = 0xff;
constexpr uint32_t HCCL_HAL_MODULE_ID = 3;
constexpr uint32_t APP_MODULE_ID = 33;

constexpr uint32_t DEVMM_NODE_MAPPED_BIT = 1;
constexpr uint32_t DEVMM_NODE_MEMTYPE_SHIFT = 2;
constexpr uint32_t DEVMM_NODE_MEMTYPE_WID = 4;

constexpr uint32_t DEVMM_NODE_MAPPED_FLG = (1UL << DEVMM_NODE_MAPPED_BIT);
constexpr uint32_t DEVMM_HEAP_HUGE_PAGE = 0xEFEF0002UL;
constexpr uint32_t DEVMM_HEAP_CHUNK_PAGE = 0xEFEF0003UL;

constexpr uint32_t DEVMM_MAX_PHY_DEVICE_NUM = 64;
constexpr uint32_t SVM_MAX_AGENT_NUM = 65;

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(val, al) ((val) & ~((al) - 1))
#endif

#ifndef ALIGN_UP
#define ALIGN_UP(val, al) (((val) + ((al) - 1)) & ~((al) - 1))
#endif

enum MemVal {
    MEM_SVM_VAL          = 0X0,
    MEM_DEV_VAL          = 0X1,
    MEM_HOST_VAL         = 0X2,
    MEM_DVPP_VAL         = 0X3,
    MEM_HOST_AGENT_VAL   = 0X4,
    MEM_RESERVE_VAL      = 0X5,
    MEM_MAX_VAL          = 0X6
};

enum DevHeapSubType {
    SUB_SVM_TYPE = 0x0,     /* user mode page is same as kernel page, huge or chunk. the same as MEM_SVM_VAL */
    SUB_DEVICE_TYPE = 0x1,  /* user mode page is same as kernel page, just huge. the same as MEM_DEV_VAL */
    SUB_HOST_TYPE = 0x2,   /* user mode page is same as kernel page just chunk. the same as MEM_HOST_VAL */
    SUB_DVPP_TYPE = 0x3,   /* kernel page is huge, user mode page is chunk. the same as MEM_DVPP_VAL */
    SUB_READ_ONLY_TYPE = 0x4,  /* kernel page is huge, user mode page is chunk. MEM_DEV_VAL */
    SUB_RESERVE_TYPE = 0X5,    /* For halMemAddressReserve */
    SUB_DEV_READ_ONLY_TYPE = 0x6,  /* kernel page is huge, user mode page is chunk. MEM_DEV_VAL */
    SUB_MAX_TYPE
};

enum DevMemType {
    DEVMM_HBM_MEM = 0x0,
    DEVMM_DDR_MEM,
    DEVMM_P2P_HBM_MEM,
    DEVMM_P2P_DDR_MEM,
    DEVMM_TS_DDR_MEM,
    DEVMM_MEM_TYPE_MAX
};

enum DevPageType {
    DEVMM_NORMAL_PAGE_TYPE = 0x0,
    DEVMM_HUGE_PAGE_TYPE,
    DEVMM_PAGE_TYPE_MAX
};

enum DevHeapListType {
    SVM_LIST,
    HOST_LIST,
    DEVICE_AGENT0_LIST,
    DEVICE_AGENT63_LIST = DEVICE_AGENT0_LIST + DEVMM_MAX_PHY_DEVICE_NUM - 1,
    HOST_AGENT_LIST,
    RESERVE_LIST,
    HEAP_MAX_LIST,
};

struct DevVirtHeapType {
    uint32_t heap_type;
    uint32_t heap_list_type;
    uint32_t heap_sub_type;
    uint32_t heap_mem_type; /* A heap belongs to only one physical memory type. --DevMemType */
};

struct MemStatsType {
    uint32_t mem_val;
    uint32_t page_type;
    uint32_t phy_memtype;
};

enum DMemType {
    DEVMM_MEM_NORMAL = 0,
    DEVMM_MEM_RDONLY,
    DEVMM_MEMTYPE_MAX,
};

struct DevVirtComHeap;

struct DComHeapOps {
    uint64_t (*heap_alloc)(struct DevVirtComHeap *heap, uint64_t va, size_t size, uint32_t advise);
    int32_t (*heap_free)(struct DevVirtComHeap *heap, uint64_t ptr);
};

struct DVirtListHead {
    struct DVirtListHead *next, *prev;
};

enum DMappedRbtreeType {
    DEVMM_MAPPED_RW_TREE = 0,
    DEVMM_MAPPED_RDONLY_TREE,
    DEVMM_MAPPED_TREE_TYPE_MAX
};

struct ListNode {
    struct ListNode *next;
    struct ListNode *prev;
};

struct DevNodeData {
    uint64_t va;
    uint64_t size;
    uint64_t total;
    uint32_t flag;
};

struct RbtreeNode {
    unsigned long rbtree_parent_color;
    struct RbtreeNode *rbtree_right;
    struct RbtreeNode *rbtree_left;
};

struct RbtreeRoot {
    struct RbtreeNode *RbtreeNode;
    uint64_t rbtree_len;
};

struct RbNode {
    struct RbtreeNode RbtreeNode;
    uint64_t key;
};

struct MultiRbNode {
    struct RbNode multi_rbtree_node;
    struct ListNode list;
    uint8_t is_list_first;
};

struct DevRbtreeNode {
    struct MultiRbNode va_node;
    struct MultiRbNode size_node;
    struct MultiRbNode cache_node;
    struct DevNodeData data;
};

struct DCacheList {
    struct DevRbtreeNode cache;
    struct ListNode list;
    uint8_t is_new;
};

struct DevHeapRbtree {
    struct RbtreeRoot *alloced_tree;
    struct RbtreeRoot *idle_size_tree;
    struct RbtreeRoot *idle_va_tree;
    struct RbtreeRoot *idle_mapped_cache_tree[DEVMM_MAPPED_TREE_TYPE_MAX];
    struct DCacheList *head;
    uint32_t devmm_cache_numsize;
};

struct DevHeapList {
    int heap_cnt;
    pthread_rwlock_t list_lock;
    struct DVirtListHead heap_list;
};

struct DevVirtComHeap {
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
    uint64_t start;
    uint64_t end;

    uint32_t module_id;     /* used for large heap (>=512M) */
    uint32_t side;          /* used for large heap (>=512M) */
    uint32_t devid;         /* used for large heap (>=512M) */
    uint64_t mapped_size;

    uint32_t chunk_size;
    uint32_t kernel_page_size;  /* get from kernel */
    uint32_t map_size;
    uint64_t heap_size;

    struct DComHeapOps *ops;
    pthread_mutex_t tree_lock;
    pthread_rwlock_t heap_rw_lock;
    uint64_t sys_mem_alloced;
    uint64_t sys_mem_freed;
    uint64_t sys_mem_alloced_num;
    uint64_t sys_mem_freed_num;

    struct DVirtListHead list;       /* associated to base heap's DevHeapList */
    struct DevHeapRbtree rbtree_queue;
};

struct DHeapQueue {
    struct DevVirtComHeap base_heap; /* use for manage 32T heap, heap range 1g */
    struct DevVirtComHeap *heaps[DEVMM_MAX_HEAP_NUM];
};

struct DevVirtHeapMgmt {
    uint32_t inited;
    pid_t pid;

    uint64_t max_conti_size; /* eq heap size */

    uint64_t start; /* svm page_size aligned */
    uint64_t end;   /* svm page_size aligned */

    uint64_t dvpp_start;  /* dvpp vaddr start */
    uint64_t dvpp_end;    /* dvpp vaddr end */
    uint64_t dvpp_mem_size[DEVMM_MAX_PHY_DEVICE_NUM];

    uint64_t read_only_start;  /* read vaddr start */
    uint64_t read_only_end;    /* read vaddr end */

    uint32_t svm_page_size;
    uint32_t local_page_size;
    uint32_t huge_page_size;
    bool support_bar_mem[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_dev_read_only[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_dev_mem_map_host[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_bar_huge_mem[DEVMM_MAX_PHY_DEVICE_NUM];
    bool host_support_pin_user_pages_interface;
    bool support_host_rw_dev_ro;
    uint64_t double_pgtable_offset[DEVMM_MAX_PHY_DEVICE_NUM];

    struct DHeapQueue heap_queue;
    struct DevHeapList huge_list[HEAP_MAX_LIST][SUB_MAX_TYPE][DEVMM_MEM_TYPE_MAX];
    struct DevHeapList normal_list[HEAP_MAX_LIST][SUB_MAX_TYPE][DEVMM_MEM_TYPE_MAX];
};

struct DevVirtHeapMgmtV2 {
    uint32_t inited;
    pid_t pid;

    uint64_t max_conti_size; /* eq heap size */

    uint64_t start; /* svm page_size aligned */
    uint64_t end;   /* svm page_size aligned */

    uint64_t dvpp_start;  /* dvpp vaddr start */
    uint64_t dvpp_end;    /* dvpp vaddr end */
    uint64_t dvpp_mem_size[DEVMM_MAX_PHY_DEVICE_NUM];

    uint64_t read_only_start;  /* read vaddr start */
    uint64_t read_only_end;    /* read vaddr end */

    uint32_t svm_page_size;
    uint32_t local_page_size;
    uint32_t huge_page_size;
    bool support_bar_mem[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_dev_read_only[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_dev_mem_map_host[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_bar_huge_mem[DEVMM_MAX_PHY_DEVICE_NUM];
    bool host_support_pin_user_pages_interface;
    bool support_host_rw_dev_ro;
    uint64_t double_pgtable_offset[DEVMM_MAX_PHY_DEVICE_NUM];

    bool support_host_pin_pre_register;
    bool support_host_mem_pool;
    bool is_dev_inited[SVM_MAX_AGENT_NUM];

    struct DHeapQueue heap_queue;
    struct DevHeapList huge_list[HEAP_MAX_LIST][SUB_MAX_TYPE][DEVMM_MEM_TYPE_MAX];
    struct DevHeapList normal_list[HEAP_MAX_LIST][SUB_MAX_TYPE][DEVMM_MEM_TYPE_MAX];
};

#endif // MEM_FABRIC_HYBRID_DEVMM_DEFINE_H