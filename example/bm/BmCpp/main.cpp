/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <bits/stdc++.h>
#include "acl/acl.h"
#include "smem.h"
#include "smem_bm.h"
#include "barrier_util.h"

#define LOG_INFO(msg) std::cout << __FILE__ << ":" << __LINE__ << "[INFO]" << msg << std::endl
#define LOG_WARN(msg) std::cout << __FILE__ << ":" << __LINE__ << "[WARN]" << msg << std::endl
#define LOG_ERROR(msg) std::cout << __FILE__ << ":" << __LINE__ << "[ERR]" << msg << std::endl

#define CHECK_RET_ERR(x, msg)   \
do {                            \
    if ((x) != 0) {             \
        LOG_ERROR(msg);         \
        return -1;              \
    }                           \
} while (0)

#define CHECK_RET_VOID(x, msg)  \
do {                            \
    if ((x) != 0) {             \
        LOG_ERROR(msg);         \
        return;                 \
    }                           \
} while (0)

const int32_t RANK_SIZE_MAX = 16;
const int32_t COPY_SIZE = 20 * 1024 * 1024;
const uint64_t GVA_SIZE = 4 * 1024ULL * 1024 * 1024;
const int32_t RANDOM_MULTIPLIER = 23;
const int32_t RANDOM_INCREMENT = 17;
const int32_t NEGATIVE_RATIO_DIVISOR = 3;
const smem_bm_data_op_type OP_TYPE = SMEMB_DATA_OP_DEVICE_RDMA;
const int RUN_BATCH = 0;

BarrierUtil *g_barrier = nullptr;
constexpr int RANK_SIZE_ARG_INDEX = 1;
constexpr int RANK_NUM_ARG_INDEX = 2;
constexpr int RANK_START_ARG_INDEX = 3;
constexpr int IPPORT_ARG_INDEX = 4;

void GenerateData(void *ptr, int32_t rank, uint32_t len = COPY_SIZE)
{
    if (ptr == nullptr) {
        return;
    }
    int32_t *arr = (int32_t *)ptr;
    static int32_t mod = INT16_MAX;
    int32_t base = rank;
    for (uint32_t i = 0; i < len / sizeof(int); i++) {
        base = (base * RANDOM_MULTIPLIER + RANDOM_INCREMENT) % mod;
        if ((i + rank) % NEGATIVE_RATIO_DIVISOR == 0) {
            arr[i] = -base; // 构造三分之一的负数
        } else {
            arr[i] = base;
        }
    }
}

bool CheckData(void *base, void *ptr, uint32_t len = COPY_SIZE)
{
    if (base == nullptr || ptr == nullptr) {
        return false;
    }
    int32_t *arr1 = (int32_t *)base;
    int32_t *arr2 = (int32_t *)ptr;
    for (uint32_t i = 0; i < len / sizeof(int); i++) {
        if (arr1[i] != arr2[i]) {
            return false;
        }
    }
    return true;
}

int32_t PreInit(uint32_t deviceId, uint32_t rankId, uint32_t rkSize,
    std::string ipPort, aclrtStream *stream)
{
    auto ret = aclInit(nullptr);
    CHECK_RET_ERR(ret, "acl init failed, ret:" << ret << " rank:" << rankId);
    ret = aclrtSetDevice(deviceId);
    CHECK_RET_ERR(ret, "acl set device failed, ret:" << ret << " rank:" << rankId);

    aclrtStream ss = nullptr;
    ret = aclrtCreateStream(&ss);
    CHECK_RET_ERR(ret, "acl create stream failed, ret:" << ret << " rank:" << rankId);

    ret = smem_init(0);
    CHECK_RET_ERR(ret, "smem init failed, ret:" << ret << " rank:" << rankId);

    smem_set_log_level(2);

    smem_bm_config_t config;
    (void)smem_bm_config_init(&config);
    std::string url = "tcp://0.0.0.0/0:10005";
    std::copy_n(url.c_str(), url.size(), config.hcomUrl);
    config.autoRanking = false;
    config.rankId = rankId;

    config.flags = 2; // init gvm
    ret = smem_bm_init(ipPort.c_str(), rkSize, deviceId, &config);
    CHECK_RET_ERR(ret, "smem bm init failed, ret:" << ret << " rank:" << rankId);

    g_barrier = new (std::nothrow) BarrierUtil;
    CHECK_RET_ERR((g_barrier == nullptr), "malloc failed, rank:" << rankId);
    ret = g_barrier->Init(deviceId, rankId, rkSize, ipPort);
    CHECK_RET_ERR(ret, "barrier init failed, rank:" << rankId << " ret:" << ret << " " << errno);

    *stream = ss;
    LOG_INFO(" ==================== [TEST] init, rank:" << rankId << " devId:" << deviceId);
    return 0;
}

