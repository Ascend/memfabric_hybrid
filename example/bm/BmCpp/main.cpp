/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <iostream>
#include <algorithm>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include "acl/acl.h"
#include "smem.h"
#include "smem_bm.h"
#include "barrier_util.h"
#include "mf_num_util.h"

#ifndef LOG_FILENAME_SHORT
#define LOG_FILENAME_SHORT (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define LOG_INFO(msg) std::cout << LOG_FILENAME_SHORT << ":" << __LINE__ << "[INFO]" << msg << std::endl;
#define LOG_WARN(msg) std::cout << LOG_FILENAME_SHORT << ":" << __LINE__ << "[WARN]" << msg << std::endl;
#define LOG_ERROR(msg) std::cout << LOG_FILENAME_SHORT << ":" << __LINE__ << "[ERR]" << msg << std::endl;

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
const int32_t COPY_2D_HEIGHT = 2;
const int32_t COPY_2D_SIZE = COPY_SIZE * COPY_2D_HEIGHT;
const uint64_t GVA_SIZE = 1024ULL * 1024 * 1024;
BarrierUtil *g_barrier = nullptr;

void GenerateData(void *ptr, int32_t rank)
{
    const int32_t FACTOR = 23;
    const int32_t INCREMENT = 17; // 23, 17 are primes
    int32_t *arr = (int32_t *)ptr;
    static int32_t mod = INT16_MAX;
    int32_t base = rank;
    for (uint32_t i = 0; i < COPY_SIZE / sizeof(int); i++) {
        base = (base * FACTOR + INCREMENT) % mod;
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
        if (arr1[i] != arr2[i]) {
            return false;
        }
    }
    return true;
}

int decrypt_handler_example(const char *cipherText, size_t cipherTextLen, char *plainText, size_t &plainTextLen)
{
    if (cipherText == nullptr || plainText == nullptr) {
        return -1;
    }

    // do decrypt here
    // check out length < plainTextLen

    size_t outPlainTextLen = cipherTextLen;

    std::copy_n(cipherText, cipherTextLen, plainText);
    plainText[cipherTextLen] = '\0';
    plainTextLen = outPlainTextLen;
    return 0;
}

void extern_logger_example(int level, const char *msg)
{
    std::cout << "level:" << level << ":" << (msg == nullptr ? "" : msg) << std::endl;
}

int32_t PreInit(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, std::string ipPort, int autoRank,
    aclrtStream *stream)
{
    auto ret = aclInit(nullptr);
    CHECK_RET_ERR(ret, "acl init failed, ret:" << ret << " rank:" << rankId);
    ret = aclrtSetDevice(deviceId);
    CHECK_RET_ERR(ret, "acl set device failed, ret:" << ret << " rank:" << rankId);

    aclrtStream ss = nullptr;
    ret = aclrtCreateStream(&ss);
    CHECK_RET_ERR(ret, "acl create stream failed, ret:" << ret << " rank:" << rankId);

    ret = smem_set_conf_store_tls(false, nullptr, 0);
    CHECK_RET_ERR(ret, "set tls info failed, ret:" << ret << " rank:" << rankId);

    ret = smem_init(0);
    CHECK_RET_ERR(ret, "smem init failed, ret:" << ret << " rank:" << rankId);

    ret = smem_set_extern_logger(extern_logger_example);
    CHECK_RET_ERR(ret, "smem set extern logger failed, ret:" << ret << " rank:" << rankId);

    smem_bm_config_t config;
    (void)smem_bm_config_init(&config);
    if (autoRank) {
        config.autoRanking = true;
    } else {
        config.rankId = rankId;
    }
    ret = smem_bm_init(ipPort.c_str(), rkSize, deviceId, &config);
    CHECK_RET_ERR(ret, "smem bm init failed, ret:" << ret << " rank:" << rankId);

    g_barrier = new BarrierUtil;
    CHECK_RET_ERR((g_barrier == nullptr), "malloc failed, rank:" << rankId);
    ret = g_barrier->Init(deviceId, rankId, rkSize, ipPort);
    CHECK_RET_ERR(ret, "barrier init failed, rank:" << rankId);

    *stream = ss;
    return 0;
}

