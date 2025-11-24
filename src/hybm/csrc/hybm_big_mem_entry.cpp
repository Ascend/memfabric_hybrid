/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hybm_logger.h"
#include "hybm_entity_factory.h"
#include "hybm_big_mem.h"
#include "dl_hal_api.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_gvm_user.h"
#include "mf_num_util.h"

using namespace ock::mf;

HYBM_API hybm_entity_t hybm_create_entity(uint16_t id, const hybm_options *options, uint32_t flags)
{
    BM_ASSERT_RETURN(HybmHasInited(), nullptr);

    auto &factory = MemEntityFactory::Instance();
    auto entity = factory.GetOrCreateEngine(id, flags);
    if (entity == nullptr) {
        BM_LOG_ERROR("create entity failed.");
        return nullptr;
    }

    auto ret = entity->Initialize(options);
    if (ret != 0) {
        MemEntityFactory::Instance().RemoveEngine(entity.get());
        BM_LOG_ERROR("initialize entity failed: " << ret);
        return nullptr;
    }

    return entity.get();
}

HYBM_API void hybm_destroy_entity(hybm_entity_t e, uint32_t flags)
{
    BM_ASSERT_RET_VOID(e != nullptr);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RET_VOID(entity != nullptr);
    entity->UnInitialize();
    MemEntityFactory::Instance().RemoveEngine(e);
}

HYBM_API int32_t hybm_reserve_mem_space(hybm_entity_t e, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->ReserveMemorySpace();
}

HYBM_API int32_t hybm_unreserve_mem_space(hybm_entity_t e, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->UnReserveMemorySpace();
}

HYBM_API void *hybm_get_memory_ptr(hybm_entity_t e, hybm_mem_type mType)
{
    auto entity = static_cast<MemEntity *>(e);
    BM_ASSERT_RETURN(entity != nullptr, nullptr);
    return entity->GetReservedMemoryPtr(mType);
}

HYBM_API hybm_mem_slice_t hybm_alloc_local_memory(hybm_entity_t e, hybm_mem_type mType, uint64_t size, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, nullptr);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, nullptr);
    hybm_mem_slice_t slice;
    auto ret = entity->AllocLocalMemory(size, mType, flags, slice);
    if (ret != 0) {
        BM_LOG_ERROR("allocate slice with size: " << size << ", mType: " << mType << " failed: " << ret);
        return nullptr;
    }

    return slice;
}

HYBM_API int32_t hybm_free_local_memory(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t count, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(slice != nullptr, BM_INVALID_PARAM);
    return entity->FreeLocalMemory(slice, flags);
}

HYBM_API hybm_mem_slice_t hybm_register_local_memory(hybm_entity_t e, hybm_mem_type mType, const void *ptr,
                                                     uint64_t size, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, nullptr);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, nullptr);

    hybm_mem_slice_t slice;
    auto ret = entity->RegisterLocalMemory(ptr, size, flags, slice);
    if (ret != 0) {
        BM_LOG_ERROR("register slice with size: " << size << " failed: " << ret);
        return nullptr;
    }

    return slice;
}

HYBM_API int32_t hybm_export(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t flags, hybm_exchange_info *exInfo)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(exInfo != nullptr, BM_INVALID_PARAM);

    ExchangeInfoWriter writer(exInfo);
    auto ret = entity->ExportExchangeInfo(slice, writer, flags);
    if (ret != 0) {
        BM_LOG_ERROR("export slices: " << slice << " failed: " << ret);
        return ret;
    }

    return BM_OK;
}

HYBM_API int32_t hybm_export_slice_size(hybm_entity_t e, size_t *size)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(size != nullptr, BM_INVALID_PARAM);

    auto ret = entity->GetExportSliceInfoSize(*size);
    return ret;
}