void FinalizeAll(aclrtStream *stream, uint32_t deviceId)
{
    if (g_barrier != nullptr) {
        delete g_barrier;
        g_barrier = nullptr;
    }
    smem_bm_uninit(0);
    if (stream != nullptr) {
        aclrtDestroyStream(stream);
    }
    aclrtResetDevice(deviceId);
    aclFinalize();
}

constexpr uint32_t LAYER_BLOCK_NUM = 1024;
constexpr uint32_t BATCH_COPY_NUM  = 20;
constexpr uint32_t BATCH_COPY_LAYER  = 122;
constexpr uint32_t BATCH_COPY_ARR_LEN = (BATCH_COPY_NUM * BATCH_COPY_LAYER);
constexpr uint32_t BATCH_LAYER_OFFSET = 2;
void CheckBatchCopy(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, smem_bm_t handle)
{
    int ret = 0;
    uint64_t remote = (uint64_t)smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_HOST, (rankId + 1) % rkSize);

    uint64_t rSrc[BATCH_COPY_LAYER];
    uint64_t rSize[BATCH_COPY_LAYER];
    for (uint32_t i = 0; i < BATCH_COPY_LAYER; i++) {
        uint64_t len = 1024 * ((i & 1) ? 16 : 128) * LAYER_BLOCK_NUM;
        void *ptr = nullptr;
        ret = aclrtMalloc(&ptr, len, ACL_MEM_MALLOC_HUGE_FIRST);
        CHECK_RET_VOID((ret != 0 || ptr == nullptr), "malloc device failed, ret:" << ret << " rank:" << rankId);
        rSrc[i] = reinterpret_cast<uint64_t>(ptr);
        rSize[i] = len;

        if (OP_TYPE == SMEMB_DATA_OP_DEVICE_RDMA) {
            smem_bm_register_user_mem(handle, rSrc[i], rSize[i]);
        }
    }

    void* srcList[BATCH_COPY_ARR_LEN];
    void* dstList[BATCH_COPY_ARR_LEN];
    uint64_t sizeList[BATCH_COPY_ARR_LEN];
    smem_batch_copy_params param = {(void **)srcList, dstList, sizeList, BATCH_COPY_ARR_LEN};

    // LD2GH
    void *hostSrc = malloc(COPY_SIZE);
    GenerateData(hostSrc, rankId);

    uint64_t offset = 0;
    for (uint32_t i = 0; i < BATCH_COPY_NUM; i++) {
        uint64_t ckLen = 0;
        for (uint32_t j = 0; j < BATCH_COPY_LAYER; j++) {
            uint64_t len = 1024 * ((j & 1) ? 16 : 128);
            void *devPtr = reinterpret_cast<void *>(rSrc[j] + (i * BATCH_LAYER_OFFSET) * len);
            ret = aclrtMemcpy(devPtr, len, (void *)((uint64_t)hostSrc + ckLen), len, ACL_MEMCPY_HOST_TO_DEVICE);
            CHECK_RET_VOID(ret, "copy host to device failed, ret:" << ret << " rank:" << rankId);

            srcList[i * BATCH_COPY_LAYER + j] = devPtr;
            dstList[i * BATCH_COPY_LAYER + j] = reinterpret_cast<void *>(remote + offset);
            sizeList[i * BATCH_COPY_LAYER + j] = len;
            offset += len;
            ckLen += len;
        }
    }

    ret = smem_bm_copy_batch(handle, &param, SMEMB_COPY_L2G, 0);
    CHECK_RET_VOID(ret, "copy hbm to gva failed, ret:" << ret << " rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm copy end, rank:" << rankId << " task_num:" << BATCH_COPY_ARR_LEN
        << " addr:" << std::hex << remote);
}

