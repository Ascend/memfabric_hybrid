/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include "acl/acl.h"
#include "smem.h"
#include "smem_bm.h"
#include "smem_shm.h"

#define LOG_INFO(msg) std::cout << __FILE__ << ":" << __LINE__ << "[INFO]" << msg << std::endl;
#define LOG_WARN(msg) std::cout << __FILE__ << ":" << __LINE__ << "[WARN]" << msg << std::endl;
#define LOG_ERROR(msg) std::cout << __FILE__ << ":" << __LINE__ << "[ERR]" << msg << std::endl;

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
const int32_t SHM_MSG_SIZE = 1024;
const int32_t COPY_SIZE = 4096;
const uint64_t GVA_SIZE = 1024ULL * 1024 * 1024;
smem_shm_t barrierHandle = nullptr;

void GenerateData(void *ptr, int32_t rank)
{
    int32_t *arr = (int32_t *)ptr;
    static int32_t mod = INT16_MAX;
    int32_t base = rank;
    for (uint32_t i = 0; i < COPY_SIZE / sizeof(int); i++) {
        base = (base * 23 + 17) % mod;
        if ((i + rank) % 3 == 0) {
            arr[i] = -base; // 构造三分之一的负数
        } else {
            arr[i] = base;
        }
    }
}

bool CheckData(void *base, void *ptr)
{
    int32_t *arr1 = (int32_t *)base;
    int32_t *arr2 = (int32_t *)ptr;
    for (uint32_t i = 0; i < COPY_SIZE / sizeof(int); i++) {
        if (arr1[i] != arr2[i]) return false;
    }
    return true;
}

int32_t PreInit(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, std::string ipPort, int autoRank, aclrtStream *stream)
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

    smem_bm_config_t config;
    (void)smem_bm_config_init(&config);
    if (autoRank) {
        config.autoRanking = true;
    } else {
        config.rankId = rankId;
    }
    ret = smem_bm_init(ipPort.c_str(), rkSize, deviceId, &config);
    CHECK_RET_ERR(ret, "smem bm init failed, ret:" << ret << " rank:" << rankId);

    smem_shm_config_t config2;
    (void)smem_shm_config_init(&config2);
    if (autoRank) config2.startConfigStore = false;
    ret = smem_shm_init(ipPort.c_str(), rkSize, rankId, deviceId, &config2);
    CHECK_RET_ERR(ret, "smem shm init failed, ret:" << ret << " rank:" << rankId);

    void *gva = nullptr;
    smem_shm_t handle = smem_shm_create(0, rkSize, rankId, GVA_SIZE, SMEMS_DATA_OP_MTE, 0, &gva);
    CHECK_RET_ERR((handle == nullptr || gva == nullptr), "smem_shm_create failed, rank:" << rankId);

    *stream = ss;
    barrierHandle = handle;
    return 0;
}

void FinalizeAll(aclrtStream *stream, uint32_t deviceId)
{
    smem_shm_destroy(barrierHandle, 0);
    smem_shm_uninit(0);
    smem_bm_uninit(0);
    aclrtDestroyStream(stream);
    aclrtResetDevice(deviceId);
    aclFinalize();
}

