/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_stub.h"

#include <string>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mockcpp/mockcpp.hpp>

#include "hybm.h"
#include "hybm_big_mem.h"
#include "hybm_data_op.h"

const char UT_SHM_NAME2[] = "/mfhy_ut_shm_128M";
const uint64_t UT_SHM_SIZE2 = 128 * 1024 * 1024ULL;

int32_t hybm_init_stub(uint16_t deviceId, uint64_t flags)
{
    return 0;
}

void hybm_uninit_stub()
{
    return;
}

int32_t hybm_set_log_level_stub(int level)
{
    if (level < 0 || level > 4) {
        return -1;
    }
    return 0;
}

int32_t hybm_set_extern_logger_stub(void (*logger)(int level, const char *msg))
{
    return 0;
}

const char *hybm_get_error_string_stub(int32_t errCode)
{
    static char stubStr[] = "STUB";
    return stubStr;
}

struct HybmMgrStub {
    uint32_t rankId;
    uint32_t ranksize;
    int shmFd = -1;
};

hybm_entity_t hybm_create_entity_stub(uint16_t id, const hybm_options *options, uint32_t flags)
{
    HybmMgrStub *mgr = new (std::nothrow) HybmMgrStub;
    if (mgr == nullptr) {
        return nullptr;
    }
    mgr->rankId = options->rankId;
    mgr->ranksize = options->rankCount;
    return reinterpret_cast<hybm_entity_t>(mgr);
}

void hybm_destroy_entity_stub(hybm_entity_t e, uint32_t flags)
{
    HybmMgrStub *mgr = reinterpret_cast<HybmMgrStub *>(e);
    if (mgr->shmFd >= 0) {
        close(mgr->shmFd);
    }
    delete mgr;
    return;
}

int32_t hybm_reserve_mem_space_stub(hybm_entity_t e, uint32_t flags, void **reservedMem)
{
    int fd = shm_open(UT_SHM_NAME2, O_RDWR, 0666);
    if (fd < 0) {
        return -1;
    }
    *reservedMem = mmap(NULL, UT_SHM_SIZE2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (*reservedMem == MAP_FAILED) {
        close(fd);
        return -1;
    }
    HybmMgrStub *mgr = reinterpret_cast<HybmMgrStub *>(e);
    mgr->shmFd = fd;
    return 0;
}

int32_t hybm_unreserve_mem_space_stub(hybm_entity_t e, uint32_t flags, void *reservedMem)
{
    return 0;
}

hybm_mem_slice_t hybm_alloc_local_memory_stub(hybm_entity_t e, hybm_mem_type mType, uint64_t size, uint32_t flags)
{
    return (void *)0x66666ULL;
}

int32_t hybm_free_local_memory_stub(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t count, uint32_t flags)
{
    return 0;
}

int32_t hybm_export_stub(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t flags, hybm_exchange_info *exInfo)
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

int32_t hybm_import_stub(hybm_entity_t e, const hybm_exchange_info allExInfo[], uint32_t count, uint32_t flags)
{
    HybmMgrStub *mgr = reinterpret_cast<HybmMgrStub *>(e);
    if (mgr == nullptr || allExInfo == nullptr) {
        return -1;
    }

    for (uint32_t i = 0; i < count; i++) {
        std::string base = std::to_string(mgr->ranksize) + "_" + std::to_string(i);
        printf(" %u descLen:%u\n", i, allExInfo[i].descLen);
        std::string input;
        input.assign(reinterpret_cast<const char*>(allExInfo[i].desc), allExInfo[i].descLen);

        if (base.compare(0, input.size(), input) != 0) {
            std::cout << "import failed, export_desc: " << base << " input_desc: " << input << std::endl;
            return -1;
        }
    }

    return 0;
}

void hybm_unmap_stub(hybm_entity_t e, uint32_t flags)
{
    return;
}

int32_t hybm_mmap_stub(hybm_entity_t e, uint32_t flags)
{
    return 0;
}

int32_t hybm_remove_imported_stub(hybm_entity_t e, uint32_t rank, uint32_t flags)
{
    return 0;
}

int32_t hybm_set_extra_context_stub(hybm_entity_t e, const void *context, uint32_t size)
{
    return 0;
}

int32_t hybm_data_copy_stub(hybm_entity_t e, const void *src, void *dest, size_t count, hybm_data_copy_direction direction,
                       void *stream, uint32_t flags)
{
    memcpy(dest, src, count);
    return 0;
}

int32_t hybm_data_copy_2d_stub(hybm_entity_t e, const void *src, uint64_t spitch,
                          void *dest, uint64_t dpitch, uint64_t width, uint64_t height,
                          hybm_data_copy_direction direction, void *stream, uint32_t flags)
{
    auto srcAddr = (uint64_t)src;
    auto destAddr = (uint64_t)dest;
    for (uint64_t i = 0; i < height; i++) {
        memcpy((void *)(destAddr + i * dpitch), (const void *)(srcAddr + i * spitch), width);
    }
    return 0;
}

void mock_hybm_api()
{
    MOCKER(hybm_init).stubs().will(invoke(hybm_init_stub));
    MOCKER(hybm_uninit).stubs().will(invoke(hybm_uninit_stub));
    MOCKER(hybm_set_extern_logger).defaults().will(invoke(hybm_set_extern_logger_stub));
    MOCKER(hybm_set_log_level).stubs().will(invoke(hybm_set_log_level_stub));
    MOCKER(hybm_create_entity).stubs().will(invoke(hybm_create_entity_stub));
    MOCKER(hybm_destroy_entity).stubs().will(invoke(hybm_destroy_entity_stub));
    MOCKER(hybm_reserve_mem_space).stubs().will(invoke(hybm_reserve_mem_space_stub));
    MOCKER(hybm_unreserve_mem_space).stubs().will(invoke(hybm_unreserve_mem_space_stub));
    MOCKER(hybm_alloc_local_memory).stubs().will(invoke(hybm_alloc_local_memory_stub));
    MOCKER(hybm_free_local_memory).stubs().will(invoke(hybm_free_local_memory_stub));
    MOCKER(hybm_export).stubs().will(invoke(hybm_export_stub));
    MOCKER(hybm_import).stubs().will(invoke(hybm_import_stub));
    MOCKER(hybm_mmap).stubs().will(invoke(hybm_mmap_stub));
    MOCKER(hybm_remove_imported).stubs().will(invoke(hybm_remove_imported_stub));
    MOCKER(hybm_set_extra_context).stubs().will(invoke(hybm_set_extra_context_stub));
    MOCKER(hybm_data_copy).stubs().will(invoke(hybm_data_copy_stub));
    MOCKER(hybm_data_copy_2d).stubs().will(invoke(hybm_data_copy_2d_stub));
}