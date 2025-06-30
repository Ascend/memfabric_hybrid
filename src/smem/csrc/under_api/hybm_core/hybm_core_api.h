/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __SMEM_HYBM_CORE_API_H__
#define __SMEM_HYBM_CORE_API_H__

#include "hybm_def.h"
#include "smem_common_includes.h"

namespace ock {
namespace smem {

using hybmInitFunc = int32_t (*)(uint16_t, uint64_t);
using hybmUninitFunc = void (*)(void);
using hybmSetLoggerFunc = int32_t (*)(void (*)(int, const char *));
using hybmSetLogLevelFunc = int32_t (*)(int);
using hybmGetErrorFunc = const char *(*)(int32_t);

using hybmCreateEntityFunc = hybm_entity_t (*)(uint16_t, const hybm_options *, uint32_t);
using hybmDestroyEntityFunc = void (*)(hybm_entity_t, uint32_t);
using hybmReserveMemFunc = int32_t (*)(hybm_entity_t, uint32_t, void **);
using hybmUnreserveMemFunc = int32_t (*)(hybm_entity_t, uint32_t, void *);
using hybmAllocLocalMemFunc = hybm_mem_slice_t (*)(hybm_entity_t, hybm_mem_type, uint64_t, uint32_t);
using hybmFreeLocalMemFunc = int32_t (*)(hybm_entity_t, hybm_mem_slice_t, uint32_t, uint32_t);
using hybmExportFunc = int32_t (*)(hybm_entity_t, hybm_mem_slice_t, uint32_t, hybm_exchange_info *);
using hybmImportFunc = int32_t (*)(hybm_entity_t, const hybm_exchange_info *, uint32_t, uint32_t);
using hybmSetExtraContextFunc = int32_t (*)(hybm_entity_t, const void *, uint32_t);
using hybmStartFunc = int32_t (*)(hybm_entity_t, uint32_t);
using hybmStopFunc = void (*)(hybm_entity_t, uint32_t);
using hybmMmapFunc = int32_t (*)(hybm_entity_t, uint32_t);
using hybmJoinFunc = int32_t (*)(hybm_entity_t, uint32_t, uint32_t);
using hybmLeaveFunc = int32_t (*)(hybm_entity_t, uint32_t, uint32_t);
using hybmDataCopyFunc = int32_t (*)(hybm_entity_t, const void *, void *, size_t, hybm_data_copy_direction, uint32_t);
using hybmDataCopy2dFunc = int32_t (*)(hybm_entity_t, const void *, uint64_t, void *, uint64_t, uint64_t, uint64_t,
                                     hybm_data_copy_direction, uint32_t);

/*
 * int32_t hybm_data_copy(hybm_entity_t e, const void *src, void *dest, size_t count, hybm_data_copy_direction direction,
                       uint32_t flags);
 */

class HybmCoreApi {
public:
    static Result LoadLibrary(const std::string &libDirPath);

    static inline int32_t HybmCoreInit(uint16_t deviceId, uint64_t flags)
    {
        return pHybmInit(deviceId, flags);
    }

    static inline void HybmCoreUninit()
    {
        return pHybmUninit();
    }

    static inline int32_t HybmCoreSetExternLogger(void (*logger)(int level, const char *msg))
    {
        return pHybmSetLogger(logger);
    }

    static inline int32_t HybmCoreSetLogLevel(int level)
    {
        return pHybmSetLogLevel(level);
    }

    static inline const char *HybmGetErrorString(int32_t errCode)
    {
        return pHybmGetError(errCode);
    }

    static inline hybm_entity_t HybmCreateEntity(uint16_t id, const hybm_options *options, uint32_t flags)
    {
        return pHybmCreateEntity(id, options, flags);
    }

    static inline void HybmDestroyEntity(hybm_entity_t e, uint32_t flags)
    {
        return pHybmDestroyEntity(e, flags);
    }

    static inline int32_t HybmReserveMemSpace(hybm_entity_t e, uint32_t flags, void **reservedMem)
    {
        return pHybmReserveMem(e, flags, reservedMem);
    }

