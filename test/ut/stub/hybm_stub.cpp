/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <string>
#include <iostream>
#include "hybm_big_mem.h"
#include "hybm_data_op.h"

int32_t hybm_init(uint16_t deviceId, uint64_t flags)
{
    return 0;
}

void hybm_uninit()
{
    return;
}

int32_t hybm_set_log_level(int level)
{
    return 0;
}

int32_t hybm_set_extern_logger(void (*logger)(int level, const char *msg))
{
    return 0;
}

const char *hybm_get_error_string(int32_t errCode)
{
    static char stubStr[] = "STUB";
    return stubStr;
}

struct HybmMgrStub {
    uint32_t rankId;
    uint32_t ranksize;
};

hybm_entity_t hybm_create_entity(uint16_t id, const hybm_options *options, uint32_t flags)
{
    HybmMgrStub *mgr = new (std::nothrow) HybmMgrStub;
    if (mgr == nullptr) {
        return nullptr;
    }
    mgr->rankId = options->rankId;
    mgr->ranksize = options->rankCount;
    return reinterpret_cast<hybm_entity_t>(mgr);
}

void hybm_destroy_entity(hybm_entity_t e, uint32_t flags)
{
    HybmMgrStub *mgr = reinterpret_cast<HybmMgrStub *>(e);
    delete mgr;
    return;
}

int32_t hybm_reserve_mem_space(hybm_entity_t e, uint32_t flags, void **reservedMem)
{
    *reservedMem = reinterpret_cast<void *>(0x123456ULL);
    return 0;
}

int32_t hybm_unreserve_mem_space(hybm_entity_t e, uint32_t flags, void *reservedMem)
{
    return 0;
}

hybm_mem_slice_t hybm_alloc_local_memory(hybm_entity_t e, hybm_mem_type mType, uint64_t size, uint32_t flags)
{
    return (void *)0x66666ULL;
}

int32_t hybm_free_local_memory(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t count, uint32_t flags)
{
    return 0;
}

int32_t hybm_export(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t flags, hybm_exchange_info *exInfo)
{
    HybmMgrStub *mgr = reinterpret_cast<HybmMgrStub *>(e);
    if (mgr != nullptr && exInfo != nullptr) {
        std::string rk = std::to_string(mgr->ranksize) + "_" + std::to_string(mgr->rankId);
        exInfo->descLen = rk.size();
        std::copy(rk.c_str(), rk.c_str() + rk.size(), exInfo->desc);
        return 0;
    } else {
        return -1;
    }
}

int32_t hybm_import(hybm_entity_t e, const hybm_exchange_info allExInfo[], uint32_t count, uint32_t flags)
{
    HybmMgrStub *mgr = reinterpret_cast<HybmMgrStub *>(e);
    if (mgr == nullptr || allExInfo == nullptr) {
        return -1;
    }

    for (uint32_t i = 0; i < mgr->ranksize; i++) {
        std::string base = std::to_string(mgr->ranksize) + "_" + std::to_string(i);
        std::string input((const char *)allExInfo[i].desc, allExInfo[i].descLen);

        if (base.compare(0, input.size(), input) != 0) {
            std::cout << "import failed, export_desc: " << base << " input_desc: " << input << std::endl;
            return -1;
        }
    }

    return 0;
}

int32_t hybm_start(hybm_entity_t e, uint32_t flags)
{
    return 0;
}

void hybm_stop(hybm_entity_t e, uint32_t flags)
{
    return;
}

int32_t hybm_mmap(hybm_entity_t e, uint32_t flags)
{
    return 0;
}

int32_t hybm_join(hybm_entity_t e, uint32_t rank, uint32_t flags)
{
    return 0;
}

int32_t hybm_leave(hybm_entity_t e, uint32_t rank, uint32_t flags)
{
    return 0;
}

int32_t hybm_set_extra_context(hybm_entity_t e, const void *context, uint32_t size)
{
    return 0;
}

int32_t hybm_data_copy(hybm_entity_t e, const void *src, void *dest, size_t count, hybm_data_copy_direction direction,
                       uint32_t flags)
{
    return 0;
}

int32_t hybm_data_copy_2d(hybm_entity_t e, const void *src, uint64_t spitch,
                          void *dest, uint64_t dpitch, uint64_t width, uint64_t height,
                          hybm_data_copy_direction direction, uint32_t flags)
{
    return 0;
}