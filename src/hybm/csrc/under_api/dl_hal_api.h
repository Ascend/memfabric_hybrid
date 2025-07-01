/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBM_CORE_DL_HAL_API_H
#define MF_HYBM_CORE_DL_HAL_API_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {
using halGvaReserveMemoryFun = int32_t (*)(void **, size_t, int32_t, uint64_t);
using halGvaUnreserveMemoryFun = int32_t (*)(void);
using halGvaAllocFun = int32_t (*)(void *, size_t, uint64_t);
using halGvaFreeFun = int32_t (*)(void *, size_t);
using halGvaOpenFun = int32_t (*)(void *, const char *, size_t, uint64_t);
using halGvaCloseFun = int32_t (*)(void *, uint64_t);

class DlHalApi {
public:
    static Result LoadLibrary();

    static inline Result HalGvaReserveMemory(void **address, size_t size, int32_t deviceId, uint64_t flags)
    {
        return pHalGvaReserveMemory(address, size, deviceId, flags);
    }

    static inline Result HalGvaUnreserveMemory()
    {
        return pHalGvaUnreserveMemory();
    }

    static inline Result HalGvaAlloc(void *address, size_t size, uint64_t flags)
    {
        return pHalGvaAlloc(address, size, flags);
    }

    static inline Result HalGvaFree(void *address, size_t size)
    {
        return pHalGvaFree(address, size);
    }

    static inline Result HalGvaOpen(void *address, const char *name, size_t size, uint64_t flags)
    {
        return pHalGvaOpen(address, name, size, flags);
    }

    static inline Result HalGvaClose(void *address, uint64_t flags)
    {
        return pHalGvaClose(address, flags);
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *halHandle;
    static const char *gAscendHalLibName;

    static halGvaReserveMemoryFun pHalGvaReserveMemory;
    static halGvaUnreserveMemoryFun pHalGvaUnreserveMemory;
    static halGvaAllocFun pHalGvaAlloc;
    static halGvaFreeFun pHalGvaFree;
    static halGvaOpenFun pHalGvaOpen;
    static halGvaCloseFun pHalGvaClose;
};

}
}

#endif  // MF_HYBM_CORE_DL_HAL_API_H
