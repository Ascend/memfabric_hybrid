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
#include <string>
#include <cstdio>
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

uint64_t COPY_SIZE = 20ULL * 1024ULL * 1024ULL;
uint64_t COPY_COUNT = 2ULL;
uint32_t BATCH_SIZE = 1ULL;

constexpr int RANK_SIZE_ARG_INDEX = 1;
constexpr int RANK_NUM_ARG_INDEX = 2;
constexpr int RANK_START_ARG_INDEX = 3;
constexpr int IPPORT_ARG_INDEX = 4;
constexpr int COPY_SIZE_INDEX = 5;
constexpr int COPY_COUNT_INDEX = 6;
constexpr int BATCH_SIZE_INDEX = 7;

const int32_t RANK_SIZE_MAX = 16;

const uint64_t GVA_SIZE = 4 * 1024ULL * 1024ULL * 1024ULL;
const int32_t RANDOM_MULTIPLIER = 23;
const int32_t RANDOM_INCREMENT = 17;
const int32_t NEGATIVE_RATIO_DIVISOR = 3;
smem_bm_data_op_type OP_TYPE = SMEMB_DATA_OP_DEVICE_RDMA;

BarrierUtil *g_barrier = nullptr;


typedef struct {
    void** localAddrs;
    void** globalAddrs;
    uint64_t *dataSizes;
    uint32_t batchSize;
} BatchCopyParam;

static std::string ExecShellCmd(const char *cmd)
{
    std::string result;
    // 使用 popen 执行命令并以只读方式打开管道
    FILE *pipe = popen(cmd, "r");

    // 检查命令是否成功执行
    if (!pipe) {
        return "ERROR: popen failed.";
    }

    char buffer[1024];
    // 从管道中逐行读取命令输出
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer; // 将读取到的内容追加到结果字符串
    }

    // 关闭管道并获取命令的退出状态
    int status = pclose(pipe);
    if (status == -1) {
        result += "\nERROR: pclose failed.";
    }

    return result;
}

static inline uint64_t TimeNs()
{
#if defined(ENABLE_CPU_MONOTONIC) && defined(__aarch64__)
    const static uint64_t TICK_PER_US = InitTickUs();
    uint64_t timeValue = 0;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(timeValue));
    return timeValue * 1000ULL / TICK_PER_US;
#else
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000UL + static_cast<uint64_t>(ts.tv_nsec);
#endif
}

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

