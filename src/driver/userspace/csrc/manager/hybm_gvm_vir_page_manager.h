/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_GVM_VIR_PAGE_MANAGER_H
#define MF_HYBRID_HYBM_GVM_VIR_PAGE_MANAGER_H

#include <cstdint>
#include <mutex>

namespace ock {
namespace mf {

struct HGM_MemInfo {
    uint64_t start;
    uint64_t reserved;
    uint64_t used;
};

class HybmGvmVirPageManager {
public:
    static HybmGvmVirPageManager *Instance();
    int32_t Initialize(uint64_t startAddr, uint64_t size, int fd);
    int32_t ReserveMemory(uint64_t *addr, uint64_t size, bool shared);

private:
    HybmGvmVirPageManager() = default;
    ~HybmGvmVirPageManager();

private:
    HGM_MemInfo local_{};
    HGM_MemInfo global_{};
    std::mutex mutex_;
};
}
}

#endif // MF_HYBRID_HYBM_GVM_VIR_PAGE_MANAGER_H
