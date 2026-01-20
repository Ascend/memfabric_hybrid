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
#include <iostream>
#include <sstream>
#include <limits> // 用于std::numeric_limits
#include <cstring>
#include "smem.h"
#include "smem_shm.h"
#include "shm_rdma_test_dev.h"
#include "data_utils.h"
#include "acl/acl.h"

static uint32_t gNpuNum = 16;
static uint64_t gNpuMallocSpace = 1024UL * 1024UL * 64;
static uint32_t messageSize = 64;
constexpr uint32_t DEBUG_PRINT_SIZE = 120;
enum Index : uint8_t {
    INDEX_0 = 0U,
    INDEX_1 = 1U,
    INDEX_2 = 2U,
    INDEX_3 = 3U,
};

static int32_t TestRDMAPollCQ(aclrtStream stream, uint8_t *gva, uint32_t rankId, uint32_t rankSize)
{
    uint64_t *xHost;
    size_t totalSize = messageSize * rankSize;
    CHECK_ACL(aclrtMallocHost((void **)(&xHost), totalSize));
    for (uint32_t i = 0; i < messageSize / sizeof(uint64_t); i++) {
        xHost[i] = rankId;
    }

    CHECK_ACL(aclrtMemcpy(gva + rankId * gNpuMallocSpace + rankId * messageSize, messageSize, xHost, messageSize,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    shm_rdma_pollcq_test_do(stream, gva, gNpuMallocSpace);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    sleep(1);

    CHECK_ACL(aclrtMemcpy(xHost, totalSize, gva + rankId * gNpuMallocSpace, totalSize, ACL_MEMCPY_DEVICE_TO_HOST));
    for (uint32_t i = 0; i < rankSize; i++) {
        CHECK_EQUALS(xHost[i * messageSize / sizeof(uint64_t)], i);
    }

    for (uint32_t curRank = 0; curRank < rankSize; curRank++) {
        if (curRank == rankId) {
            continue;
        }
        CHECK_ACL(aclrtMemcpy(xHost, DEBUG_PRINT_SIZE,
                              gva + rankId * gNpuMallocSpace + rankSize * messageSize + curRank * DEBUG_PRINT_SIZE,
                              DEBUG_PRINT_SIZE, ACL_MEMCPY_DEVICE_TO_HOST));
        for (uint32_t i = 0; i < DEBUG_PRINT_SIZE / sizeof(uint64_t); i++) {
            printf("PollCQ srcRank = %d, destRank = %d, index = %d, value = %lu\n", rankId, curRank, i, xHost[i]);
        }
        sleep(1);
    }

    CHECK_ACL(aclrtFreeHost(xHost));
    return 0;
}

static int32_t TestRDMAWrite(aclrtStream stream, uint8_t *gva, uint32_t rankId, uint32_t rankSize)
{
    uint32_t *xHost;
    size_t totalSize = messageSize * rankSize;
    CHECK_ACL(aclrtMallocHost((void **)(&xHost), totalSize));
    for (uint32_t i = 0; i < messageSize / sizeof(uint32_t); i++) {
        xHost[i] = rankId;
    }

    CHECK_ACL(aclrtMemcpy(gva + rankId * gNpuMallocSpace + rankId * messageSize, messageSize, xHost, messageSize,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    shm_rdma_write_test_do(stream, gva, gNpuMallocSpace);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    sleep(1);

    CHECK_ACL(aclrtMemcpy(xHost, totalSize, gva + rankId * gNpuMallocSpace, totalSize, ACL_MEMCPY_DEVICE_TO_HOST));
    for (uint32_t i = 0; i < rankSize; i++) {
        CHECK_EQUALS(xHost[i * messageSize / sizeof(uint32_t)], i);
    }

    CHECK_ACL(aclrtFreeHost(xHost));
    return 0;
}

static int32_t TestRDMARead(aclrtStream stream, uint8_t *gva, uint32_t rankId, uint32_t rankSize)
{
    uint32_t *xHost;
    size_t totalSize = messageSize * rankSize;
    CHECK_ACL(aclrtMallocHost((void **)(&xHost), totalSize));
    for (uint32_t i = 0; i < messageSize / sizeof(uint32_t); i++) {
        xHost[i] = rankId;
    }

    CHECK_ACL(aclrtMemcpy(gva + rankId * gNpuMallocSpace + rankId * messageSize, messageSize, xHost, messageSize,
                          ACL_MEMCPY_HOST_TO_DEVICE));
    shm_rdma_write_test_do(stream, gva, gNpuMallocSpace);
    CHECK_ACL(aclrtSynchronizeStream(stream));
    sleep(1);

    CHECK_ACL(aclrtMemcpy(xHost, totalSize, gva + rankId * gNpuMallocSpace, totalSize, ACL_MEMCPY_DEVICE_TO_HOST));
    for (uint32_t i = 0; i < rankSize; i++) {
        CHECK_EQUALS(xHost[i * messageSize / sizeof(uint32_t)], i);
    }

    CHECK_ACL(aclrtFreeHost(xHost));
    return 0;
}

static int32_t TestGetQPInfo(aclrtStream stream, uint8_t *gva, uint32_t rankId, uint32_t rankSize)
{
    uint64_t *xHost;
    size_t totalSize = 120;
    size_t elementCount = totalSize / sizeof(uint64_t);
    CHECK_ACL(aclrtMallocHost((void **)(&xHost), totalSize));
    std::fill(xHost, xHost + elementCount, 0);

    for (uint32_t curRank = 0; curRank < rankSize; curRank++) {
        if (curRank == rankId) {
            continue;
        }
        shm_rdma_get_qpinfo_test_do(stream, gva + rankId * gNpuMallocSpace, curRank);
        CHECK_ACL(aclrtSynchronizeStream(stream));
        sleep(1);

        CHECK_ACL(aclrtMemcpy(xHost, totalSize, gva + rankId * gNpuMallocSpace, totalSize, ACL_MEMCPY_DEVICE_TO_HOST));
        for (uint32_t i = 0; i < totalSize / sizeof(uint64_t); i++) {
            printf("GetQPInfo srcRank = %d, destRank = %d, index = %d, value = %lu\n", rankId, curRank, i, xHost[i]);
        }
    }

    CHECK_ACL(aclrtFreeHost(xHost));
    return 0;
}

int32_t main(int32_t argc, char *argv[])
{
    int rankSize = atoi(argv[INDEX_1]);
    int rankId = atoi(argv[INDEX_2]);
    std::string ipport = argv[INDEX_3];
    std::cout << "[TEST] input rank_size: " << rankSize << " rank_id:" << rankId << " input_ip: " << ipport
              << std::endl;

    if (rankSize != (rankSize & (~(rankSize - 1)))) {
        std::cout << "[TEST] input rank_size: " << rankSize << " is not the power of 2" << std::endl;
        return -1;
    }

    CHECK_ACL(aclInit(nullptr));
    int32_t deviceId = rankId % gNpuNum;
    CHECK_ACL(aclrtSetDevice(deviceId));
    aclrtStream stream = nullptr;
    CHECK_ACL(aclrtCreateStream(&stream));

    auto ret = smem_init(0);
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
    smem_shm_t handle = smem_shm_create(0, rankSize, rankId, gNpuMallocSpace, SMEMS_DATA_OP_RDMA, flags, &gva);
    if (handle == nullptr || gva == nullptr) {
        ERROR_LOG("[TEST] smem_shm_create failed, rank:%d", rankId);
        return -1;
    }
    INFO_LOG("[TEST] smem_shm_create gva %p, size %lu, rank:%d", gva, gNpuMallocSpace, rankId);
    TestGetQPInfo(stream, (uint8_t *)gva, rankId, rankSize);
    sleep(1);
    TestRDMAWrite(stream, (uint8_t *)gva, rankId, rankSize);

    std::cout << "[TEST] begin to exit...... rank: " << rankId << std::endl;
    smem_shm_destroy(handle, flags);

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
    return 0;
}