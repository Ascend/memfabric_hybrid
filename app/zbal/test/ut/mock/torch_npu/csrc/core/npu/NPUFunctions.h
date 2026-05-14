#ifndef TORCH_NPU_CSRC_CORE_NPU_NPU_FUNCTIONS_H_
#define TORCH_NPU_CSRC_CORE_NPU_NPU_FUNCTIONS_H_

#include <cstdint>

constexpr int32_t ACL_SUCCESS = 0;

namespace c10_npu {

inline int device_count()
{
    return 8; // mock: 8 devices
}

inline int32_t SetDevice(int /**/)
{
    return ACL_SUCCESS;
}

inline int32_t GetDevice(int *device)
{
    if (device != nullptr) {
        *device = 0;
    }
    return ACL_SUCCESS;
}

inline void npuSynchronizeDevice(bool /**/)
{
}

} // namespace c10_npu

#endif // TORCH_NPU_CSRC_CORE_NPU_NPU_FUNCTIONS_H_