void FinalizeAll(aclrtStream *stream, uint32_t deviceId)
{
    delete g_barrier;
    g_barrier = nullptr;
    smem_bm_uninit(0);
    aclrtDestroyStream(stream);
    aclrtResetDevice(deviceId);
    aclFinalize();
}

void CheckSmemCopy(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, std::string ipPort, int autoRank,
                   smem_bm_t handle, void *gva)
{
    int ret = 0;
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

    smem_copy_params params = {hostSrc, remote, COPY_SIZE};
    ret = smem_bm_copy(handle, &params, SMEMB_COPY_H2G, 0);
    CHECK_RET_VOID(ret, "copy host to gva failed, ret:" << ret << " rank:" << rankId);

    params = {deviceSrc, (void *)((uint64_t)remote + COPY_SIZE), COPY_SIZE};
    ret = smem_bm_copy(handle, &params, SMEMB_COPY_L2G, 0);
    CHECK_RET_VOID(ret, "copy hbm to gva failed, ret:" << ret << " rank:" << rankId);

    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after copy, ret:" << ret << " rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm copy ok, rank:" << rankId);
    // check
    GenerateData(hostDst, (rankId + rkSize - 1) % rkSize);
    params = {gva, hostSrc, COPY_SIZE};
    ret = smem_bm_copy(handle, &params, SMEMB_COPY_G2H, 0);
    CHECK_RET_VOID(ret, "copy gva to host failed, ret:" << ret << " rank:" << rankId);
    CHECK_RET_VOID((!CheckData(hostDst, hostSrc)), "check G2H data failed, rank:" << rankId);

    params = {(void *)((uint64_t)gva + COPY_SIZE), deviceSrc, COPY_SIZE};
    ret = smem_bm_copy(handle, &params, SMEMB_COPY_G2L, 0);
    CHECK_RET_VOID(ret, "copy hbm to gva failed, ret:" << ret << " rank:" << rankId);

    ret = aclrtMemcpy(hostSrc, COPY_SIZE, deviceSrc, COPY_SIZE, ACL_MEMCPY_DEVICE_TO_HOST);
    CHECK_RET_VOID(ret, "copy device to host failed, ret:" << ret << " rank:" << rankId);
    CHECK_RET_VOID((!CheckData(hostDst, hostSrc)), "check G2L data failed, rank:" << rankId);

    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after check, ret:" << ret << " rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm check ok, rank:" << rankId);
    // exit
    free(hostSrc);
    free(hostDst);
    aclrtFree(deviceSrc);
}

