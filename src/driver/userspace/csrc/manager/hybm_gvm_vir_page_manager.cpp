/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#include "hybm_gvm_vir_page_manager.h"

#include <cerrno>
#include <cstring>
#include <sys/mman.h>

#include "hybm_user_logger.h"

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
    if (startAddr_ != 0) {
        BM_USER_LOG_ERROR("Gvm manager is already inited" << startAddr << " size:" << size);
        return -1;
    }

    void *mapped = mmap(reinterpret_cast<void *>(startAddr), size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        BM_USER_LOG_ERROR("Failed to mmap startAddr:" << startAddr << " size:" << size << " error:" << strerror(errno));
        return -1;
    }

    startAddr_ = startAddr;
    totalSize_ = size;
    return 0;
}

int32_t HybmGvmVirPageManager::ReserveMemory(uint64_t *addr, uint64_t size)
{
    std::unique_lock<std::mutex> lockGuard{mutex_};
    if (size % HYBM_VIR_PAGE_SIZE != 0) {
        BM_USER_LOG_ERROR("Failed to reserve memory size:" << size << " must alignment " << HYBM_VIR_PAGE_SIZE);
        return -1;
    }
    if (size + reservedSize_ > totalSize_) {
        BM_USER_LOG_ERROR("Failed to reserve memory size:" << size << " reservedSize:" << reservedSize_
                                                           << " totalSize:" << totalSize_);
        return -1;
    }
    *addr = startAddr_ + reservedSize_;
    reservedSize_ += size;
    return 0;
}