HYBM_API int32_t hybm_import(hybm_entity_t e, const hybm_exchange_info allExInfo[], uint32_t count, void *addresses[],
                             uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(allExInfo != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(count > 0, BM_INVALID_PARAM);

    std::vector<ExchangeInfoReader> readers(count);
    for (auto i = 0U; i < count; i++) {
        readers[i].Reset(allExInfo + i);
    }

    return entity->ImportExchangeInfo(readers.data(), count, addresses, flags);
}

HYBM_API int32_t hybm_entity_export(hybm_entity_t e, uint32_t flags, hybm_exchange_info *exInfo)
{
    auto entity = static_cast<MemEntity *>(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(exInfo != nullptr, BM_INVALID_PARAM);

    ExchangeInfoWriter writer(exInfo);
    auto ret = entity->ExportExchangeInfo(writer, 0);
    if (ret != 0) {
        BM_LOG_ERROR("export entity data failed: " << ret);
        return ret;
    }

    return BM_OK;
}

HYBM_API int32_t hybm_entity_import(hybm_entity_t e, const hybm_exchange_info allExInfo[], uint32_t count,
                                    uint32_t flags)
{
    auto entity = static_cast<MemEntity *>(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(allExInfo != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(count > 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(count <= RANK_MAX, BM_INVALID_PARAM);

    std::vector<ExchangeInfoReader> readers(count);
    for (auto i = 0U; i < count; i++) {
        readers[i].Reset(allExInfo + i);
    }

    return entity->ImportEntityExchangeInfo(readers.data(), count, flags);
}

HYBM_API int32_t hybm_mmap(hybm_entity_t e, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->Mmap();
}

HYBM_API int32_t hybm_entity_reach_types(hybm_entity_t e, uint32_t rank, hybm_data_op_type &reachTypes, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);

    reachTypes = entity->CanReachDataOperators(rank);
    return BM_OK;
}

HYBM_API int32_t hybm_remove_imported(hybm_entity_t e, uint32_t rank, uint32_t flags)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);

    std::vector<uint32_t> ranks = {rank};
    return entity->RemoveImported(ranks);
}

HYBM_API int32_t hybm_set_extra_context(hybm_entity_t e, const void *context, uint32_t size)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(context != nullptr, BM_INVALID_PARAM);
    return entity->SetExtraContext(context, size);
}

HYBM_API void hybm_unmap(hybm_entity_t e, uint32_t flags)
{
    BM_ASSERT_RET_VOID(e != nullptr);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RET_VOID(entity != nullptr);
    entity->Unmap();
}

HYBM_API int32_t hybm_register_user_mem(hybm_entity_t e, uint64_t addr, uint64_t size)
{
    BM_ASSERT_RETURN(e != nullptr, BM_INVALID_PARAM);
    auto entity = MemEntityFactory::Instance().FindEngineByPtr(e);
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->RegisterMem(addr, size);
}

void *hybm_host_mem_malloc(uint64_t size, uint64_t flags)
{
    BM_ASSERT_RETURN(HybmHasInited(), nullptr);
    BM_ASSERT_RETURN(size != 0, nullptr);
    return (void *)hybm_gvm_user_alloc(size);
}

void hybm_host_mem_free(void *addr)
{
    BM_ASSERT_RET_VOID(HybmHasInited());
    BM_ASSERT_RET_VOID(addr != nullptr);
    return hybm_gvm_user_free((uint64_t)addr);
}

int32_t hybm_host_mem_register(uint64_t src, uint64_t size, uint64_t *dest)
{
    BM_ASSERT_RETURN(HybmHasInited(), BM_ERROR);
    BM_ASSERT_RETURN(src != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(size != 0, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(dest != nullptr, BM_INVALID_PARAM);

    auto ret = DlHalApi::HalHostRegister((void *)src, size, HOST_MEM_MAP_DEV, HybmGetInitDeviceId(), (void **)dest);
    if (ret != 0) {
        BM_LOG_ERROR("register host memory to device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

int32_t hybm_host_mem_unregister(uint64_t src)
{
    BM_ASSERT_RETURN(HybmHasInited(), BM_ERROR);
    BM_ASSERT_RETURN(src != 0, BM_INVALID_PARAM);

    auto ret = DlHalApi::HalHostUnregisterEx((void *)src, HybmGetInitDeviceId(), HOST_MEM_MAP_DEV);
    if (ret != 0) {
        BM_LOG_ERROR("unregister host memory to device failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}
