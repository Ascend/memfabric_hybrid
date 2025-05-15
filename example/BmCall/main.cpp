/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <iostream>
#include <unistd.h>
#include "acl/acl.h"
#include "smem.h"
#include "smem_bm.h"

#define LOG_INFO(msg) std::cout << __FILE__ << ":" << __LINE__ << "[INFO]" << msg << std::endl;
#define LOG_WARN(msg) std::cout << __FILE__ << ":" << __LINE__ << "[WARN]" << msg << std::endl;
#define LOG_ERROR(msg) std::cout << __FILE__ << ":" << __LINE__ << "[ERR]" << msg << std::endl;

const int32_t RANK_SIZE_MAX = 8;
const uint64_t GVA_SIZE = 1024ULL * 1024 * 1024;
int pipeFd[RANK_SIZE_MAX][2];

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
    while(true) {
        usleep(1000);
    }
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

    pid_t pids[RANK_SIZE_MAX];
    for (int i = 0; i < rankSize; ++i) {
        if (pipe(pipeFd[i]) == -1) {
            LOG_ERROR("create pipe failed ! " << pids[i]);
            exit(-1);
        }

        pids[i] = fork();
        if (pids[i] < 0) {
            LOG_ERROR("fork failed ! " << pids[i]);
            exit(-1);
        } else if (pids[i] == 0) {
            // 子进程
            close(pipeFd[i][1]); // close write
            SubProcessRuning(i, rankSize, ipport);
            exit(0);
        }
        close(pipeFd[i][0]); // close read
    }

    while(true) {
        usleep(1000);
    }
    return 0;
}