int PrepareLocalMem(smem_bm_mem_type localMemType, smem_bm_mem_type rmtMemType, uint16_t gvaRankId, smem_bm_t handle,
                    BatchCopyParam &param)
{
    int ret = 0;
    if (rmtMemType != SMEM_MEM_TYPE_DEVICE && rmtMemType != SMEM_MEM_TYPE_HOST) {
        LOG_ERROR("rmtMemType type error, localMemType:" << localMemType << " rmtMemType:" << rmtMemType);
        return -1;
    }
    uint64_t len = BATCH_SIZE * COPY_SIZE;
    void *ptr = nullptr;
    aclrtMemcpyKind kind = ACL_MEMCPY_HOST_TO_DEVICE;
    if (localMemType == SMEM_MEM_TYPE_LOCAL_DEVICE) {
        ret = aclrtMalloc(&ptr, len, ACL_MEM_MALLOC_HUGE_FIRST);
        CHECK_RET_ERR((ret != 0 || ptr == nullptr), "malloc device failed, ret:" << ret << " len:" << len);
        kind = ACL_MEMCPY_HOST_TO_DEVICE;
        if (OP_TYPE == SMEMB_DATA_OP_DEVICE_RDMA || OP_TYPE == SMEMB_DATA_OP_SDMA) {
            ret = smem_bm_register_user_mem(handle, (uint64_t)(uintptr_t)ptr, len);
            CHECK_RET_ERR((ret != 0 || ptr == nullptr),
                          "register failed, addr:" << ptr << " len:" << len << " localMemType:" << localMemType);
        }
    } else if (localMemType == SMEM_MEM_TYPE_LOCAL_HOST) {
        void *devPtr = nullptr;
        ret = aclrtMallocHost(&ptr, len);
        CHECK_RET_ERR((ret != 0 || ptr == nullptr), "mallpc host ptr failed, ret:" << ret << " len:" << len);
        kind = ACL_MEMCPY_HOST_TO_HOST;
        ret = aclrtHostRegister(ptr, len, ACL_HOST_REGISTER_MAPPED, &devPtr);
        CHECK_RET_ERR((ret != 0 || devPtr == nullptr), "mapped host to device failed, ret:" << ret << " len:" << len);
        if (OP_TYPE == SMEMB_DATA_OP_DEVICE_RDMA || OP_TYPE == SMEMB_DATA_OP_SDMA) {
            ret = smem_bm_register_user_mem(handle, (uint64_t)(uintptr_t)devPtr, len);
            CHECK_RET_ERR((ret != 0 || ptr == nullptr),
                          "register failed, addr:" << ptr << " len:" << len << " localMemType:" << localMemType);
        }
    } else {
        LOG_ERROR("rmtMemType type error, localMemType:" << localMemType << " rmtMemType:" << rmtMemType);
        return -1;
    }

    // check data
    void *hostSrc = malloc(COPY_SIZE);
    GenerateData(hostSrc, gvaRankId);

    uint64_t gva = (uint64_t)smem_bm_ptr_by_mem_type(handle, rmtMemType, gvaRankId);
    uint64_t offset = 0;
    for (uint64_t i = 0; i < param.batchSize; i++) {
        void *devPtr = reinterpret_cast<void *>((uintptr_t)ptr + i * COPY_SIZE);
        ret = aclrtMemcpy(devPtr, COPY_SIZE, hostSrc, COPY_SIZE, kind);
        CHECK_RET_ERR(ret, "copy host to device failed, ret:" << ret << " rank:" << gvaRankId);

        param.localAddrs[i] = devPtr;
        param.globalAddrs[i] = reinterpret_cast<void *>(gva + offset);
        param.dataSizes[i] = COPY_SIZE;
        offset += COPY_SIZE;
    }
    return 0;
}

inline std::string FormatString(std::string &name, uint64_t totalTimeUs, uint64_t copySize, uint64_t batchSize,
                                uint64_t copyCount)
{
    uint64_t totalKB = copySize * copyCount * batchSize / 1024;
    uint64_t avgTimeUs = totalTimeUs / copyCount;
    auto bandwidthGBps = totalKB * 1000000.0 / totalTimeUs / 1024 / 1024;

    int32_t DIGIT_WIDTH = 10;
    std::string str;
    std::ostringstream os(str);
    os.flags(std::ios::fixed);
    os.precision(3);
    os << std::left << std::setw(5) << name << std::left << std::setw(10) << "size(KB):" << std::left
       << std::setw(DIGIT_WIDTH) << totalKB << std::left << std::setw(15) << "totalTime(us):" << std::left
       << std::setw(DIGIT_WIDTH) << totalTimeUs << std::left << std::setw(13) << "avgTime(us):" << std::left
       << std::setw(DIGIT_WIDTH) << avgTimeUs << std::left << std::setw(10) << "BW(GB/s):" << std::left
       << std::setw(DIGIT_WIDTH) << bandwidthGBps << std::left;

    return os.str();
}

