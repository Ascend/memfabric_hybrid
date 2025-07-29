/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DEFINE_H
#define MEM_FABRIC_HYBRID_HYBM_DEFINE_H

#include <cstdint>

namespace ock {
namespace mf {

constexpr uint64_t DEVICE_LARGE_PAGE_SIZE = 2UL * 1024UL * 1024UL;  // 大页的size, 2M
constexpr uint64_t SVM_END_ADDR = 0x100000000000ULL + 0x80000000000ULL - (1UL << 30UL); // svm的结尾虚拟地址
constexpr uint64_t HYBM_DEVICE_PRE_META_SIZE = 128UL; // 128B
constexpr uint64_t HYBM_DEVICE_GLOBAL_META_SIZE = HYBM_DEVICE_PRE_META_SIZE; // 128B
constexpr uint64_t HYBM_ENTITY_NUM_MAX = 511UL; // entity最大数量
constexpr uint64_t HYBM_DEVICE_META_SIZE = HYBM_DEVICE_PRE_META_SIZE * HYBM_ENTITY_NUM_MAX
    + HYBM_DEVICE_GLOBAL_META_SIZE; // 64K

constexpr uint64_t HYBM_DEVICE_USER_CONTEXT_PRE_SIZE = 64UL * 1024UL; // 64K
constexpr uint64_t HYBM_DEVICE_INFO_SIZE = HYBM_DEVICE_USER_CONTEXT_PRE_SIZE * HYBM_ENTITY_NUM_MAX
    + HYBM_DEVICE_META_SIZE; // 元数据+用户context,总大小32M, 对齐DEVICE_LARGE_PAGE_SIZE
constexpr uint64_t HYBM_DEVICE_META_ADDR = SVM_END_ADDR - HYBM_DEVICE_INFO_SIZE;
constexpr uint64_t HYBM_DEVICE_USER_CONTEXT_ADDR = HYBM_DEVICE_META_ADDR + HYBM_DEVICE_META_SIZE;
constexpr uint32_t ACL_MEMCPY_HOST_TO_HOST = 0;
constexpr uint32_t ACL_MEMCPY_HOST_TO_DEVICE = 1;
constexpr uint32_t ACL_MEMCPY_DEVICE_TO_HOST = 2;
constexpr uint32_t ACL_MEMCPY_DEVICE_TO_DEVICE = 3;

struct HybmDeviceGlobalMeta {
    uint64_t entityCount;
    uint64_t reserved[15]; // total 128B, equal HYBM_DEVICE_PRE_META_SIZE
};

struct HybmDeviceMeta {
    uint32_t entityId;
    uint32_t rankId;
    uint32_t rankSize;
    uint32_t extraContextSize;
    uint64_t symmetricSize;
    uint64_t reserved[13]; // total 128B, equal HYBM_DEVICE_PRE_META_SIZE
};

// macro for gcc optimization for prediction of if/else
#ifndef LIKELY
#define LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

#define HYBM_API __attribute__((visibility("default")))

#define DL_LOAD_SYM(TARGET_FUNC_VAR, TARGET_FUNC_TYPE, FILE_HANDLE, SYMBOL_NAME)                      \
    do {                                                                                              \
        TARGET_FUNC_VAR = (TARGET_FUNC_TYPE)dlsym(FILE_HANDLE, SYMBOL_NAME);                          \
        if ((TARGET_FUNC_VAR) == nullptr) {                                                           \
            BM_LOG_ERROR("Failed to call dlsym to load " << (SYMBOL_NAME) << ", error" << dlerror()); \
            dlclose(FILE_HANDLE);                                                                     \
            return BM_DL_FUNCTION_FAILED;                                                             \
        }                                                                                             \
    } while (0)


enum HybmGvaVersion : uint32_t {
    HYBM_GVA_V1 = 0,
    HYBM_GVA_V2 = 1,
    HYBM_GVA_UNKNOWN
};

}  // namespace mf
}  // namespace ock

#endif  // MEM_FABRIC_HYBRID_HYBM_DEFINE_H
