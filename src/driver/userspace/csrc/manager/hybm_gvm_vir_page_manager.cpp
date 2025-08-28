/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/mman.h>

#include "hybm_gvm_user_def.h"
#include "hybm_user_logger.h"
#include "hybm_gvm_vir_page_manager.h"

using namespace ock::mf;

namespace {
constexpr uint64_t HYBM_VIR_PAGE_SIZE = 1024ULL * 1024ULL * 1024ULL;
}

HybmGvmVirPageManager *HybmGvmVirPageManager::Instance()
{
    static HybmGvmVirPageManager instance;
    return &instance;
}

HybmGvmVirPageManager::~HybmGvmVirPageManager()
{
}

int32_t HybmGvmVirPageManager::Initialize(uint64_t startAddr, uint64_t size, int fd)
{
    if (local_.start != 0 || global_.start != 0) {
        BM_USER_LOG_ERROR("Gvm manager is already inited" << startAddr << " size:" << size);
        return -1;
    }

    void *mapped = mmap(reinterpret_cast<void *>(startAddr), size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        BM_USER_LOG_ERROR("Failed to mmap startAddr:" << std::hex << startAddr << " size:" <<
            size << " error:" << strerror(errno));
        return -1;
    }

    if (startAddr < HYBM_GVM_SDMA_START_ADDR) {
        local_.start = startAddr;
        local_.reserved = std::min(startAddr + size, (uint64_t)HYBM_GVM_SDMA_START_ADDR) - startAddr;
    } else {
        local_.start = local_.reserved = 0;
    }

    if (startAddr + size > HYBM_GVM_SDMA_START_ADDR) {
        global_.start = std::max((uint64_t)HYBM_GVM_SDMA_START_ADDR, startAddr);
        global_.reserved = startAddr + size - global_.start;
    } else {
        global_.start = global_.reserved = 0;
    }

    local_.used = global_.used = 0;
    return 0;
}

int32_t HybmGvmVirPageManager::ReserveMemory(uint64_t *addr, uint64_t size, bool shared)
{
    std::unique_lock<std::mutex> lockGuard{mutex_};
    if (size % HYBM_VIR_PAGE_SIZE != 0) {
        BM_USER_LOG_ERROR("Failed to reserve memory size:" << size << " must alignment " << HYBM_VIR_PAGE_SIZE);
        return -1;
    }

    HGM_MemInfo *info = (shared ? &global_ : &local_);
    if (size + info->used > info->reserved) {
        BM_USER_LOG_ERROR("Failed to reserve memory size:" << size << " usedSize:" << info->used
                                                           << " totalSize:" << info->reserved);
        return -1;
    }
    *addr = info->start + info->used;
    info->used += size;
    return 0;
}
