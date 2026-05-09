/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* MemFabric_Hybrid is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#ifndef MF_HYBM_CORE_DL_RT_API_H
#define MF_HYBM_CORE_DL_RT_API_H

#include <mutex>
#include "hybm_common_include.h"

namespace ock {
namespace mf {
using rtStreamGetSqidFunc = int32_t (*)(const void *, uint32_t *);
using rtStreamGetCqidFunc = int32_t (*)(const void *, uint32_t *, uint32_t *);

class DlRtApi {
public:
    static Result LoadLibrary();
    static void CleanupLibrary();

    static inline Result RtStreamGetSqid(const void *stm, uint32_t *sqId)
    {
        if (pRtStreamGetSqid == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtStreamGetSqid(stm, sqId);
    }

    static inline Result RtStreamGetCqid(const void *stm, uint32_t *cqId, uint32_t *logicCqId)
    {
        if (pRtStreamGetCqid == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return pRtStreamGetCqid(stm, cqId, logicCqId);
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *rtHandle;
    static const char *gRtLibName;

    static rtStreamGetSqidFunc pRtStreamGetSqid;
    static rtStreamGetCqidFunc pRtStreamGetCqid;
};
} // namespace mf
} // namespace ock

#endif // MF_HYBM_CORE_DL_RT_API_H