/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file constains code of cpu debug and npu code.We read data from bin file
 * and write result to file.
 */
#include <iostream>
#include <sstream>
#include <limits> // 用于std::numeric_limits
#include "smem.h"
#include "smem_shm.h"
#include "shm_all_reduce.h"
#include "data_utils.h"
#include "acl/acl.h"

static uint32_t gNpuNum = 16;
static uint64_t gNpuMallocSpace = 1024UL * 1024UL * 1024;
static uint64_t gFlagOffset = 1024UL * 1024UL; // 前1M作为flag空间
static size_t gDataByteSize = 16 * 2048 * sizeof(uint16_t);   // uint16_t represent half
enum Index : uint8_t {
    INDEX_0 = 0U,
    INDEX_1 = 1U,
    INDEX_2 = 2U,
    INDEX_3 = 3U,
};

// FNV-1a 32-bit hash function
uint32_t fnv1a_32(const void *data, size_t length)
{
    const unsigned char *p = static_cast<const unsigned char *>(data);
    uint32_t hash = 0x811c9dc5; // FNV offset basis for 32-bit

    for (size_t i = 0; i < length; ++i) {
        hash ^= p[i]; // XOR with byte
        hash *= 0x01000193; // Multiply by prime number
    }

    return hash;
}

// 将哈希值转换为十六进制字符串表示
std::string hashToHexString(uint32_t hash)
{
    const int HASH_STRING_WIDTH = 8;
    std::ostringstream hexStream;
    hexStream << std::hex << std::setw(HASH_STRING_WIDTH) << std::setfill('0') << hash;
    return hexStream.str();
}

static int32_t TestAllReduce(aclrtStream stream, uint8_t *gva, uint32_t rankId, uint32_t rankSize)
{
    uint32_t blockDim = 8;

    // 申请本地主机内存 和 device内存
    uint8_t *inputHost;
    uint8_t *outputHost;
    CHECK_ACL(aclrtMallocHost((void**)(&inputHost), gDataByteSize));
    CHECK_ACL(aclrtMallocHost((void**)(&outputHost), gDataByteSize));

    // 分配共享内存
    uint8_t* localShm = gva + (rankId * gNpuMallocSpace) + gFlagOffset;
    uint8_t* inputShm = localShm;
    uint8_t* outputShm = inputShm;

    // 载入 input 数据到 device内存
    std::string fileNmae = "./input/input_" + std::to_string(rankId) + ".bin";
    ReadFile(fileNmae, gDataByteSize, inputHost, gDataByteSize);

    // 将本地 host copy到 shm local
    int32_t ret = aclrtMemcpy(inputShm, gDataByteSize, inputHost, gDataByteSize, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        ERROR_LOG("aclrtMemcpy to local shm failed, ret: %d, rankId: %u", ret, rankId);
        return -1;
    }

    // 用 shm 内存 执行算子
    INFO_LOG("rankId: %u, start run allreduce...", rankId);
    shm_all_reduce_do(blockDim, stream, gva, gNpuMallocSpace, gFlagOffset, rankId, rankSize);
    CHECK_ACL_RET(aclrtSynchronizeStream(stream), "after allreduce sync stream");
    INFO_LOG("rankId: %u, end run allreduce...", rankId);

    // 将ouput copy到 host
    ret = aclrtMemcpy(outputHost, gDataByteSize, outputShm, gDataByteSize, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        ERROR_LOG("aclrtMemcpy from local shm failed, ret: %d, rankId: %u", ret, rankId);
        return -1;
    }

    // 比较MD5
    uint32_t hashValue1 = fnv1a_32(outputHost, gDataByteSize);
    std::string hexHash1 = hashToHexString(hashValue1);
    if (rankId == 0) {
        WriteFile("./output/output_z.bin", outputHost, gDataByteSize);
    }
    ReadFile("./output/golden.bin", gDataByteSize, outputHost, gDataByteSize);
    uint32_t hashValue2 = fnv1a_32(outputHost, gDataByteSize);
    std::string hexHash2 = hashToHexString(hashValue2);
    if (hexHash1 == hexHash2) {
        INFO_LOG("rankId: %u do all reduce ok, output: %s", rankId, hexHash2.c_str());
    } else {
        ERROR_LOG("rankId: %u do all reduce failed, ori: %s, cal: %s", rankId, hexHash1.c_str(), hexHash2.c_str());
    }

    CHECK_ACL(aclrtFreeHost(inputHost));
    CHECK_ACL(aclrtFreeHost(outputHost));
    return 0;
}

int32_t main(int32_t argc, char* argv[])
{
    int rankSize = atoi(argv[INDEX_1]);
    int rankId = atoi(argv[INDEX_2]);
    std::string ipport = argv[INDEX_3];
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
    smem_shm_t handle = smem_shm_create(0, rankSize, rankId, gNpuMallocSpace, SMEMS_DATA_OP_MTE, flags, &gva);
    if (handle == nullptr || gva == nullptr) {
        ERROR_LOG("[TEST] smem_shm_create failed, rank:%d", rankId);
        return -1;
    }
    WARN_LOG("[TEST] smem_shm_create, size %llu, rank:%d", static_cast<unsigned long long>(gNpuMallocSpace), rankId);
    TestAllReduce(stream, (uint8_t *)gva, rankId, rankSize);

    std::cout << "[TEST] begin to exit...... rank: " << rankId << std::endl;
    smem_shm_destroy(handle, flags);

    CHECK_ACL(aclrtDestroyStream(stream));
    CHECK_ACL(aclrtResetDevice(deviceId));
    CHECK_ACL(aclFinalize());
    return 0;
}
