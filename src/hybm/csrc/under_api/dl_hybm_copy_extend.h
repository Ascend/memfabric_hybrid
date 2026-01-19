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
#ifndef MEMFABRIC_HYBRID_DL_HYBM_COPY_EXTEND_H
#define MEMFABRIC_HYBRID_DL_HYBM_COPY_EXTEND_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {

using hybmCopyExtendFunc = void (*)(void *, void *, uint64_t, uint32_t, void *);
using hybmBatchCopyExtendFunc = void (*)(void *, uint32_t, void *, uint32_t, void *);

class DlHybmExtendApi {
public:
    static Result TryLoadLibrary();
    static void CleanupLibrary();

    static inline Result HybmCopyExtend(const void *src, void *dst, uint64_t len, uint32_t concurrent, void *stream)
    {
        if (pHybmCopyExtend == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        pHybmCopyExtend(const_cast<void *>(src), dst, len, concurrent, stream);
        return BM_OK;
    }

    static inline Result HybmBatchCopyExtend(void *param, uint32_t count, void *mask, uint32_t concurrent, void *stream)
    {
        if (pHybmBatchCopyExtend == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        pHybmBatchCopyExtend(param, count, mask, concurrent, stream);
        return BM_OK;
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *libHandle;
    static const char *gHybmExtendLibName;

    static hybmCopyExtendFunc pHybmCopyExtend;
    static hybmBatchCopyExtendFunc pHybmBatchCopyExtend;
};
}
}
#endif // MEMFABRIC_HYBRID_DL_HYBM_COPY_EXTEND_H
