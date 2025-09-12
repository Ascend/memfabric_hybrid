/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "hybm_logger.h"
#include "hybm_entity_factory.h"
#include "hybm_big_mem.h"
#include "hybm_gvm_user.h"

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
    auto entity = (MemEntity *)e;
    BM_ASSERT_RET_VOID(entity != nullptr);
    entity->UnInitialize();
    MemEntityFactory::Instance().RemoveEngine(e);
}

HYBM_API int32_t hybm_reserve_mem_space(hybm_entity_t e, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->ReserveMemorySpace();
}

HYBM_API int32_t hybm_unreserve_mem_space(hybm_entity_t e, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->UnReserveMemorySpace();
}

HYBM_API void *hybm_get_memory_ptr(hybm_entity_t e, hybm_mem_type mType)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, nullptr);
    return entity->GetReservedMemoryPtr(mType);
}

HYBM_API hybm_mem_slice_t hybm_alloc_local_memory(hybm_entity_t e, hybm_mem_type mType, uint64_t size, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    hybm_mem_slice_t slice;
    BM_ASSERT_RETURN(entity != nullptr, nullptr);
    auto ret = entity->AllocLocalMemory(size, mType, flags, slice);
    if (ret != 0) {
        BM_LOG_ERROR("allocate slice with size: " << size << ", mType: " << mType << " failed: " << ret);
        return nullptr;
    }

    return slice;
}

HYBM_API int32_t hybm_free_local_memory(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t count, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(slice != nullptr, BM_INVALID_PARAM);
    entity->FreeLocalMemory(slice, flags);
    return 0;
}

HYBM_API int32_t hybm_export(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t flags, hybm_exchange_info *exInfo)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(slice != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(exInfo != nullptr, BM_INVALID_PARAM);

    ExchangeInfoWriter writer(exInfo);
    auto ret = entity->ExportExchangeInfo(slice, writer, flags);
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
    BM_ASSERT_RETURN(allExInfo != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(count > 0, BM_INVALID_PARAM);

    std::vector<ExchangeInfoReader> readers(count);
    for (auto i = 0U; i < count; i++) {
        readers[i].Reset(allExInfo + i);
    }

    return entity->ImportExchangeInfo(readers.data(), count, flags);
}

HYBM_API int32_t hybm_entity_export(hybm_entity_t e, uint32_t flags, hybm_exchange_info *exInfo)
{
    auto entity = (MemEntity *)e;
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
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(allExInfo != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(count > 0, BM_INVALID_PARAM);

    std::vector<ExchangeInfoReader> readers(count);
    for (auto i = 0U; i < count; i++) {
        readers[i].Reset(allExInfo + i);
    }

    return entity->ImportEntityExchangeInfo(readers.data(), count, flags);
}

HYBM_API int32_t hybm_mmap(hybm_entity_t e, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    return entity->Mmap();
}

HYBM_API int32_t hybm_entity_reach_types(hybm_entity_t e, uint32_t rank, hybm_data_op_type &reachTypes, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);

    reachTypes = entity->CanReachDataOperators(rank);
    return BM_OK;
}

HYBM_API int32_t hybm_remove_imported(hybm_entity_t e, uint32_t rank, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);

    std::vector<uint32_t> ranks = {rank};
    return entity->RemoveImported(ranks);
}

HYBM_API int32_t hybm_set_extra_context(hybm_entity_t e, const void *context, uint32_t size)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RETURN(entity != nullptr, BM_INVALID_PARAM);
    BM_ASSERT_RETURN(context != nullptr, BM_INVALID_PARAM);
    return entity->SetExtraContext(context, size);
}

HYBM_API void hybm_unmap(hybm_entity_t e, uint32_t flags)
{
    auto entity = (MemEntity *)e;
    BM_ASSERT_RET_VOID(entity != nullptr);
    entity->Unmap();
}

HYBM_API int32_t hybm_mem_register_into_svsp(uint64_t addr, uint64_t size)
{
    if (!HybmGvmHasInited()) {
        BM_LOG_ERROR("gmv is not inited!");
        return BM_ERROR;
    }

    size = (size + DEVICE_LARGE_PAGE_SIZE - 1) / DEVICE_LARGE_PAGE_SIZE * DEVICE_LARGE_PAGE_SIZE;
    return hybm_gvm_mem_register(addr, size);
}