void BatchCopyPut(smem_bm_mem_type localMemType, smem_bm_mem_type rmtMemType, uint16_t gvaRankId, smem_bm_t handle,
                  std::string direction)
{
    void *localAddrs[BATCH_SIZE];
    void *globalAddrs[BATCH_SIZE];
    uint64_t sizeList[BATCH_SIZE];
    BatchCopyParam tmpParam = {(void **)localAddrs, globalAddrs, sizeList, BATCH_SIZE};
    int ret = PrepareLocalMem(localMemType, rmtMemType, gvaRankId, handle, tmpParam);
    CHECK_RET_VOID(ret, "prepare local failed, ret:" << ret << " rank:" << gvaRankId);

    smem_batch_copy_params param = {};
    param.sources = tmpParam.localAddrs;
    param.destinations = tmpParam.globalAddrs;
    param.dataSizes = tmpParam.dataSizes;
    param.batchSize = tmpParam.batchSize;

    smem_bm_copy_type copyType = SMEMB_COPY_L2G;
    if (localMemType == SMEM_MEM_TYPE_LOCAL_DEVICE) {
        copyType = SMEMB_COPY_L2G;
    } else if (localMemType == SMEM_MEM_TYPE_LOCAL_HOST) {
        copyType = SMEMB_COPY_H2G;
    } else {
        LOG_ERROR("rmtMemType type error, localMemType:" << localMemType << " rmtMemType:" << rmtMemType);
        return;
    }
    uint64_t startTime = TimeNs();
    for (uint64_t i = 0; i < COPY_COUNT; i++) {
        ret = smem_bm_copy_batch(handle, &param, copyType, 0);
        CHECK_RET_VOID(ret, "copy hbm to gva failed, ret:" << ret << " rank:" << gvaRankId);
    }
    uint64_t totalTimeUs = (TimeNs() - startTime) / 1000;
    LOG_INFO("=== " << FormatString(direction, totalTimeUs, COPY_SIZE, BATCH_SIZE, COPY_COUNT) << " ===");
}

void BatchCopyGet(smem_bm_mem_type localMemType, smem_bm_mem_type rmtMemType, uint16_t gvaRankId, smem_bm_t handle,
                  std::string direction)
{
    void *localAddrs[BATCH_SIZE];
    void *globalAddrs[BATCH_SIZE];
    uint64_t sizeList[BATCH_SIZE];
    BatchCopyParam tmpParam = {(void **)localAddrs, globalAddrs, sizeList, BATCH_SIZE};
    int ret = PrepareLocalMem(localMemType, rmtMemType, gvaRankId, handle, tmpParam);
    CHECK_RET_VOID(ret, "prepare local failed, ret:" << ret << " rank:" << gvaRankId);

    smem_batch_copy_params param = {};
    param.sources = tmpParam.globalAddrs;
    param.destinations = tmpParam.localAddrs;
    param.dataSizes = tmpParam.dataSizes;
    param.batchSize = tmpParam.batchSize;

    smem_bm_copy_type copyType = SMEMB_COPY_L2G;
    if (localMemType == SMEM_MEM_TYPE_LOCAL_DEVICE) {
        copyType = SMEMB_COPY_G2L;
    } else if (localMemType == SMEM_MEM_TYPE_LOCAL_HOST) {
        copyType = SMEMB_COPY_G2H;
    } else {
        LOG_ERROR("rmtMemType type error, localMemType:" << localMemType << " rmtMemType:" << rmtMemType);
        return;
    }
    uint64_t startTime = TimeNs();
    for (uint64_t i = 0; i < COPY_COUNT; i++) {
        ret = smem_bm_copy_batch(handle, &param, copyType, 0);
        CHECK_RET_VOID(ret, "copy hbm to gva failed, ret:" << ret << " rank:" << gvaRankId);
    }
    uint64_t totalTimeUs = (TimeNs() - startTime) / 1000;
    LOG_INFO("=== " << FormatString(direction, totalTimeUs, COPY_SIZE, BATCH_SIZE, COPY_COUNT) << " ===");
}

