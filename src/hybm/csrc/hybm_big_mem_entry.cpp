/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_logger.h"
#include "hybm_entity_factory.h"
#include "hybm_big_mem.h"

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
        BM_LOG_ERROR("initialize entity failed: " << ret);
        return nullptr;
    }

    return entity.get();
}

HYBM_API void hybm_destroy_entity(hybm_entity_t e, uint32_t flags)
{
    MemEntityFactory::Instance().RemoveEngine(e);
}

HYBM_API int32_t hybm_reserve_mem_space(hybm_entity_t e, uint32_t flags, void **reservedMem)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->ReserveMemorySpace(reservedMem);
}

HYBM_API int32_t hybm_unreserve_mem_space(hybm_entity_t e, uint32_t flags, void *reservedMem)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->UnReserveMemorySpace();
}

HYBM_API hybm_mem_slice_t hybm_alloc_local_memory(hybm_entity_t e, hybm_mem_type mType, uint64_t size, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    hybm_mem_slice_t slice;
    BM_ASSERT_RETURN(entity != nullptr, nullptr);
    auto ret = entity->AllocLocalMemory(size, flags, slice);
    if (ret != 0) {
        BM_LOG_ERROR("allocate slice with size: " << size << " failed: " << ret);
        return nullptr;
    }

    return slice;
}

HYBM_API int32_t hybm_free_local_memory(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t count, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    entity->FreeLocalMemory(slice, flags);
    return 0;
}

HYBM_API int32_t hybm_export(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t flags, hybm_exchange_info *exInfo)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    auto ret = entity->ExportExchangeInfo(slice, *exInfo, flags);
    if (ret != 0) {
        BM_LOG_ERROR("export slices: " << slice << " failed: " << ret);
        return ret;
    }

    return BM_OK;
}

HYBM_API int32_t hybm_import(hybm_entity_t e, const hybm_exchange_info allExInfo[], uint32_t count, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->ImportExchangeInfo(allExInfo, count, flags);
}

HYBM_API int32_t hybm_mmap(hybm_entity_t e, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->Mmap();
}

HYBM_API int32_t hybm_join(hybm_entity_t e, uint32_t rank, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->Join(rank);
}

HYBM_API int32_t hybm_leave(hybm_entity_t e, uint32_t rank, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->Leave(rank);
}

HYBM_API int32_t hybm_set_extra_context(hybm_entity_t e, const void *context, uint32_t size)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->SetExtraContext(context, size);
}

HYBM_API int32_t hybm_start(hybm_entity_t e, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->Start(flags);
}

HYBM_API void hybm_stop(hybm_entity_t e, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RET_VOID(entity != nullptr);
    entity->Stop();
}