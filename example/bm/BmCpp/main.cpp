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
const int32_t COPY_SIZE = 1024 * 1024 * 1024;
const uint64_t GVA_SIZE = 4 * 1024ULL * 1024 * 1024;
const int32_t RANDOM_MULTIPLIER = 23;
const int32_t RANDOM_INCREMENT = 17;
const int32_t NEGATIVE_RATIO_DIVISOR = 3;
smem_bm_data_op_type OP_TYPE = SMEMB_DATA_OP_DEVICE_RDMA;

BarrierUtil *g_barrier = nullptr;
constexpr int RANK_SIZE_ARG_INDEX = 1;
constexpr int RANK_NUM_ARG_INDEX = 2;
constexpr int RANK_START_ARG_INDEX = 3;
constexpr int IPPORT_ARG_INDEX = 4;
constexpr int TRANSPORT_ARG_INDEX = 5;

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

void BigCopy(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, smem_bm_t handle)
{
    void *remote_host = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_HOST, (rankId + 1) % rkSize);
    void *local_host = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_HOST, rankId);
    void *remote_dev = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_DEVICE, (rankId + 1) % rkSize);
    void *local_dev = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_DEVICE, rankId);

    LOG_INFO(" ==================== [TEST] init, rank:" << rankId << " ptr:" << local_host << " " << local_dev << " " << remote_host << " " << remote_dev);

    local_dev = (void *)(reinterpret_cast<uint64_t>(local_dev) + COPY_SIZE);
    void *base = malloc(COPY_SIZE);
    CHECK_RET_VOID(base == nullptr, "alloc host mem failed");
    GenerateData(base, rankId, COPY_SIZE);
    smem_copy_params params1 = {base, local_dev, COPY_SIZE};
    int ret = smem_bm_copy(handle, &params1, SMEMB_COPY_H2G, 0);
    CHECK_RET_VOID(ret, "copy local to global failed, ret:" << ret << " rank:" << rankId <<
        " ptr:" << base << " " << local_dev);
    free(base);

    smem_copy_params params2 = {local_dev, remote_dev, COPY_SIZE};
    ret = smem_bm_copy(handle, &params2, SMEMB_COPY_G2G, 0);
    CHECK_RET_VOID(ret, "copy hbm to remote hbm failed, ret:" << ret << " rank:" << rankId <<
        " ptr:" << local_dev << " " << remote_dev);

    smem_copy_params params3 = {local_dev, remote_host, COPY_SIZE};
    ret = smem_bm_copy(handle, &params3, SMEMB_COPY_G2G, 0);
    CHECK_RET_VOID(ret, "copy hbm to remote dram failed, ret:" << ret << " rank:" << rankId <<
                            " ptr:" << local_dev << " " << remote_host);
    LOG_INFO(" ==================== [TEST] bm copy ok, rank:" << rankId << " size:" << COPY_SIZE);
}

void BigCopyCheck(uint32_t deviceId, uint32_t rankId, uint32_t rkSize, smem_bm_t handle)
{
    void *base = malloc(COPY_SIZE);
    void *receive = malloc(COPY_SIZE);
    CHECK_RET_VOID(base == nullptr || receive == nullptr, "alloc host mem failed");
    GenerateData(base, (rankId + rkSize - 1) % rkSize, COPY_SIZE);

    void *local_host = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_HOST, rankId);
    void *local_dev = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_DEVICE, rankId);

    int ret;
    smem_copy_params params1 = {local_dev, receive, COPY_SIZE};
    ret = smem_bm_copy(handle, &params1, SMEMB_COPY_G2H, 0);
    CHECK_RET_VOID(ret, "copy hbm to local host failed, ret:" << ret << " rank:" << rankId <<
        " ptr:" << local_dev << " " << receive);
    ret = CheckData(base, receive, COPY_SIZE);
    CHECK_RET_VOID((ret == false), "check hbm data failed, rank:" << rankId);

    if (OP_TYPE == SMEMB_DATA_OP_SDMA) {
        smem_copy_params params2 = {local_host, local_dev, COPY_SIZE};
        ret = smem_bm_copy(handle, &params2, SMEMB_COPY_G2G, 0);
        CHECK_RET_VOID(ret, "copy hbm to host failed, ret:" << ret << " rank:" << rankId << " ptr:" << local_host
            << " " << local_dev);

        ret = smem_bm_copy(handle, &params1, SMEMB_COPY_G2H, 0);
        CHECK_RET_VOID(ret, "copy hbm to local host failed, ret:" << ret << " rank:" << rankId <<
            " ptr:" << local_dev << " " << receive);

        ret = CheckData(base, receive, COPY_SIZE);
        CHECK_RET_VOID((ret == false), "check host data failed, rank:" << rankId);
    } else {
        ret = CheckData(base, local_host, COPY_SIZE);
        CHECK_RET_VOID((ret == false), "check host data failed, rank:" << rankId);
    }

    LOG_INFO(" ==================== [TEST] bm check ok, rank:" << rankId);
    free(base);
    free(receive);
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

    BigCopy(deviceId, rankId, rkSize, handle);
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);

    BigCopyCheck(deviceId, rankId, rkSize, handle);
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);

    smem_bm_destroy(handle);
    FinalizeAll(&stream, deviceId);
}

int main(int32_t argc, char* argv[])
{
    if (argc <= TRANSPORT_ARG_INDEX) {
        LOG_ERROR("input param is invalid!");
        return -1;
    }

    int rankSize = atoi(argv[RANK_SIZE_ARG_INDEX]);
    int rankNum = atoi(argv[RANK_NUM_ARG_INDEX]);
    int rankStart = atoi(argv[RANK_START_ARG_INDEX]);
    std::string ipport = argv[IPPORT_ARG_INDEX];
    int op = atoi(argv[TRANSPORT_ARG_INDEX]);

    if (op == 0) OP_TYPE = SMEMB_DATA_OP_SDMA;
    else OP_TYPE = SMEMB_DATA_OP_DEVICE_RDMA;

    LOG_INFO("input rank_size:" << rankSize << " local_size:" << rankNum << " rank_offset:" << rankStart <<
        " input_ip:" << ipport << " transport:" << (OP_TYPE == SMEMB_DATA_OP_SDMA ? "SDMA" : "RDMA"));

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