/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "acl/acl.h"
#include "smem.h"
#include "smem_bm.h"

#define LOG_INFO(msg) std::cout << __FILE__ << ":" << __LINE__ << "[INFO]" << msg << std::endl;
#define LOG_WARN(msg) std::cout << __FILE__ << ":" << __LINE__ << "[WARN]" << msg << std::endl;
#define LOG_ERROR(msg) std::cout << __FILE__ << ":" << __LINE__ << "[ERR]" << msg << std::endl;

const int32_t RANK_SIZE_MAX = 8;
const int32_t SHM_MSG_SIZE = 1024;
const int32_t COPY_SIZE = 4096;
const uint64_t GVA_SIZE = 1024ULL * 1024 * 1024;
int32_t *shmAddr = nullptr;

void SetStatus(int32_t rk, int32_t val)
{
    shmAddr[rk] = val;
    msync(shmAddr, SHM_MSG_SIZE, MS_SYNC | MS_INVALIDATE);
}

void WaitStatus(int32_t rk, int32_t x)
{
    while (shmAddr[rk] != x) {
        usleep(1000);
    }
}

int32_t CreateShmMsg()
{
    int32_t shmFd = memfd_create("bm_example_fd", MFD_CLOEXEC);
    if (shmFd == -1) {
        LOG_ERROR("memfd_create failed");
        exit(-1);
    }

    if (ftruncate(shmFd, SHM_MSG_SIZE) == -1) {
        LOG_ERROR("ftruncate failed");
        close(shmFd);
        exit(-1);
    }

    shmAddr = (int32_t *)mmap(nullptr, SHM_MSG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (shmAddr == MAP_FAILED) {
        LOG_ERROR("mmap failed");
        close(shmFd);
        exit(-1);
    }
    return shmFd;
}

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

void SubProcessRuning(uint32_t rankId, uint32_t rkSize, std::string ipPort)
{
    uint32_t deviceId = rankId;
    auto ret = aclInit(nullptr);
    if (ret != 0) {
        LOG_ERROR("acl init failed, ret:" << ret << " rank:" << rankId);
        return;
    }
    ret = aclrtSetDevice(deviceId);
    if (ret != 0) {
        LOG_ERROR("acl set device failed, ret:" << ret << " rank:" << rankId);
        return;
    }
    aclrtStream stream = nullptr;
    ret = aclrtCreateStream(&stream);
    if (ret != 0) {
        LOG_ERROR("acl create stream failed, ret:" << ret << " rank:" << rankId);
        return;
    }

    ret = smem_init(0);
    if (ret != 0) {
        LOG_ERROR("smem init failed, ret:" << ret << " rank:" << rankId);
        return;
    }
    smem_bm_config_t config;
    (void)smem_bm_config_init(&config);
    config.rankId = rankId;
    ret = smem_bm_init(ipPort.c_str(), rkSize, deviceId, &config);
    if (ret != 0) {
        LOG_ERROR("smem bm init failed, ret:" << ret << " rank:" << rankId);
        return;
    }

    void *gva = nullptr;
    smem_bm_t handle = smem_bm_create(0, 0, SMEMB_MEM_TYPE_HBM, SMEMB_DATA_OP_SDMA, GVA_SIZE, 0);
    if (handle == nullptr) {
        LOG_ERROR("smem_bm_create failed, rank:" << rankId);
        return;
    }

    ret = smem_bm_join(handle, 0, &gva);
    if (ret != 0) {
        LOG_ERROR("smem_bm_join failed, ret:" << ret << " rank:" << rankId);
        return;
    }
    LOG_INFO("smem_bm_create gva:" << gva << " rank:" << rankId);

    SetStatus(rankId, 1);
    WaitStatus(rankId, 2);
    // copy
    void *remote = smem_bm_ptr(handle, (rankId + 1) % rkSize);
    void *hostSrc = malloc(COPY_SIZE);
    void *hostDst = malloc(COPY_SIZE);
    if (hostSrc == nullptr || hostDst == nullptr) {
        LOG_ERROR("malloc host failed, rank:" << rankId);
        return;
    }
    void *deviceSrc = nullptr;
    ret = aclrtMalloc((void**)(&deviceSrc), COPY_SIZE, ACL_MEM_MALLOC_HUGE_FIRST);
    if (ret != 0 || deviceSrc == nullptr) {
        LOG_ERROR("malloc device failed, ret:" << ret << " rank:" << rankId);
        return;
    }

    GenerateData(hostSrc, rankId);
    ret = aclrtMemcpy(deviceSrc, COPY_SIZE, hostSrc, COPY_SIZE, ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != 0) {
        LOG_ERROR("copy host to device failed, ret:" << ret << " rank:" << rankId);
        return;
    }
    ret = smem_bm_copy(handle, hostSrc, remote, COPY_SIZE, SMEMB_COPY_H2G, 0);
    if (ret != 0) {
        LOG_ERROR("copy host to gva failed, ret:" << ret << " rank:" << rankId);
        return;
    }
    ret = smem_bm_copy(handle, deviceSrc, (void *)((uint64_t)remote + COPY_SIZE), COPY_SIZE, SMEMB_COPY_L2G, 0);
    if (ret != 0) {
        LOG_ERROR("copy hbm to gva failed, ret:" << ret << " rank:" << rankId);
        return;
    }
    SetStatus(rankId, 3);
    WaitStatus(rankId, 4);
    // check
    GenerateData(hostDst, (rankId + rkSize - 1) % rkSize);
    ret = smem_bm_copy(handle, gva, hostSrc, COPY_SIZE, SMEMB_COPY_G2H, 0);
    if (ret != 0) {
        LOG_ERROR("copy gva to host failed, ret:" << ret << " rank:" << rankId);
        return;
    }
    if (!CheckData(hostDst, hostSrc)) {
        LOG_ERROR("check G2H data failed, rank:" << rankId);
        return;
    }

    ret = smem_bm_copy(handle, (void *)((uint64_t)gva + COPY_SIZE), deviceSrc, COPY_SIZE, SMEMB_COPY_G2L, 0);
    if (ret != 0) {
        LOG_ERROR("copy hbm to gva failed, ret:" << ret << " rank:" << rankId);
        return;
    }
    ret = aclrtMemcpy(hostSrc, COPY_SIZE, deviceSrc, COPY_SIZE, ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != 0) {
        LOG_ERROR("copy device to host failed, ret:" << ret << " rank:" << rankId);
        return;
    }
    if (!CheckData(hostDst, hostSrc)) {
        LOG_ERROR("check G2L data failed, rank:" << rankId);
        return;
    }
    SetStatus(rankId, 5);
    WaitStatus(rankId, 6);
    // exit
    free(hostSrc);
    free(hostDst);
    aclrtFree(deviceSrc);
}

int main(int32_t argc, char* argv[])
{
    int rankSize = atoi(argv[1]);
    std::string ipport = argv[2];
    LOG_INFO("input rank_size: " << rankSize << " input_ip: " << ipport);

    if (rankSize != (rankSize & (~(rankSize - 1)))) {
        LOG_ERROR("input rank_size: " << rankSize << " is not the power of 2");
        return -1;
    }
    if (rankSize > RANK_SIZE_MAX) {
        LOG_ERROR("input rank_size: " << rankSize << " is large than " << RANK_SIZE_MAX);
        return -1;
    }

    int32_t fd = CreateShmMsg();
    for (int i = 0; i < rankSize; i++) shmAddr[i] = 0;

    pid_t pids[RANK_SIZE_MAX];
    for (int i = 0; i < rankSize; ++i) {
        pids[i] = fork();
        if (pids[i] < 0) {
            LOG_ERROR("fork failed ! " << pids[i]);
            exit(-1);
        } else if (pids[i] == 0) {
            // subprocess
            SubProcessRuning(i, rankSize, ipport);
            munmap(shmAddr, SHM_MSG_SIZE);
            exit(0);
        }
    }

    for (int i = 0; i < rankSize; i++) WaitStatus(i, 1); // 等待集群拉起
    LOG_INFO("all subprocess init ok.");
    for (int i = 0; i < rankSize; i++) SetStatus(i, 2); // 通知可以copy

    for (int i = 0; i < rankSize; i++) WaitStatus(i, 3); // 等待都copy完成
    LOG_INFO("all subprocess copy ok.");
    for (int i = 0; i < rankSize; i++) SetStatus(i, 4); // 通知可以check

    for (int i = 0; i < rankSize; i++) WaitStatus(i, 5); // 等待都check完成
    LOG_INFO("all subprocess check ok.");
    for (int i = 0; i < rankSize; i++) SetStatus(i, 6); // 通知可以退出

    munmap(shmAddr, SHM_MSG_SIZE);
    close(fd);
    return 0;
}