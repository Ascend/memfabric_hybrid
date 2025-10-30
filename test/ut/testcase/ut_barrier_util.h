/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef UT_BARRIER_UTIL_H
#define UT_BARRIER_UTIL_H

#include <string>
#include "smem_shm.h"

class UtBarrierUtil {
public:
    UtBarrierUtil() {}
    ~UtBarrierUtil()
    {
        if (barrierHandle != nullptr) {
            smem_shm_destroy(barrierHandle, 0);
            smem_shm_uninit(0);
        }
    }

    int32_t Init(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, const char* ipPort, uint64_t memSize)
    {
        smem_shm_config_t config2;
        (void)smem_shm_config_init(&config2);
        config2.startConfigStore = false;
        auto ret = smem_shm_init(ipPort, rkSize, rankId, deviceId, &config2);
        if (ret != 0) {
            return ret;
        }

        void *gva = nullptr;
        smem_shm_t handle = smem_shm_create(0, rkSize, rankId, memSize, SMEMS_DATA_OP_MTE, 0, &gva);
        if (handle == nullptr || gva == nullptr) {
            return -1;
        }
        barrierHandle = handle;
        return 0;
    }

    int32_t Barrier()
    {
        return smem_shm_control_barrier(barrierHandle);
    }
private:
    smem_shm_t barrierHandle = nullptr;
};

#endif // UT_BARRIER_UTIL_H
