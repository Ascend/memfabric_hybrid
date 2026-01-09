/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#ifndef MEM_FABRIC_HYBRID_HYBM_DEFINE_H
#define MEM_FABRIC_HYBRID_HYBM_DEFINE_H

#include <netinet/in.h>
#include <cstdint>
#include <cstddef>
#include "mf_out_logger.h"

namespace ock {
namespace mf {

constexpr uint64_t KB = 1024ULL;
constexpr uint64_t MB = KB * 1024ULL;
constexpr uint64_t GB = MB * 1024ULL;
constexpr uint64_t TB = GB * 1024ULL;

constexpr uint32_t RANK_MAX = 1024UL;
constexpr uint64_t SMALL_PAGE_SIZE = 4U * KB;

constexpr uint64_t HYBM_LARGE_PAGE_SIZE = 2UL * 1024UL * 1024UL; // 大页的size, 2M
constexpr uint64_t HYBM_DEVICE_VA_START = 0x100000000000UL;      // NPU上的地址空间起始: 16T
constexpr uint64_t HYBM_DEVICE_VA_SIZE = 0x80000000000UL;        // NPU上的地址空间范围: 8T
constexpr uint64_t SVM_END_ADDR = HYBM_DEVICE_VA_START + HYBM_DEVICE_VA_SIZE - (1UL << 30UL); // svm的结尾虚拟地址
constexpr uint64_t HYBM_DEVICE_PRE_META_SIZE = 128UL;                                         // 128B
constexpr uint64_t HYBM_DEVICE_GLOBAL_META_SIZE = HYBM_DEVICE_PRE_META_SIZE;                  // 128B
constexpr uint64_t HYBM_ENTITY_NUM_MAX = 511UL;                                               // entity最大数量
constexpr uint64_t HYBM_DEVICE_META_SIZE =
    HYBM_DEVICE_PRE_META_SIZE * HYBM_ENTITY_NUM_MAX + HYBM_DEVICE_GLOBAL_META_SIZE; // 64K

constexpr uint64_t HYBM_DEVICE_USER_CONTEXT_PRE_SIZE = 64UL * 1024UL; // 64K
constexpr uint64_t HYBM_DEVICE_INFO_SIZE =
    HYBM_DEVICE_USER_CONTEXT_PRE_SIZE * HYBM_ENTITY_NUM_MAX +
    HYBM_DEVICE_META_SIZE; // 元数据+用户context,总大小32M, 对齐HYBM_LARGE_PAGE_SIZE
constexpr uint64_t HYBM_DEVICE_META_ADDR = SVM_END_ADDR - HYBM_DEVICE_INFO_SIZE;
constexpr uint64_t HYBM_DEVICE_USER_CONTEXT_ADDR = HYBM_DEVICE_META_ADDR + HYBM_DEVICE_META_SIZE;
constexpr uint32_t ACL_MEMCPY_HOST_TO_HOST = 0;
constexpr uint32_t ACL_MEMCPY_HOST_TO_DEVICE = 1;
constexpr uint32_t ACL_MEMCPY_DEVICE_TO_HOST = 2;
constexpr uint32_t ACL_MEMCPY_DEVICE_TO_DEVICE = 3;

constexpr uint64_t HYBM_HBM_START_ADDR = HYBM_DEVICE_VA_START;
constexpr uint64_t HYBM_HBM_END_ADDR = HYBM_DEVICE_VA_START + HYBM_DEVICE_VA_SIZE;
constexpr uint64_t HYBM_HOST_CONN_START_ADDR = 0x30000000000UL; // 48T
constexpr uint64_t HYBM_HOST_CONN_ADDR_SIZE = 0x100000000000UL; // 16T
constexpr uint64_t HYBM_GVM_START_ADDR = 0x280000000000UL;      // 40T
constexpr uint64_t HYBM_GVM_END_ADDR = 0xA80000000000UL;        // 168T

constexpr uint64_t ENTITY_EXPORT_INFO_MAGIC = 0xAABB1234FFFFEE00UL;
constexpr uint64_t HBM_SLICE_EXPORT_INFO_MAGIC = 0xAABB1234FFFFEE01UL;
constexpr uint64_t DRAM_SLICE_EXPORT_INFO_MAGIC = 0xAABB1234FFFFEE02UL;
constexpr uint64_t VMM_BASE_HBM_SLICE_EXPORT_INFO_MAGIC = 0xAABB1234FFFFEE03UL;
constexpr uint64_t VMM_BASE_DRAM_SLICE_EXPORT_INFO_MAGIC = 0xAABB1234FFFFEE04UL;
constexpr uint64_t EXPORT_INFO_VERSION = 0x1UL;

inline bool IsDramSlice(uint64_t magic)
{
    return magic == DRAM_SLICE_EXPORT_INFO_MAGIC || magic == VMM_BASE_DRAM_SLICE_EXPORT_INFO_MAGIC;
}

inline uint64_t Valid48BitsAddress(uint64_t address)
{
    return address & 0xffffffffffffUL;
}

inline const void *Valid48BitsAddress(const void *address)
{
    uint64_t addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
    return reinterpret_cast<const void *>(static_cast<uintptr_t>(Valid48BitsAddress(addr)));
}

inline void *Valid48BitsAddress(void *address)
{
    uint64_t addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
    return reinterpret_cast<void *>(static_cast<uintptr_t>(Valid48BitsAddress(addr)));
}

enum AscendSocType {
    ASCEND_UNKNOWN = 0,
    ASCEND_910B,
    ASCEND_910C,
};

enum DeviceSystemInfoType {
    INFO_TYPE_VERSION = 1,
    INFO_TYPE_PHY_CHIP_ID = 18,
    INFO_TYPE_PHY_DIE_ID,
    INFO_TYPE_SDID = 26,
    INFO_TYPE_SERVER_ID,
    INFO_TYPE_SCALE_TYPE,
    INFO_TYPE_SUPER_POD_ID,
    INFO_TYPE_ADDR_MODE,
};

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
    uint64_t qpInfoAddress;
    uint64_t reserved[12]; // total 128B, equal HYBM_DEVICE_PRE_META_SIZE
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
            FILE_HANDLE = nullptr;                                                                    \
            return BM_DL_FUNCTION_FAILED;                                                             \
        }                                                                                             \
    } while (0)