void CheckBatchCopy2(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, smem_bm_t handle)
{
    int ret = 0;
    uint64_t local = (uint64_t)smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_HOST, rankId);

    LOG_INFO(" ==================== [TEST] bm check start, rank:" << rankId << " addr:" << std::hex << local);
    uint64_t rSrc[BATCH_COPY_LAYER];
    uint64_t rSize[BATCH_COPY_LAYER];
    for (uint32_t i = 0; i < BATCH_COPY_LAYER; i++) {
        uint64_t len = 1024 * ((i & 1) ? 16 : 128) * LAYER_BLOCK_NUM;
        void *ptr = nullptr;
        ret = aclrtMalloc(&ptr, len, ACL_MEM_MALLOC_HUGE_FIRST);
        CHECK_RET_VOID((ret != 0 || ptr == nullptr), "malloc device failed, ret:" << ret << " rank:" << rankId);
        rSrc[i] = reinterpret_cast<uint64_t>(ptr);
        rSize[i] = len;

        if (OP_TYPE == SMEMB_DATA_OP_DEVICE_RDMA) {
            smem_bm_register_user_mem(handle, rSrc[i], rSize[i]);
        }
    }

    void* srcList[BATCH_COPY_ARR_LEN];
    void* dstList[BATCH_COPY_ARR_LEN];
    uint64_t sizeList[BATCH_COPY_ARR_LEN];
    smem_batch_copy_params param = {(void **)srcList, dstList, sizeList, BATCH_COPY_ARR_LEN};

    uint64_t offset = 0;
    for (uint32_t i = 0; i < BATCH_COPY_NUM; i++) {
        for (uint32_t j = 0; j < BATCH_COPY_LAYER; j++) {
            uint64_t len = 1024 * ((j & 1) ? 16 : 128);
            void *devPtr = reinterpret_cast<void *>(rSrc[j] + (i * BATCH_LAYER_OFFSET) * len);

            srcList[i * BATCH_COPY_LAYER + j] = reinterpret_cast<void *>(local + offset);
            dstList[i * BATCH_COPY_LAYER + j] = devPtr;
            sizeList[i * BATCH_COPY_LAYER + j] = len;
            offset += len;
        }
    }
    ret = smem_bm_copy_batch(handle, &param, SMEMB_COPY_G2L, 0);
    CHECK_RET_VOID(ret, "copy gva to hbm failed, ret:" << ret << " rank:" << rankId);

    void *hostSrc = malloc(COPY_SIZE);
    void *hostDest = malloc(COPY_SIZE);
    GenerateData(hostSrc, (rankId + rkSize - 1) % rkSize);

    for (uint32_t i = 0; i < BATCH_COPY_NUM; i++) {
        uint64_t ckLen = 0;
        for (uint32_t j = 0; j < BATCH_COPY_LAYER; j++) {
            uint64_t len = 1024 * ((j & 1) ? 16 : 128);
            void *devPtr = reinterpret_cast<void *>(rSrc[j] + (i * BATCH_LAYER_OFFSET) * len);
            ret = aclrtMemcpy(reinterpret_cast<void *>((uint64_t)hostDest + ckLen), len,
                              devPtr, len, ACL_MEMCPY_DEVICE_TO_HOST);
            CHECK_RET_VOID(ret, "copy host to device failed, ret:" << ret << " rank:" << rankId);
            ckLen += len;
        }
        ret = CheckData(hostSrc, hostDest, 1024);
        CHECK_RET_VOID((ret == false), "check G2H data failed, rank:" << rankId);
    }
    LOG_INFO(" ==================== [TEST] bm check ok, rank:" << rankId);
}

constexpr uint64_t bitCheckSize = 2 * 1024ULL * 1024 * 1024;
constexpr int BIG_COPY_NUM = 2;
void BigCopy(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, smem_bm_t handle)
{
    void *remote_host = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_HOST, (rankId + 1) % rkSize);
    void *local_host = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_HOST, rankId);
    void *remote_dev = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_DEVICE, (rankId + 1) % rkSize);
    void *local_dev = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_DEVICE, rankId);

    GenerateData(local_host, rankId, bitCheckSize);

    int ret;
    for (int i = 0; i < BIG_COPY_NUM; i++) {
        smem_copy_params params{};
        params.dest = remote_dev;
        params.src = local_host;
        params.dataSize = bitCheckSize;
        ret = smem_bm_copy(handle, &params, SMEMB_COPY_G2G, 0);
        CHECK_RET_VOID(ret, "copy g2g failed, ret:" << ret << " rank:" << rankId <<
            " ptr:" << local_host << " " << remote_dev);
    }
    LOG_INFO(" ==================== [TEST] bm copy ok, rank:" << rankId);
}