void SubProcessRuning(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, std::string ipPort, int autoRank)
{
    aclrtStream stream;
    auto ret = PreInit(deviceId, rankId, rkSize, ipPort, autoRank, &stream);
    CHECK_RET_VOID(ret, "pre init failed, ret:" << ret << " rank:" << rankId);

    void *gva = nullptr;
    smem_bm_t handle = smem_bm_create(0, 0, SMEMB_MEM_TYPE_HBM, SMEMB_DATA_OP_SDMA, GVA_SIZE, 0);
    CHECK_RET_VOID((handle == nullptr), "smem_bm_create failed, rank:" << rankId);

    ret = smem_bm_join(handle, 0, &gva);
    CHECK_RET_VOID(ret, "smem_bm_join failed, ret:" << ret << " rank:" << rankId);
    LOG_INFO("smem_bm_create gva:" << gva << " rank:" << rankId);

    ret = smem_shm_control_barrier(barrierHandle);
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm init ok, rank:" << rankId);
    // copy
    void *remote = smem_bm_ptr(handle, (rankId + 1) % rkSize);
    void *hostSrc = malloc(COPY_SIZE);
    void *hostDst = malloc(COPY_SIZE);
    CHECK_RET_VOID((hostSrc == nullptr || hostDst == nullptr), "malloc host failed, rank:" << rankId);

    void *deviceSrc = nullptr;
    ret = aclrtMalloc((void**)(&deviceSrc), COPY_SIZE, ACL_MEM_MALLOC_HUGE_FIRST);
    CHECK_RET_VOID((ret != 0 || deviceSrc == nullptr), "malloc device failed, ret:" << ret << " rank:" << rankId);

    GenerateData(hostSrc, rankId);
    ret = aclrtMemcpy(deviceSrc, COPY_SIZE, hostSrc, COPY_SIZE, ACL_MEMCPY_HOST_TO_DEVICE);
    CHECK_RET_VOID(ret, "copy host to device failed, ret:" << ret << " rank:" << rankId);

    ret = smem_bm_copy(handle, hostSrc, remote, COPY_SIZE, SMEMB_COPY_H2G, 0);
    CHECK_RET_VOID(ret, "copy host to gva failed, ret:" << ret << " rank:" << rankId);

    ret = smem_bm_copy(handle, deviceSrc, (void *)((uint64_t)remote + COPY_SIZE), COPY_SIZE, SMEMB_COPY_L2G, 0);
    CHECK_RET_VOID(ret, "copy hbm to gva failed, ret:" << ret << " rank:" << rankId);

    ret = smem_shm_control_barrier(barrierHandle);
    CHECK_RET_VOID(ret, "barrier failed after copy, ret:" << ret << " rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm copy ok, rank:" << rankId);
    // check
    GenerateData(hostDst, (rankId + rkSize - 1) % rkSize);
    ret = smem_bm_copy(handle, gva, hostSrc, COPY_SIZE, SMEMB_COPY_G2H, 0);
    CHECK_RET_VOID(ret, "copy gva to host failed, ret:" << ret << " rank:" << rankId);
    CHECK_RET_VOID((!CheckData(hostDst, hostSrc)), "check G2H data failed, rank:" << rankId);

    ret = smem_bm_copy(handle, (void *)((uint64_t)gva + COPY_SIZE), deviceSrc, COPY_SIZE, SMEMB_COPY_G2L, 0);
    CHECK_RET_VOID(ret, "copy hbm to gva failed, ret:" << ret << " rank:" << rankId);

    ret = aclrtMemcpy(hostSrc, COPY_SIZE, deviceSrc, COPY_SIZE, ACL_MEMCPY_DEVICE_TO_HOST);
    CHECK_RET_VOID(ret, "copy device to host failed, ret:" << ret << " rank:" << rankId);
    CHECK_RET_VOID((!CheckData(hostDst, hostSrc)), "check G2L data failed, rank:" << rankId);

    ret = smem_shm_control_barrier(barrierHandle);
    CHECK_RET_VOID(ret, "barrier failed after check, ret:" << ret << " rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm check ok, rank:" << rankId);
    // exit
    free(hostSrc);
    free(hostDst);
    aclrtFree(deviceSrc);
    FinalizeAll(&stream, deviceId);
}

int main(int32_t argc, char* argv[])
{
    int rankSize = atoi(argv[1]);
    int rankNum = atoi(argv[2]);
    int rankStart = atoi(argv[3]);
    std::string ipport = argv[4];
    int autoRank = 0;
    if (argc >5) autoRank = atoi(argv[5]);

    LOG_INFO("input rank_size:" << rankSize << " local_size:" << rankNum << " rank_offset:" << rankStart <<
        " input_ip:" << ipport << " autoRank:" << autoRank);

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
            SubProcessRuning(i, i + rankStart, rankSize, ipport, autoRank);
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