    static inline int32_t HybmUnreserveMemSpace(hybm_entity_t e, uint32_t flags, void *reservedMem)
    {
        return pHybmUnreserveMem(e, flags, reservedMem);
    }

    static inline hybm_mem_slice_t HybmAllocLocalMemory(hybm_entity_t e, hybm_mem_type mType, uint64_t size,
                                                        uint32_t flags)
    {
        return pHybmAllocLocalMem(e, mType, size, flags);
    }

    static inline int32_t HybmFreeLocalMemory(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t count, uint32_t flags)
    {
        return pHybmFreeLocalMem(e, slice, count, flags);
    }

    static inline int32_t HybmExport(hybm_entity_t e, hybm_mem_slice_t slice, uint32_t flags,
                                     hybm_exchange_info *exInfo)
    {
        return pHybmExport(e, slice, flags, exInfo);
    }

    static inline int32_t HybmImport(hybm_entity_t e, const hybm_exchange_info allExInfo[], uint32_t count,
                                     uint32_t flags)
    {
        return pHybmImport(e, allExInfo, count, flags);
    }

    static inline int32_t HybmSetExtraContext(hybm_entity_t e, const void *context, uint32_t size)
    {
        return pHybmSetExtraContext(e, context, size);
    }

    static inline int32_t HybmStart(hybm_entity_t e, uint32_t flags)
    {
        return pHybmStart(e, flags);
    }

    static inline void HybmStop(hybm_entity_t e, uint32_t flags)
    {
        return pHybmStop(e, flags);
    }

    static inline int32_t HybmMmap(hybm_entity_t e, uint32_t flags)
    {
        return pHybmMmap(e, flags);
    }

    static inline int32_t HybmJoin(hybm_entity_t e, uint32_t rank, uint32_t flags)
    {
        return pHybmJoin(e, rank, flags);
    }

    static inline int32_t HybmLeave(hybm_entity_t e, uint32_t rank, uint32_t flags)
    {
        return pHybmLeave(e, rank, flags);
    }

    static inline int32_t HybmDataCopy(hybm_entity_t e, const void *src, void *dest, size_t count,
                                       hybm_data_copy_direction direction, uint32_t flags)
    {
        return pHybmDataCopy(e, src, dest, count, direction, flags);
    }

    static inline int32_t HybmDataCopy2d(hybm_entity_t e, const void *src, uint64_t spitch,
                                         void *dest, uint64_t dpitch, uint64_t width, uint64_t height,
                                         hybm_data_copy_direction direction, uint32_t flags)
    {
        return pHybmDataCopy2d(e, src, spitch, dest, dpitch, width, height, direction, flags);
    }

private:
    static int32_t GetLibPath(const std::string &libDir, std::string &outputPath);

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *coreHandle;
    static const char *gHybmCoreLibName;

    static hybmInitFunc pHybmInit;
    static hybmUninitFunc pHybmUninit;
    static hybmSetLoggerFunc pHybmSetLogger;
    static hybmSetLogLevelFunc pHybmSetLogLevel;
    static hybmGetErrorFunc pHybmGetError;

    static hybmCreateEntityFunc pHybmCreateEntity;
    static hybmDestroyEntityFunc pHybmDestroyEntity;
    static hybmReserveMemFunc pHybmReserveMem;
    static hybmUnreserveMemFunc pHybmUnreserveMem;
    static hybmAllocLocalMemFunc pHybmAllocLocalMem;
    static hybmFreeLocalMemFunc pHybmFreeLocalMem;
    static hybmExportFunc pHybmExport;
    static hybmImportFunc pHybmImport;
    static hybmSetExtraContextFunc pHybmSetExtraContext;
    static hybmStartFunc pHybmStart;
    static hybmStopFunc pHybmStop;
    static hybmMmapFunc pHybmMmap;
    static hybmJoinFunc pHybmJoin;
    static hybmLeaveFunc pHybmLeave;
    static hybmDataCopyFunc pHybmDataCopy;
    static hybmDataCopy2dFunc pHybmDataCopy2d;
};
}
}

#endif  // __SMEM_HYBM_CORE_API_H__
