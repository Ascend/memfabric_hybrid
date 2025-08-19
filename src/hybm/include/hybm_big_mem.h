/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_BIG_MEM_C_H
#define MEM_FABRIC_HYBRID_HYBM_BIG_MEM_C_H

#include <stddef.h>
#include "hybm.h"
#include "hybm_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create Hybrid Big Memory(HyBM) entity at local side
 *
 * @param id               [in] identify for new entity
 * @param options          [in] options of the HyBM entity
 * @param flags            [in] optional flags, default value 0
 * @return entity if created, nullptr if failed to create
 */
hybm_entity_t hybm_create_entity(uint16_t id, const hybm_options *options, uint32_t flags);

/**
 * @brief Destroy HyBM entity
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param flags            [in] optional flags, default value 0
 */
void hybm_destroy_entity(hybm_entity_t e, uint32_t flags);

/**
 * @brief Reserve virtual memory space at local side
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param flags            [in] optional flags, default value 0
 * @param reservedMem      [out] pointer of reserved virtual space
 * @return 0 if reserved successful
 */
int32_t hybm_reserve_mem_space(hybm_entity_t e, uint32_t flags, void **reservedMem);

/**
 * @brief Un-reserve virtual memory space at local side
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param flags            [in] optional flags, default value 0
 * @param reservedMem      [int] pointer of reserved virtual space to be un-reserved
 * @return 0 if reserved successful
 */
int32_t hybm_unreserve_mem_space(hybm_entity_t e, uint32_t flags, void *reservedMem);

/**
 * @brief Allocate memory at local side(mmap local memory)
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param mType            [in] memory type, device or host
 * @param size             [in] size of memory
 * @param flags            [in] optional flags, default value 0
 * @return mem slice handle if successful, null ptr if failed
 */
hybm_mem_slice_t hybm_alloc_local_memory(hybm_entity_t e, hybm_mem_type mType, uint64_t size, uint32_t flags);

/**
 * @brief Free memory at local side
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param slice            [in] pointer of mem slice array, could be null if free all
 * @param count            [in] count of mem slice array, could be 0 if free all
 * @param flags            [in] HYBM_FREE_PARTIAL_SLICE or HYBM_FREE_ALL_SLICE
 * @return 0 if successful, error code if failed
 */
int32_t hybm_free_local_memory(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t count, uint32_t flags);

/**
 * @brief Register memory at local side, registered memory can be accessed by remote.
 * @param e                [in] entity created by hybm_create_entity
 * @param mType            [in] memory type, device or host
 * @param ptr              [in] local memory start address
 * @param size             [in] size of memory
 * @param flags            [in] optional flags, default value 0
 * @return mem slice handle if successful, null ptr if failed
 */
hybm_mem_slice_t hybm_register_local_memory(hybm_entity_t e, hybm_mem_type mType, const void *ptr, uint64_t size,
                                            uint32_t flags);

/**
 * @brief Export exchange info for peer to import
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param slice            [in] slice to export, could be null if export all
 * @param flags            [in] HYBM_EXPORT_SINGLE_SLICE | HYBM_EXPORT_ALL_SLICE
 * @param exInfo           [out] exchange info to be filled in
 * @return 0 if successful, error code if failed
 */
int32_t hybm_export(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t flags, hybm_exchange_info *exInfo);

/**
 * @brief get fixed size for export slice size
 * @param e               [in] entity created by hybm_create_entity
 * @param size            [out] fixed size when export slice
 * @return 0 if successful, error code if failed
 */
int32_t hybm_export_slice_size(hybm_entity_t e, size_t *size);

/**
 * @brief Import batch of exchange info of other HyBM entities
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param allExInfo        [in] ptr of entities array
 * @param count            [in] count of entities
 * @param addresses        [out] import slices got local virtual address, can be null if not care
 * @param flags            [in] optional flags, default value 0
 * @return 0 if successful
 */
int32_t hybm_import(hybm_entity_t e, const hybm_exchange_info allExInfo[], uint32_t count, void *addresses[],
                    uint32_t flags);

/**
 * @brief Export exchange info for peer to import
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param flags            [in] HYBM_EXPORT_SINGLE_SLICE | HYBM_EXPORT_ALL_SLICE
 * @param exInfo           [out] exchange info to be filled in
 * @return 0 if successful, error code if failed
 */
int32_t hybm_entity_export(hybm_entity_t e, uint32_t flags, hybm_exchange_info *exInfo);

/**
 * @brief Import batch of exchange info of other HyBM entities
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param allExInfo        [in] ptr of entities array
 * @param count            [in] count of entities
 * @param flags            [in] optional flags, default value 0
 * @return 0 if successful
 */
int32_t hybm_entity_import(hybm_entity_t e, const hybm_exchange_info allExInfo[], uint32_t count, uint32_t flags);

/**
 * @brief mmap all memory which is imported
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param flags            [in] optional flags, default value 0
 * @return 0 if successful, error code if failed
 */
int32_t hybm_mmap(hybm_entity_t e, uint32_t flags);

/**
 * @brief Determine whether various channels from local rank to the remote is reachable.
 * @param e                [in] entity created by hybm_create_entity
 * @param rank             [in] remote rank
 * @param reaches          [out] array of size HYBM_DOP_TYPE_BUTT, for storing reachable results of various channels.
 *                               0 means unreachable, otherwise means reachable.
 * @param flags            [in] optional flags, default value 0
 * @return 0 if successful, error code if failed
 */
int32_t hybm_entity_reach_types(hybm_entity_t e, uint32_t rank, int32_t reaches[HYBM_DOP_TYPE_BUTT], uint32_t flags);

/**
 * @brief join one rank after start
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param rank             [in] join rank
 * @param flags            [in] optional flags, default value 0
 * @return 0 if successful, error code if failed
 */
int32_t hybm_join(hybm_entity_t e, uint32_t rank, uint32_t flags);

/**
 * @brief leave one rank after start
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param rank             [in] leave rank
 * @param flags            [in] optional flags, default value 0
 * @return 0 if successful, error code if failed
 */
int32_t hybm_remove_imported(hybm_entity_t e, uint32_t rank, uint32_t flags);

/**
 * @brief write user extra context into the fixed addr of the hbm
 *
 * @param e                [in] entity created by hybm_create_entity
 * @param context          [in] user extra context addr
 * @param size             [in] user extra context size
 * @return 0 if successful
 */
int32_t hybm_set_extra_context(hybm_entity_t e, const void *context, uint32_t size);

/**
 * @brief unmap the entity
 *
 * @param e                 [in] entity created by hybm_create_entity
 * @param flags             [in] optional flags, default value 0
 */
void hybm_unmap(hybm_entity_t e, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif // MEM_FABRIC_HYBRID_HYBM_BIG_MEM_C_H
