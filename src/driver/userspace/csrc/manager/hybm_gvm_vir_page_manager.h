/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_GVM_VIR_PAGE_MANAGER_H
#define MF_HYBRID_HYBM_GVM_VIR_PAGE_MANAGER_H

#include <cstdint>
#include <mutex>

namespace ock {
namespace mf {

class HybmGvmVirPageManager {
public:
    static HybmGvmVirPageManager *Instance();
    int32_t Initialize(uint64_t startAddr, uint64_t size, int fd);
    int32_t ReserveMemory(uint64_t *addr, uint64_t size);

private:
    HybmGvmVirPageManager() = default;
    ~HybmGvmVirPageManager();

private:
    uint64_t startAddr_{0};
    uint64_t totalSize_{0};
    uint64_t reservedSize_{0};
    std::mutex mutex_;
};
}
}

#endif // MF_HYBRID_HYBM_GVM_VIR_PAGE_MANAGER_H