void SubProcessRuning(uint32_t deviceId, uint32_t rankId, uint32_t localRankNum, uint32_t rkSize, std::string ipPort)
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
    if (rankId >= localRankNum) {  // 只有一个节点进行读写
        g_barrier->Barrier();
        g_barrier->Barrier();
        g_barrier->Barrier();
        g_barrier->Barrier();
        g_barrier->Barrier();
        g_barrier->Barrier();
        g_barrier->Barrier();
        g_barrier->Barrier();
        g_barrier->Barrier();
        g_barrier->Barrier();
        smem_bm_destroy(handle);
        FinalizeAll(&stream, deviceId);
        return;
    }
    ////////////////////////////////////////////  PUT  ///////////////////////////////////////////////////////////
    // H2D
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_HOST, SMEM_MEM_TYPE_DEVICE, rankId, handle, "H2D");
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    // D2H
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_DEVICE, SMEM_MEM_TYPE_HOST, rankId, handle, "D2H");
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    // H2RD
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_HOST, SMEM_MEM_TYPE_DEVICE, (rankId + localRankNum) % rkSize, handle, "H2RD");
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    // H2RH
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_HOST, SMEM_MEM_TYPE_HOST, (rankId + localRankNum) % rkSize, handle, "H2RH");
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    // D2RH
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_DEVICE, SMEM_MEM_TYPE_HOST, (rankId + localRankNum) % rkSize, handle, "D2RH");
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    // D2RD
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_DEVICE, SMEM_MEM_TYPE_DEVICE, (rankId + localRankNum) % rkSize, handle, "D2RD");
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    ////////////////////////////////////////////  GET  ///////////////////////////////////////////////////////////
    // RH2D
    BatchCopyGet(SMEM_MEM_TYPE_LOCAL_DEVICE, SMEM_MEM_TYPE_HOST, (rankId + localRankNum) % rkSize, handle, "RH2D");
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    // RH2H
    BatchCopyGet(SMEM_MEM_TYPE_LOCAL_HOST, SMEM_MEM_TYPE_HOST, (rankId + localRankNum) % rkSize, handle, "RH2H");
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    // RD2D
    BatchCopyGet(SMEM_MEM_TYPE_LOCAL_DEVICE, SMEM_MEM_TYPE_DEVICE, (rankId + localRankNum) % rkSize, handle, "RD2D");
    ret = g_barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    // RD2H
    BatchCopyGet(SMEM_MEM_TYPE_LOCAL_HOST, SMEM_MEM_TYPE_DEVICE, (rankId + localRankNum) % rkSize, handle, "RD2H");
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
    if (argc > COPY_SIZE_INDEX) {
        COPY_SIZE = atoi(argv[COPY_SIZE_INDEX]);
    }
    if (argc > COPY_COUNT_INDEX) {
        COPY_COUNT = atoi(argv[COPY_COUNT_INDEX]);
    }
    if (argc > BATCH_SIZE_INDEX) {
        BATCH_SIZE = atoi(argv[BATCH_SIZE_INDEX]);
    }

    LOG_INFO("input rank_size:" << rankSize << " local_size:" << rankNum << " rank_offset:" << rankStart
                                << " input_ip:" << ipport << " copy_size:" << COPY_SIZE << " copy_count:" << COPY_COUNT
                                << " batch_size:" << BATCH_SIZE);

    if (rankSize != (rankSize & (~(rankSize - 1)))) {
        LOG_ERROR("input rank_size: " << rankSize << " is not the power of 2");
        return -1;
    }
    if (rankSize > RANK_SIZE_MAX) {
        LOG_ERROR("input rank_size: " << rankSize << " is large than " << RANK_SIZE_MAX);
        return -1;
    }

    // 检查硬件环境
    std::string productName = ExecShellCmd("dmidecode | grep 'Product Name'");
    if (productName.find("A3") != std::string::npos) {
        OP_TYPE = SMEMB_DATA_OP_SDMA;
    } else if (productName.find("A2") != std::string::npos) {
        OP_TYPE = SMEMB_DATA_OP_DEVICE_RDMA;
    } else {
        LOG_ERROR("unexcept machine, productName: " << productName);
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
            SubProcessRuning(i, i + rankStart, rankNum, rankSize, ipport);
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
