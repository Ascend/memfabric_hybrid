/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef SHMEM_MF_HYBRID_API_H
#define SHMEM_MF_HYBRID_API_H

#include "shm_functions.h"
#include "smem_shm_def.h"

namespace shm {
/* smem functions */
using SmemInitFunc = int32_t (*)(uint32_t);
using SmemSetExternLoggerFunc = int32_t (*)(void (*func)(int level, const char *));
using SmemSetLogLevelFunc = int32_t (*)(int);
using SmemUnInitFunc = void (*)();
using SmemGetLastErrMsgFunc = const char *(*)();
using SmemGetAndClearLastErrMsgFunc = const char *(*)();

/* smem shm functions */
using SmemShmConfigInitFunc = int32_t (*)(smem_shm_config_t *config);
using SmemShmInitFunc = int32_t (*)(const char *, uint32_t, uint32_t, uint16_t, uint64_t, smem_shm_config_t *);
using SmemShmUnInitFunc = void (*)(uint32_t flags);
using SmemShmQuerySupportDataOpFunc = uint32_t (*)(void);
using SmemShmCreateFunc = smem_shm_t (*)(uint32_t, uint32_t, uint32_t, uint64_t, smem_shm_data_op_type, uint32_t,
                                         void **);
using SmemShmDestroyFunc = int32_t (*)(smem_shm_t, uint32_t);
using SmemShmSetExtraContextFunc = int32_t (*)(smem_shm_t, const void *, uint32_t);
using SmemShmTeamGetRankFunc = uint32_t (*)(smem_shm_t);
using SmemShmTeamGetSizeFunc = uint32_t (*)(smem_shm_t);
using SmemShmControlBarrierFunc = int32_t (*)(smem_shm_t);
using SmemShmControlAllGatherFunc = int32_t (*)(smem_shm_t, const char *, uint32_t, char *, uint32_t);
using SmemShmTopoCanReachFunc = int32_t (*)(smem_shm_t, uint32_t, uint32_t *);

class SmemApi {
public:
    static int32_t LoadLibrary(const std::string &libDirPath);

public:
    /* smem api */
    static inline int32_t SmemInit(uint32_t flags)
    {
        return gSmemInit(flags);
    }

    static inline int32_t SmemSetExternLogger(void (*func)(int level, const char *msg))
    {
        return gSmemSetExternLogger(func);
    }

    static inline int32_t SmemSetLogLevel(int level)
    {
        return gSmemSetLogLevel(level);
    }

    static inline void SmemUnInit()
    {
        return gSmemUnInit();
    }

    static inline const char *SmemGetLastErrMsg()
    {
        return gSmemGetLastErrMsg();
    }

    static inline const char *SmemGetAndClearLastErrMsg()
    {
        return gSmemGetAndClearLastErrMsg();
    }

    /* smem shm api */
    static inline int32_t SmemShmConfigInit(smem_shm_config_t *config)
    {
        return gSmemShmConfigInit(config);
    }

    static inline int32_t SmemShmInit(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId,
                                      uint16_t deviceId, uint64_t gvaSpaceSize, smem_shm_config_t *config)
    {
        return gSmemShmInit(configStoreIpPort, worldSize, rankId, deviceId, gvaSpaceSize, config);
    }

    static inline void SmemShmUnInit(uint32_t flags)
    {
        return gSmemShmUnInit(flags);
    }

    static inline uint32_t SmemShmQuerySupportDataOp()
    {
        return gSmemShmQuerySupportDataOp();
    }

    static inline smem_shm_t SmemShmCreate(uint32_t id, uint32_t rankSize, uint32_t rankId, uint64_t symmetricSize,
                                           smem_shm_data_op_type dataOpType, uint32_t flags, void **gva)
    {
        return gSmemShmCreate(id, rankSize, rankId, symmetricSize, dataOpType, flags, gva);
    }

    static inline int32_t SmemShmDestroy(smem_shm_t handle, uint32_t flags)
    {
        return gSmemShmDestroy(handle, flags);
    }

    static inline int32_t SmemShmSetExtraContext(smem_shm_t handle, const void *context, uint32_t size)
    {
        return gSmemShmSetExtraContext(handle, context, size);
    }

    static inline uint32_t SmemShmTeamGetRank(smem_shm_t handle)
    {
        return gSmemShmTeamGetRank(handle);
    }

    static inline uint32_t SmemShmTeamGetSize(smem_shm_t handle)
    {
        return gSmemShmTeamGetSize(handle);
    }

    static inline int32_t SmemShmControlBarrier(smem_shm_t handle)
    {
        return gSmemShmControlBarrier(handle);
    }

    static inline int32_t SmemShmControlAllGather(smem_shm_t handle, const char *sendBuf, uint32_t sendSize,
                                                  char *recvBuf, uint32_t recvSize)
    {
        return gSmemShmControlAllGather(handle, sendBuf, sendSize, recvBuf, recvSize);
    }

    static inline int32_t SmemShmTopoCanReach(smem_shm_t handle, uint32_t remoteRank, uint32_t *reachInfo)
    {
        return gSmemShmTopoCanReach(handle, remoteRank, reachInfo);
    }

private:
    static bool gLoaded;
    static std::mutex gMutex;

    static void *gSmemHandle;
    static const char *gSmemFileName;

    static SmemInitFunc gSmemInit;
    static SmemSetExternLoggerFunc gSmemSetExternLogger;
    static SmemSetLogLevelFunc gSmemSetLogLevel;
    static SmemUnInitFunc gSmemUnInit;
    static SmemGetLastErrMsgFunc gSmemGetLastErrMsg;
    static SmemGetAndClearLastErrMsgFunc gSmemGetAndClearLastErrMsg;

    static SmemShmConfigInitFunc gSmemShmConfigInit;
    static SmemShmInitFunc gSmemShmInit;
    static SmemShmUnInitFunc gSmemShmUnInit;
    static SmemShmQuerySupportDataOpFunc gSmemShmQuerySupportDataOp;
    static SmemShmCreateFunc gSmemShmCreate;
    static SmemShmDestroyFunc gSmemShmDestroy;
    static SmemShmSetExtraContextFunc gSmemShmSetExtraContext;
    static SmemShmTeamGetRankFunc gSmemShmTeamGetRank;
    static SmemShmTeamGetSizeFunc gSmemShmTeamGetSize;
    static SmemShmControlBarrierFunc gSmemShmControlBarrier;
    static SmemShmControlAllGatherFunc gSmemShmControlAllGather;
    static SmemShmTopoCanReachFunc gSmemShmTopoCanReach;
};
}  // namespace shm

#endif  //SHMEM_MF_HYBRID_API_H
