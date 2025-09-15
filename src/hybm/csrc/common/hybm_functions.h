/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_FUNCTIONS_H
#define MEM_FABRIC_HYBRID_HYBM_FUNCTIONS_H

#include "hybm_define.h"
#include "hybm_types.h"
#include "hybm_logger.h"
#include "hybm_str_helper.h"

namespace ock {
namespace mf {
class Func {
public:
    static bool Realpath(std::string &path);
    static Result LibraryRealPath(const std::string &libDirPath, const std::string &libName, std::string &realPath);

    static uint64_t MakeObjectMagic(uint64_t srcAddress);
    static uint64_t ValidateObjectMagic(void *ptr, uint64_t magic);

    static inline int32_t GetLogicDeviceId(const int &deviceId)
    {
        int logicDeviceId = -1;
        auto visibleDevStr = std::getenv("ASCEND_RT_VISIBLE_DEVICES");
        if (visibleDevStr == nullptr) {
            BM_LOG_INFO("Not set rt visible env return deviceId: " << deviceId);
            return deviceId;
        } else {
            auto devList = StrHelper::Split(visibleDevStr, ',');
            if (devList.size() <= static_cast<uint32_t>(deviceId)) {
                BM_LOG_ERROR("Failed to get visible dev: " << visibleDevStr << " deviceId: " << deviceId);
                return BM_ERROR;
            } else {
                try {
                    logicDeviceId = std::stoi(devList[deviceId]);
                } catch (...) {
                    BM_LOG_ERROR("Failed to get visible dev: " << visibleDevStr << " deviceId: " << deviceId);
                    return BM_ERROR;
                }
            }
        }
        return logicDeviceId;
    }

private:
    const static uint64_t gMagicBits = 0xFFFFFFFFFF; /* get lower 40bits */
};

inline uint64_t Func::MakeObjectMagic(uint64_t srcAddress)
{
    return (srcAddress & gMagicBits) + UN40;
}

inline uint64_t Func::ValidateObjectMagic(void *ptr, const uint64_t magic)
{
    auto tmp = reinterpret_cast<uint64_t>(ptr);
    return magic == ((tmp & gMagicBits) + UN40);
}
}
}

#endif // MEM_FABRIC_HYBRID_HYBM_FUNCTIONS_H