#define DL_LOAD_SYM_OPTIONAL(TARGET_FUNC_VAR, TARGET_FUNC_TYPE, FILE_HANDLE, SYMBOL_NAME)            \
    do {                                                                                             \
        TARGET_FUNC_VAR = (TARGET_FUNC_TYPE)dlsym(FILE_HANDLE, SYMBOL_NAME);                         \
        if ((TARGET_FUNC_VAR) == nullptr) {                                                          \
            BM_LOG_WARN("Failed to call dlsym to load " << (SYMBOL_NAME) << ", error" << dlerror()); \
        }                                                                                            \
    } while (0)

#define DL_LOAD_SYM_ALT(TARGET_FUNC_VAR, TARGET_FUNC_TYPE, FILE_HANDLE, SYMBOL_NAME, SYMBOL_NAME_ALT) \
    do {                                                                                              \
        TARGET_FUNC_VAR = (TARGET_FUNC_TYPE)dlsym(FILE_HANDLE, SYMBOL_NAME);                          \
        if ((TARGET_FUNC_VAR) != nullptr) {                                                           \
            BM_LOG_DEBUG("Loaded symbol " << (SYMBOL_NAME) << " successfully");                       \
            break;                                                                                    \
        }                                                                                             \
        TARGET_FUNC_VAR = (TARGET_FUNC_TYPE)dlsym(FILE_HANDLE, SYMBOL_NAME_ALT);                      \
        if ((TARGET_FUNC_VAR) != nullptr) {                                                           \
            BM_LOG_DEBUG("Loaded symbol " << (SYMBOL_NAME_ALT) << " successfully");                   \
            break;                                                                                    \
        }                                                                                             \
        BM_LOG_ERROR("Failed to call dlsym to load " << (SYMBOL_NAME) << " or " << (SYMBOL_NAME_ALT)  \
                                                     << ", error" << dlerror());                      \
        dlclose(FILE_HANDLE);                                                                         \
        FILE_HANDLE = nullptr;                                                                        \
        return BM_DL_FUNCTION_FAILED;                                                                 \
    } while (0)

enum HybmGvaVersion : uint32_t { HYBM_GVA_V1 = 0, HYBM_GVA_V2 = 1, HYBM_GVA_V3 = 2, HYBM_GVA_V4 = 3, HYBM_GVA_UNKNOWN };

} // namespace mf
} // namespace ock

#endif // MEM_FABRIC_HYBRID_HYBM_DEFINE_H