void BigCopyCheck(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, smem_bm_t handle)
{
    void *base = malloc(bitCheckSize);
    GenerateData(base, (rankId + rkSize - 1) % rkSize, bitCheckSize);

    void *local_host = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_HOST, rankId);
    void *local_dev = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_DEVICE, rankId);

    int ret;
    for (int i = 0; i < BIG_COPY_NUM; i++) {
        smem_copy_params params{};
        params.dest = local_host;
        params.src = local_dev;
        params.dataSize = bitCheckSize;
        ret = smem_bm_copy(handle, &params, SMEMB_COPY_G2G, 0);
        CHECK_RET_VOID(ret, "copy g2g failed, ret:" << ret << " rank:" << rankId <<
                                                    " ptr:" << local_host << " " << local_dev);
    }

    ret = CheckData(base, local_host, bitCheckSize);
    free(base);
    CHECK_RET_VOID((ret == false), "check G2G data failed, rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm check ok, rank:" << rankId);
}

void SubProcessRuning(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, std::string ipPort)
{
    aclrtStream stream;
    auto ret = PreInit(deviceId, rankId, rkSize, ipPort, &stream);
    CHECK_RET_VOID(ret, "pre init failed, ret:" << ret << " rank:" << rankId);

    smem_bm_t handle = smem_bm_create(0, 0, OP_TYPE, GVA_SIZE, GVA_SIZE, 0);
    CHECK_RET_VOID((handle == nullptr), "smem_bm_create failed, rank:" << rankId);

    ret = smem_bm_join(handle, 0);
    CHECK_RET_VOID(ret, "smem_bm_join failed, ret:" << ret << " rank:" << rankId);
    LOG_INFO("smem_bm_create, rank:" << rankId);

    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm init ok, rank:" << rankId);

    if (RUN_BATCH) {
        CheckBatchCopy(deviceId, rankId, rkSize, handle);
    } else {
        BigCopy(deviceId, rankId, rkSize, handle);
    }
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);

    if (RUN_BATCH) {
        CheckBatchCopy2(deviceId, rankId, rkSize, handle);
    } else {
        BigCopyCheck(deviceId, rankId, rkSize, handle);
    }
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);

    smem_bm_destroy(handle);
    FinalizeAll(&stream, deviceId);
}

int main(int32_t argc, char* argv[])
{
    int rankSize = atoi(argv[RANK_SIZE_ARG_INDEX]);
    int rankNum = atoi(argv[RANK_NUM_ARG_INDEX]);
    int rankStart = atoi(argv[RANK_START_ARG_INDEX]);
    std::string ipport = argv[IPPORT_ARG_INDEX];

    LOG_INFO("input rank_size:" << rankSize << " local_size:" << rankNum << " rank_offset:" << rankStart <<
        " input_ip:" << ipport);

    if (rankSize != (rankSize & (~(rankSize - 1)))) {
        LOG_ERROR("input rank_size: " << rankSize << " is not the power of 2");
        return -1;
    }
    if (rankSize > RANK_SIZE_MAX) {
        LOG_ERROR("input rank_size: " << rankSize << " is large than " << RANK_SIZE_MAX);
        return -1;
    }

    pid_t pids[RANK_SIZE_MAX];
    for (int i = 0; i < rankNum; ++i) {
        pids[i] = fork();
        if (pids[i] < 0) {
            LOG_ERROR("fork failed ! " << pids[i]);
            exit(-1);
        } else if (pids[i] == 0) {
            // subprocess
            SubProcessRuning(i, i + rankStart, rankSize, ipport);
            LOG_INFO("subprocess (" << i << ") exited.");
            exit(0);
        }
    }

    int status[RANK_SIZE_MAX];
    for (int i = 0; i < rankNum; ++i) {
        waitpid(pids[i], &status[i], 0);
        if (WIFEXITED(status[i]) && WEXITSTATUS(status[i]) != 0) {
            LOG_INFO("subprocess exit error: " << i);
        }
    }
    LOG_INFO("main process exited.");
    return 0;
}