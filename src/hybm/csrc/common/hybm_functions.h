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
    static uint64_t ValidateObjectMagic(const void *ptr, const uint64_t magic);

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
                BM_LOG_ERROR("Failed to get visible devSize: " << devList.size() << " deviceId: " << deviceId);
                return BM_ERROR;
            } else {
                if (!StrHelper::OckStol(devList[deviceId], logicDeviceId)) {
                    BM_LOG_ERROR("Failed to get visible dev size: " << devList.size() << " deviceId: " << deviceId);
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

inline uint64_t Func::ValidateObjectMagic(const void *ptr, const uint64_t magic)
{
    auto tmp = reinterpret_cast<uint64_t>(ptr);
    return magic == ((tmp & gMagicBits) + UN40);
}

inline std::string SafeStrError(int errNum)
{
    locale_t loc = newlocale(LC_ALL_MASK, "", static_cast<locale_t>(nullptr));
    if (loc == static_cast<locale_t>(nullptr)) {
        return "Failed to create locale";
    }
    const char *error_msg = strerror_l(errNum, loc);
    std::string result(error_msg ? error_msg : "Unknown error");
    freelocale(loc);
    return result;
}

template<typename II, typename OI>
Result SafeCopy(II first, II last, OI result)
{
    try {
        std::copy(first, last, result);
    } catch (...) {
        BM_LOG_ERROR("copy failed.");
        return BM_MALLOC_FAILED;
    }
    return BM_OK;
}
} // namespace mf
} // namespace ock

#endif // MEM_FABRIC_HYBRID_HYBM_FUNCTIONS_H