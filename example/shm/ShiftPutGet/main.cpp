/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 * This file constains code of cpu debug and npu code.We read data from bin file
 * and write result to file.
 */
#include <iostream>
#include <sstream>
#include <limits> // 用于std::numeric_limits
#include <cstring>
#include "data_utils.h"
#include "acl/acl.h"
#include "smem.h"
#include "smem_shm.h"
#include "shm_all_shift.h"

static uint32_t gNpuNum = 16;
static uint64_t gNpuMallocSpace = 1024UL * 1024UL * 64;
static uint32_t gInputLen = 4;
static uint32_t ctxSize = 16;

static int32_t TestAllShift(aclrtStream stream, uint8_t *gva, uint32_t rankId, uint32_t rankSize)
{
    int64_t *xHost;
    int64_t *xDevice;
    uint32_t *yHost;
    size_t inputSize = (gInputLen + 2) * sizeof(int64_t);
    size_t outputSize = (gInputLen + 2) * sizeof(int64_t);
    CHECK_ACL(aclrtMallocHost((void **)(&xHost), inputSize)); // size = 32
    CHECK_ACL(aclrtMallocHost((void **)(&yHost), outputSize)); // size = 48
    CHECK_ACL(aclrtMalloc((void **)&xDevice, inputSize, ACL_MEM_MALLOC_HUGE_FIRST));
    xHost[0U] = rankId;
    xHost[1U] = rankSize;
    xHost[2U] = gInputLen;
    xHost[3U] = gNpuMallocSpace;

    uint64_t metaAddr = 0x180000000000ULL - (1UL << 30UL) - 32ULL * 1024 * 1024 + 128UL;
    CHECK_ACL(aclrtMemcpy(yHost, inputSize, (void *)metaAddr, inputSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_EQUALS(yHost[0U], 0);
    CHECK_EQUALS(yHost[1U], rankId);
    CHECK_EQUALS(yHost[2U], rankSize);
    CHECK_EQUALS(yHost[3U], ctxSize);

    CHECK_ACL(aclrtMemcpy(xDevice, inputSize, xHost, inputSize, ACL_MEMCPY_HOST_TO_DEVICE));
    shm_all_shift_do(stream, gva, xDevice);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    sleep(1);

    CHECK_ACL(aclrtMemcpy(xHost, outputSize, gva + rankId * gNpuMallocSpace, outputSize, ACL_MEMCPY_DEVICE_TO_HOST));
    CHECK_EQUALS(xHost[0], (rankId + rankSize - 1) % rankSize);
    CHECK_EQUALS(xHost[4], rankId);

    CHECK_ACL(aclrtFree(xDevice));
    CHECK_ACL(aclrtFreeHost(xHost));
    CHECK_ACL(aclrtFreeHost(yHost));
    return 0;
}

static void TestContext(smem_shm_t handle)
{
    char srcCtx[] = "1234567890123456";
    CHECK_ACL(smem_shm_set_extra_context(handle, (void *)srcCtx, ctxSize));

    char dstCtx[32];
    uint64_t ctxAddr = 0x180000000000ULL - (1UL << 30UL) - 32ULL * 1024 * 1024 + 64ULL * 1024;
    CHECK_ACL(aclrtMemcpy(dstCtx, ctxSize, (void *)ctxAddr, ctxSize, ACL_MEMCPY_DEVICE_TO_HOST));
    dstCtx[ctxSize] = 0;

    CHECK_ACL(strcmp(srcCtx, dstCtx));
    std::cout << "[OUTPUT] check set context ok" << std::endl;
}

int32_t main(int32_t argc, char* argv[])
{
    int rankSize = atoi(argv[1]);
    int rankId = atoi(argv[2]);
    std::string ipport = argv[3];
    std::cout << "[TEST] input rank_size: " << rankSize << " rank_id:" << rankId << " input_ip: " <<ipport << std::endl;

    if (rankSize != (rankSize & (~(rankSize - 1)))) {
        std::cout << "[TEST] input rank_size: " << rankSize << " is not the power of 2" << std::endl;
        return -1;
    }

    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = rankId % gNpuNum;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    auto ret = smem_set_conf_store_tls(false, nullptr, 0);
    if (ret != 0) {
        ERROR_LOG("[TEST] smem set tls info failed, ret:%d, rank:%d", ret, rankId);
        return -1;
    }

    ret = smem_init(0);
    if (ret != 0) {
        ERROR_LOG("[TEST] smem init failed, ret:%d, rank:%d", ret, rankId);
        return -1;
    }
    smem_shm_config_t config;
    (void)smem_shm_config_init(&config);
    ret = smem_shm_init(ipport.c_str(), rankSize, rankId, deviceId, &config);
    if (ret != 0) {
        ERROR_LOG("[TEST] shm init failed, ret:%d, rank:%d", ret, rankId);
        return -1;
    }

    uint32_t flags = 0;
    void *gva = nullptr;
    smem_shm_t handle = smem_shm_create(0, rankSize, rankId, gNpuMallocSpace, SMEMS_DATA_OP_MTE, flags, &gva);
    if (handle == nullptr || gva == nullptr) {
        ERROR_LOG("[TEST] smem_shm_create failed, rank:%d", rankId);
        return -1;
    }
    WARN_LOG("[TEST] smem_shm_create, size %llu, rank:%d", gNpuMallocSpace, rankId);
    TestContext(handle);
    TestAllShift(stream, (uint8_t *)gva, rankId, rankSize);

    std::cout << "[TEST] begin to exit...... rank: " << rankId << std::endl;
    smem_shm_destroy(handle, flags);

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
    return 0;
}