void CheckSmemCopy2D(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, std::string ipPort, int autoRank,
                     smem_bm_t handle, void *gva)
{
    int ret = 0;
    void *remote = smem_bm_ptr(handle, (rankId + 1) % rkSize);
    void *hostSrc = malloc(COPY_2D_SIZE);
    void *hostDst = malloc(COPY_2D_SIZE);
    CHECK_RET_VOID((hostSrc == nullptr || hostDst == nullptr), "malloc host failed, rank:" << rankId);

    void *deviceSrc = nullptr;
    ret = aclrtMalloc((void**)(&deviceSrc), COPY_2D_SIZE, ACL_MEM_MALLOC_HUGE_FIRST);
    CHECK_RET_VOID((ret != 0 || deviceSrc == nullptr), "malloc device failed, ret:" << ret << " rank:" << rankId);

    GenerateData(hostSrc, rankId);
    ret = aclrtMemcpy(deviceSrc, COPY_2D_SIZE, hostSrc, COPY_2D_SIZE, ACL_MEMCPY_HOST_TO_DEVICE);
    CHECK_RET_VOID(ret, "copy host to device failed, ret:" << ret << " rank:" << rankId);

    smem_copy_2d_params params = {hostSrc, COPY_SIZE, remote, COPY_SIZE, COPY_SIZE, COPY_2D_HEIGHT};
    ret = smem_bm_copy_2d(handle, &params, SMEMB_COPY_H2G, 0);
    CHECK_RET_VOID(ret, "copy2d host to gva failed, ret:" << ret << " rank:" << rankId);

    params = {deviceSrc, COPY_SIZE, (void *)((uint64_t)remote + COPY_2D_SIZE), COPY_SIZE, COPY_SIZE, COPY_2D_HEIGHT};
    ret = smem_bm_copy_2d(handle, &params, SMEMB_COPY_L2G, 0);
    CHECK_RET_VOID(ret, "copy2d hbm to gva failed, ret:" << ret << " rank:" << rankId);

    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after copy, ret:" << ret << " rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm copy ok, rank:" << rankId);
    // check
    GenerateData(hostDst, (rankId + rkSize - 1) % rkSize);
    params = {gva, COPY_SIZE, hostSrc, COPY_SIZE, COPY_SIZE, COPY_2D_HEIGHT};
    ret = smem_bm_copy_2d(handle, &params, SMEMB_COPY_G2H, 0);
    CHECK_RET_VOID(ret, "copy gva to host failed, ret:" << ret << " rank:" << rankId);
    CHECK_RET_VOID((!CheckData(hostDst, hostSrc)), "check copy2d G2H data failed, rank:" << rankId);

    params = {(void *)((uint64_t)gva + COPY_2D_SIZE), COPY_SIZE, deviceSrc, COPY_SIZE, COPY_SIZE, COPY_2D_HEIGHT};
    ret = smem_bm_copy_2d(handle, &params, SMEMB_COPY_G2L, 0);
    CHECK_RET_VOID(ret, "copy hbm to gva failed, ret:" << ret << " rank:" << rankId);

    ret = aclrtMemcpy(hostSrc, COPY_2D_HEIGHT, deviceSrc, COPY_2D_HEIGHT, ACL_MEMCPY_DEVICE_TO_HOST);
    CHECK_RET_VOID(ret, "copy device to host failed, ret:" << ret << " rank:" << rankId);
    CHECK_RET_VOID((!CheckData(hostDst, hostSrc)), "check copy2d G2L  data failed, rank:" << rankId);

    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after check, ret:" << ret << " rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm check copy2d ok, rank:" << rankId);
    // exit
    free(hostSrc);
    free(hostDst);
    aclrtFree(deviceSrc);
}

void SubProcessRuning(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, std::string ipPort, int autoRank)
{
    aclrtStream stream;
    auto ret = PreInit(deviceId, rankId, rkSize, ipPort, autoRank, &stream);
    CHECK_RET_VOID(ret, "pre init failed, ret:" << ret << " rank:" << rankId);

    void *gva = nullptr;
    smem_bm_t handle = smem_bm_create(0, 0, SMEMB_DATA_OP_SDMA, 0, GVA_SIZE, 0);
    CHECK_RET_VOID((handle == nullptr), "smem_bm_create failed, rank:" << rankId);

    ret = smem_bm_join(handle, 0, &gva);
    CHECK_RET_VOID(ret, "smem_bm_join failed, ret:" << ret << " rank:" << rankId);
    LOG_INFO("smem_bm_create succeeded, rank:" << rankId);

    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm init ok, rank:" << rankId);

    CheckSmemCopy(deviceId, rankId, rkSize, ipPort, autoRank, handle, gva);
    CheckSmemCopy2D(deviceId, rankId, rkSize, ipPort, autoRank, handle, gva);

    smem_bm_destroy(handle);
    FinalizeAll(&stream, deviceId);
}

int main(int32_t argc, char* argv[])
{
    int rankSize = atoi(argv[INDEX_1]);
    int rankNum = atoi(argv[INDEX_2]);
    int rankStart = atoi(argv[INDEX_3]);
    std::string ipport = argv[INDEX_4];
    int autoRank = 0;
    if (argc > 5) {
        autoRank = atoi(argv[INDEX_5]);
    }

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