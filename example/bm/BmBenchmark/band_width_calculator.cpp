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
#include <algorithm>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <bits/stdc++.h>
#include <string>
#include <cstdio>
#include "band_width_calculator.h"

const uint64_t GVA_SIZE = 4 * 1024ULL * 1024ULL * 1024ULL;
const int32_t RANDOM_MULTIPLIER = 23;
const int32_t RANDOM_INCREMENT = 17;
const int32_t NEGATIVE_RATIO_DIVISOR = 3;
const uint16_t BARRIOR_PORT = 21666U;
constexpr int32_t DIRECTION_TYPE_NUM_MAX = static_cast<int32_t>(CopyType::ALL_DIRECTION);

using OptFunc = void (BandWidthCalculator::*)(BarrierUtil *, smem_bm_t, uint32_t, uint32_t, BwTestResult *);

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

void GenerateData(void *ptr, int32_t rank, uint32_t len)
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

int64_t CheckData(void *base, void *ptr, uint32_t len)
{
    int64_t wrongNum = 0;
    if (base == nullptr || ptr == nullptr) {
        return wrongNum;
    }
    int32_t *arr1 = (int32_t *)base;
    int32_t *arr2 = (int32_t *)ptr;
    for (uint32_t i = 0; i < len / sizeof(int); i++) {
        if (arr1[i] != arr2[i]) {
            wrongNum++;
        }
    }
    return wrongNum;
}

std::string ReplacePort(const std::string &input, uint16_t newPort)
{
    size_t colonPos = input.rfind(':');
    if (colonPos == std::string::npos) {
        return input;
    }
    std::string newPortStr = std::to_string(newPort);
    std::string result = input.substr(0, colonPos + 1) + newPortStr;

    return result;
}

std::string CopyType2Str(CopyType type)
{
    switch (type) {
        case CopyType::HOST_TO_DEVICE:
            return "H2D";
        case CopyType::DEVICE_TO_HOST:
            return "D2H";
        case CopyType::HOST_TO_REMOTE_DEVICE:
            return "H2RD";
        case CopyType::HOST_TO_REMOTE_HOST:
            return "H2RH";
        case CopyType::DEVICE_TO_REMOTE_DEVICE:
            return "D2RD";
        case CopyType::DEVICE_TO_REMOTE_HOST:
            return "D2RH";
        case CopyType::REMOTE_HOST_TO_DEVICE:
            return "RH2D";
        case CopyType::REMOTE_HOST_TO_HOST:
            return "RH2H";
        case CopyType::REMOTE_DEVICE_TO_DEVICE:
            return "RD2D";
        case CopyType::REMOTE_DEVICE_TO_HOST:
            return "RD2H";
        default:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

uint32_t GenRmtRankId(uint32_t rankId, uint32_t localRankSize, uint32_t worldSize)
{
    if (localRankSize == worldSize) {
        return (rankId + 1) % worldSize;
    } else {
        return (rankId + localRankSize) % worldSize;
    }
}

BandWidthCalculator::BandWidthCalculator(BwTestParam &param)
{
    cmdParam_ = param;
    if (param.localRankStart == 0) {
        isMaster_ = true;
    }
}

int32_t BandWidthCalculator::MultiProcessExecute()
{
    pid_t pids[RANK_SIZE_MAX];
    int32_t pipes[RANK_SIZE_MAX][2];
    for (uint32_t i = 0; i < cmdParam_.localRankSize; ++i) {
        if (pipe(pipes[i]) == -1) {
            LOG_ERROR("pipe failed ! " << i);
            return -1;
        }
        pids[i] = fork();
        if (pids[i] < 0) {
            LOG_ERROR("fork failed ! " << pids[i]);
            return -1;
        } else if (pids[i] == 0) {
            // subprocess
            LOG_INFO("subprocess (" << i << ") start.");
            close(pipes[i][0]);
            auto ret = Execute(i + cmdParam_.deviceId, i + cmdParam_.localRankStart, cmdParam_.localRankSize,
                               cmdParam_.worldRankSize, pipes[i][1]);
            LOG_INFO("subprocess (" << i << ") exited.");
            if (ret != 0) {
                std::exit(1);
            }
            std::exit(0);
        } else {
            // main process
            close(pipes[i][1]);
        }
    }

    int status[RANK_SIZE_MAX];
    for (uint32_t i = 0; i < cmdParam_.localRankSize; ++i) {
        waitpid(pids[i], &status[i], 0);
        if (WIFEXITED(status[i]) && WEXITSTATUS(status[i]) != 0) {
            LOG_INFO("subprocess exit error: " << i);
        } else if (WIFSIGNALED(status[i])) {
            LOG_INFO("subprocess (" << i << ") terminated by signal: " << WTERMSIG(status[i]));
            if (WCOREDUMP(status[i])) {
                LOG_INFO("subprocess (" << i << ") dumped core.");
            }
        }
    }

    std::vector<BwTestResult> resultVec;
    for (uint32_t i = 0; i < cmdParam_.localRankSize; ++i) {
        BwTestResult results[DIRECTION_TYPE_NUM_MAX];
        auto len = sizeof(BwTestResult) * DIRECTION_TYPE_NUM_MAX;
        auto n = read(pipes[i][0], &results, len);
        if (n == 0) {
            LOG_WARN("pipe " << i << " no data received");
        } else if (n < 0) {
            LOG_ERROR("pipe " << i << " read failed, errno:" << errno);
        } else if (n < len) {
            LOG_WARN("pipe " << i << " partial data received: " << n << " bytes");
        } else {
            for (int32_t j = 0; j < DIRECTION_TYPE_NUM_MAX; ++j) {
                if (results[j].flag < 0) {
                    continue;
                }
                resultVec.push_back(results[j]);
            }
        }
        close(pipes[i][0]);
    }
    PrintResult(resultVec);
    return 0;
}

int32_t BandWidthCalculator::MultiThreadExecute()
{
    return 0;
}

int32_t BandWidthCalculator::PreInit(uint32_t deviceId, BarrierUtil *&barrier, uint32_t rankId, uint32_t rkSize,
                                     aclrtStream *stream)
{
    CHECK_RET_ERR(barrier != nullptr, "barrier is not nullptr, rank:" << rankId);
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
    std::copy_n(cmdParam_.rdmaIpPort.c_str(), cmdParam_.rdmaIpPort.size(), config.hcomUrl);
    config.autoRanking = false;
    config.rankId = rankId;
    if (rankId != 0) {
        config.startConfigStoreServer = false;
    }
    ret = smem_bm_init(cmdParam_.ipPort.c_str(), rkSize, deviceId, &config);
    CHECK_RET_ERR(ret, "smem bm init failed, ret:" << ret << " rank:" << rankId);

    barrier = new (std::nothrow) BarrierUtil;
    CHECK_RET_ERR((barrier == nullptr), "malloc failed, rank:" << rankId);
    ret = barrier->Init(deviceId, rankId, rkSize, ReplacePort(cmdParam_.ipPort, BARRIOR_PORT));
    CHECK_RET_ERR(ret, "barrier init failed, rank:" << rankId << " ret:" << ret << " " << errno);

    *stream = ss;
    LOG_INFO(" ==================== [TEST] init, rank:" << rankId << " devId:" << deviceId);
    return 0;
}

void BandWidthCalculator::FinalizeAll(uint32_t deviceId, BarrierUtil *&barrier, aclrtStream *stream)
{
    if (barrier != nullptr) {
        delete barrier;
        barrier = nullptr;
    }
    smem_bm_uninit(0);
    if (stream != nullptr) {
        aclrtDestroyStream(stream);
    }
    aclrtResetDevice(deviceId);
    aclFinalize();
}

int32_t BandWidthCalculator::PrepareLocalMem(smem_bm_t handle, uint32_t rankId)
{
    uint64_t len = cmdParam_.batchSize * cmdParam_.copySize;
    void *dataPtr = malloc(len);
    CHECK_RET_ERR((dataPtr == nullptr), "malloc data dram failed, len:" << len);
    GenerateData(dataPtr, static_cast<int32_t>(rankId), len);
    if (localDram_ == nullptr) {
        auto ret = aclrtMallocHost(&localDram_, len);
        CHECK_RET_ERR((ret != 0 || localDram_ == nullptr), "malloc dram failed, len:" << len);
        ret = aclrtMemcpy(localDram_, len, dataPtr, len, ACL_MEMCPY_HOST_TO_HOST);
        CHECK_RET_ERR((ret != 0), "memcpy data dram failed, len:" << len);
        void *tmpHostPtr = nullptr;
        ret = aclrtHostRegister(localDram_, len, ACL_HOST_REGISTER_MAPPED, &tmpHostPtr);
        if (ret != 0) {
            LOG_WARN("register host dram failed, ret:" << ret << ", len:" << len
                                                       << ", addr:" << reinterpret_cast<uint64_t>(localDram_));
        } else {
            ret = smem_bm_register_user_mem(handle, reinterpret_cast<uint64_t>(tmpHostPtr), len);
            if (ret != 0) {
                LOG_WARN("register hbm failed, ret:" << ret << ", len:" << len << ", addr:" << tmpHostPtr);
            } else {
                registedLocalDram_ = tmpHostPtr;
            }
        }
    }
    if (localHbm_ == nullptr) {
        auto ret = aclrtMalloc(&localHbm_, len, ACL_MEM_MALLOC_HUGE_FIRST);
        CHECK_RET_ERR((ret != 0 || localHbm_ == nullptr), "malloc hbm failed, ret:" << ret << " len:" << len);
        ret = aclrtMemcpy(localHbm_, len, dataPtr, len, ACL_MEMCPY_HOST_TO_DEVICE);
        CHECK_RET_ERR((ret != 0), "memcpy data dram failed, len:" << len);
        ret = smem_bm_register_user_mem(handle, reinterpret_cast<uint64_t>(localHbm_), len);
        if (ret != 0) {
            LOG_WARN("register hbm failed, ret:" << ret << ", len:" << len
                                                 << ", addr:" << reinterpret_cast<uint64_t>(localHbm_));
        } else {
            registedLocalHbm_ = localHbm_;
        }
    }
    if (cmdParam_.opType == SMEMB_DATA_OP_SDMA) {
        auto gva = smem_bm_ptr_by_mem_type(handle, SMEM_MEM_TYPE_HOST, rankId);
        CHECK_RET_ERR((len > GVA_SIZE || gva == nullptr || dataPtr == nullptr), "check memcpy param failed.");
        memcpy(gva, dataPtr, len);
        CHECK_RET_ERR((memcmp(gva, dataPtr, len) != 0), "check data failed, len:" << len);
    }
    free(dataPtr);
    return 0;
}

int32_t BandWidthCalculator::PrepareCopyParam(smem_bm_mem_type localMemType, smem_bm_mem_type rmtMemType,
                                              uint32_t gvaRankId, smem_bm_t handle, BatchCopyParam &param)
{
    if (rmtMemType != SMEM_MEM_TYPE_DEVICE && rmtMemType != SMEM_MEM_TYPE_HOST) {
        LOG_ERROR("rmtMemType type error, localMemType:" << localMemType << " rmtMemType:" << rmtMemType);
        return -1;
    }
    void *localPtr = nullptr;
    if (localMemType == SMEM_MEM_TYPE_LOCAL_DEVICE) {
        localPtr = registedLocalHbm_ == nullptr ? localHbm_ : registedLocalHbm_;
    } else if (localMemType == SMEM_MEM_TYPE_LOCAL_HOST) {
        localPtr = registedLocalDram_ == nullptr ? localDram_ : registedLocalDram_;
    }
    CHECK_RET_ERR((localPtr == nullptr), "localPtr is nullptr, localMemType:" << localMemType);
    uint64_t gva = (uint64_t)smem_bm_ptr_by_mem_type(handle, rmtMemType, gvaRankId);
    uint64_t offset = 0;
    for (uint32_t i = 0; i < param.batchSize; i++) {
        param.localAddrs[i] = reinterpret_cast<void *>((uintptr_t)localPtr + i * cmdParam_.copySize);
        param.globalAddrs[i] = reinterpret_cast<void *>(gva + offset);
        param.dataSizes[i] = cmdParam_.copySize;
        offset += cmdParam_.copySize;
    }
    return 0;
}

int64_t BandWidthCalculator::CheckCopyResult(smem_bm_mem_type localMemType, smem_bm_mem_type rmtMemType,
                                             uint32_t gvaRankId, smem_bm_t handle)
{
    if (rmtMemType != SMEM_MEM_TYPE_DEVICE && rmtMemType != SMEM_MEM_TYPE_HOST) {
        LOG_ERROR("rmtMemType type error, localMemType:" << localMemType << " rmtMemType:" << rmtMemType);
        return -1;
    }
    int ret = 0;
    uint64_t len = cmdParam_.copySize * cmdParam_.batchSize;
    void *localPtr = malloc(len);
    CHECK_RET_ERR((localPtr == nullptr), "malloc failed, len:" << len);
    if (localMemType == SMEM_MEM_TYPE_LOCAL_DEVICE) {
        ret = aclrtMemcpy(localPtr, len, localHbm_, len, ACL_MEMCPY_DEVICE_TO_HOST);
    } else if (localMemType == SMEM_MEM_TYPE_LOCAL_HOST) {
        ret = aclrtMemcpy(localPtr, len, localDram_, len, ACL_MEMCPY_HOST_TO_HOST);
    }
    CHECK_RET_ERR((ret != 0), "memcpy failed, ret:" << ret);

    void *rmtPtr = malloc(len);
    CHECK_RET_ERR((rmtPtr == nullptr), "malloc failed, len:" << len);
    void *gva = smem_bm_ptr_by_mem_type(handle, rmtMemType, gvaRankId);
    smem_copy_params params{};
    params.src = gva;
    params.dest = rmtPtr;
    params.dataSize = len;
    ret = smem_bm_copy(handle, &params, SMEMB_COPY_G2H, 0);
    CHECK_RET_ERR((ret != 0), "g2h copy failed, ret:" << ret);
    auto wrongNum = CheckData(localPtr, rmtPtr, len);
    if (wrongNum != 0) {
        LOG_ERROR("check data failed, localMemType:" << localMemType << " rmtMemType:" << rmtMemType);
    }
    return wrongNum;
}

void BandWidthCalculator::PrintResult(std::vector<BwTestResult> &results)
{
    if (results.empty()) {
        LOG_INFO("No bandwidth test results available.");
        return;
    }

    std::vector<BwTestResult> sortedResults = results;
    std::sort(sortedResults.begin(), sortedResults.end(), [](const BwTestResult &a, const BwTestResult &b) {
        if (a.flag != b.flag) {
            return a.flag < b.flag;
        }
        return a.devId < b.devId;
    });

    std::cout << std::endl
              << "  copy_size:" << results[0].copySize << "  copy_count:" << results[0].copyCount
              << "  batch_size:" << results[0].batchSize << std::endl;
    std::cout << "----------------------------------------------------------"
                 "Band Width Test----------------------------------------------------------\n";
    int32_t DIGIT_WIDTH = 10;
    std::cout << std::left << std::setw(DIGIT_WIDTH) << "Type" << std::setw(DIGIT_WIDTH) << "NPU"
              << std::setw(DIGIT_WIDTH) << "Rank" << std::setw(20) << "TotalSize(KB)" << std::setw(20)
              << "TotalTime(us)" << std::setw(20) << "AvgTime(us)" << std::setw(20) << "BW(GB/s)"
              << std::setw(DIGIT_WIDTH) << "WrongBytes(B)" << std::endl;

    for (const auto &r : sortedResults) {
        if (r.flag < 0) {
            continue;
        }
        uint64_t totalKB = r.copySize * r.copyCount * r.batchSize / 1024;
        uint64_t avgTimeUs = r.totalTimeUs / r.copyCount;
        auto bandwidthGBps = totalKB * 1024.0 / r.totalTimeUs / 1000.0;

        std::cout << std::left << std::setw(DIGIT_WIDTH) << r.typeName << std::setw(DIGIT_WIDTH) << r.devId
                  << std::setw(DIGIT_WIDTH) << r.rankId << std::setw(20) << totalKB << std::setw(20) << r.totalTimeUs
                  << std::setw(20) << std::fixed << std::setprecision(0) << avgTimeUs << std::setw(20) << std::fixed
                  << std::setprecision(3) << bandwidthGBps << std::setw(DIGIT_WIDTH) << r.wrongNum << std::endl;
    }
    std::cout << std::endl;
}

void BandWidthCalculator::SendResult(BwTestResult *results, int32_t pipeFdWrite)
{
    if (pipeFdWrite == 0) {
        return;
    }
    auto len = sizeof(BwTestResult) * DIRECTION_TYPE_NUM_MAX;
    auto ret = write(pipeFdWrite, results, len);
    if (ret != len) {
        LOG_ERROR("pipe write failed, errno:" << errno);
    }
    close(pipeFdWrite);
}

int32_t BandWidthCalculator::Execute(uint32_t deviceId, uint32_t rankId, uint32_t localRankNum, uint32_t rkSize,
                                     int32_t pipeFdWrite)
{
    aclrtStream stream;
    BarrierUtil *barrier = nullptr;
    auto ret = PreInit(deviceId, barrier, rankId, rkSize, &stream);
    CHECK_RET_ERR(ret, "pre init failed, ret:" << ret << " rank:" << rankId);

    smem_bm_t handle = smem_bm_create(0, 0, cmdParam_.opType, GVA_SIZE, GVA_SIZE, 0);
    CHECK_RET_ERR((handle == nullptr), "smem_bm_create failed, rank:" << rankId);

    ret = smem_bm_join(handle, 0);
    CHECK_RET_ERR(ret, "smem_bm_join failed, ret:" << ret << " rank:" << rankId);
    LOG_INFO("smem_bm_join, rank:" << rankId);

    ret = barrier->Barrier();
    CHECK_RET_ERR(ret, "barrier failed after init, ret:" << ret << " rank:" << rankId);
    ret = PrepareLocalMem(handle, rankId);
    CHECK_RET_ERR(ret, "prepare local mem failed, ret:" << ret << " rank:" << rankId);
    ret = barrier->Barrier();
    CHECK_RET_ERR(ret, "barrier failed after prepare local mem, ret:" << ret << " rank:" << rankId);
    LOG_INFO(" ==================== [TEST] bm init ok, rank:" << rankId);

    BwTestResult testResults[DIRECTION_TYPE_NUM_MAX];
    for (auto i = 0; i < DIRECTION_TYPE_NUM_MAX; ++i) {
        testResults[i].flag = -1;
        testResults[i].devId = deviceId;
        testResults[i].rankId = rankId;
    }
    ret = BandWidthCalculation(barrier, handle, rankId,
                               GenRmtRankId(rankId, localRankNum, cmdParam_.worldRankSize), testResults);
    if (ret == 0) {
        SendResult(testResults, pipeFdWrite);
    }
    smem_bm_destroy(handle);
    FinalizeAll(deviceId, barrier, &stream);
    return 0;
}

int32_t BandWidthCalculator::BandWidthCalculation(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId,
                                                  uint32_t remoteRankId, BwTestResult *results)
{
    static const OptFunc OPT_FUNC_MAP[DIRECTION_TYPE_NUM_MAX] = {
        &BandWidthCalculator::H2D,
        &BandWidthCalculator::D2H,
        &BandWidthCalculator::H2RD,
        &BandWidthCalculator::H2RH,
        &BandWidthCalculator::D2RD,
        &BandWidthCalculator::D2RH,
        &BandWidthCalculator::RH2D,
        &BandWidthCalculator::RH2H,
        &BandWidthCalculator::RD2D,
        &BandWidthCalculator::RD2H,
    };
    if (static_cast<int32_t>(cmdParam_.type) > DIRECTION_TYPE_NUM_MAX || static_cast<int32_t>(cmdParam_.type) < 0) {
        LOG_ERROR("copy type error" << static_cast<int32_t>(cmdParam_.type));
        return -1;
    }
    if (cmdParam_.type != CopyType::ALL_DIRECTION) {
        if (!isMaster_) {
            barrier->Barrier();
        }
        (this->*OPT_FUNC_MAP[static_cast<int32_t>(cmdParam_.type)])(barrier, handle, rankId, remoteRankId, results);
        if (isMaster_) {
            barrier->Barrier();
        }
        return 0;
    }
    for (auto i = 0; i < DIRECTION_TYPE_NUM_MAX; ++i) {
        if (!isMaster_) {
            barrier->Barrier();
        }
        (this->*OPT_FUNC_MAP[i])(barrier, handle, rankId, remoteRankId, results);
        if (isMaster_) {
            barrier->Barrier();
        }
    }
    return 0;
}

void BandWidthCalculator::BatchCopyPut(smem_bm_mem_type localMemType, smem_bm_mem_type rmtMemType, uint32_t gvaRankId,
                                       smem_bm_t handle, CopyType type, BwTestResult &result)
{
    void *localAddrs[cmdParam_.batchSize];
    void *globalAddrs[cmdParam_.batchSize];
    uint64_t sizeList[cmdParam_.batchSize];
    BatchCopyParam tmpParam = {(void **)localAddrs, globalAddrs, sizeList, static_cast<uint32_t>(cmdParam_.batchSize)};
    int ret = PrepareCopyParam(localMemType, rmtMemType, gvaRankId, handle, tmpParam);
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
    for (uint64_t i = 0; i < cmdParam_.copyCount; i++) {
        ret = smem_bm_copy_batch(handle, &param, copyType, 0);
        CHECK_RET_VOID(ret, "copy hbm to gva failed, ret:" << ret << " rank:" << gvaRankId);
    }
    uint64_t totalTimeUs = (TimeNs() - startTime) / 1000;
    auto direction = CopyType2Str(type);
    result.wrongNum = CheckCopyResult(localMemType, rmtMemType, gvaRankId, handle);
    if (result.wrongNum == -1) {
        LOG_ERROR(direction << " check data failed, rank:" << gvaRankId);
        return;
    } else if (result.wrongNum != 0) {
        LOG_WARN(direction << " check data wrong num:" << result.wrongNum << ", rank:" << gvaRankId);
    }
    result.totalTimeUs = totalTimeUs;
    result.copySize = cmdParam_.copySize;
    result.batchSize = cmdParam_.batchSize;
    result.copyCount = cmdParam_.copyCount;
    result.flag = static_cast<int32_t>(type);
    std::fill_n(result.typeName, sizeof(result.typeName), 0);
    std::copy_n(direction.begin(), std::min(direction.size(), sizeof(result.typeName) - 1), result.typeName);
    LOG_INFO(direction << " finished. rank: " << gvaRankId << ", flag:" << result.flag);
}

void BandWidthCalculator::BatchCopyGet(smem_bm_mem_type localMemType, smem_bm_mem_type rmtMemType, uint32_t gvaRankId,
                                       smem_bm_t handle, CopyType type, BwTestResult &result)
{
    void *localAddrs[cmdParam_.batchSize];
    void *globalAddrs[cmdParam_.batchSize];
    uint64_t sizeList[cmdParam_.batchSize];
    BatchCopyParam tmpParam = {(void **)localAddrs, globalAddrs, sizeList, static_cast<uint32_t>(cmdParam_.batchSize)};
    int ret = PrepareCopyParam(localMemType, rmtMemType, gvaRankId, handle, tmpParam);
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
    for (uint64_t i = 0; i < cmdParam_.copyCount; i++) {
        ret = smem_bm_copy_batch(handle, &param, copyType, 0);
        CHECK_RET_VOID(ret, "copy hbm to gva failed, ret:" << ret << " rank:" << gvaRankId);
    }
    uint64_t totalTimeUs = (TimeNs() - startTime) / 1000;
    auto direction = CopyType2Str(type);
    result.wrongNum = CheckCopyResult(localMemType, rmtMemType, gvaRankId, handle);
    if (result.wrongNum == -1) {
        LOG_ERROR(direction << " check data failed, rank:" << gvaRankId);
        return;
    } else if (result.wrongNum != 0) {
        LOG_ERROR(direction << " check data wrong num:" << result.wrongNum << ", rank:" << gvaRankId);
    }
    result.totalTimeUs = totalTimeUs;
    result.copySize = cmdParam_.copySize;
    result.batchSize = cmdParam_.batchSize;
    result.copyCount = cmdParam_.copyCount;
    result.flag = static_cast<int32_t>(type);
    std::fill_n(result.typeName, sizeof(result.typeName), 0);
    std::copy_n(direction.begin(), std::min(direction.size(), sizeof(result.typeName) - 1), result.typeName);
    LOG_INFO(direction << " finished. rank: " << gvaRankId << ", flag:" << result.flag);
}

void BandWidthCalculator::H2D(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId,
                              uint32_t remoteRankId, BwTestResult *results)
{
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_HOST, SMEM_MEM_TYPE_DEVICE, rankId, handle, CopyType::HOST_TO_DEVICE,
                 results[static_cast<int32_t>(CopyType::HOST_TO_DEVICE)]);
    auto ret = barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after H2D, ret:" << ret << " rank:" << rankId);
}

void BandWidthCalculator::D2H(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId,
                              uint32_t remoteRankId, BwTestResult *results)
{
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_DEVICE, SMEM_MEM_TYPE_HOST, rankId, handle, CopyType::DEVICE_TO_HOST,
                 results[static_cast<int32_t>(CopyType::DEVICE_TO_HOST)]);
    auto ret = barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after D2H, ret:" << ret << " rank:" << rankId);
}

void BandWidthCalculator::H2RD(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId,
                               uint32_t remoteRankId, BwTestResult *results)
{
    if (cmdParam_.opType == SMEMB_DATA_OP_HOST_RDMA) {
        LOG_WARN("H2RD not support in HOST_RDMA mode");
        barrier->Barrier();
        return;
    }
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_HOST, SMEM_MEM_TYPE_DEVICE, remoteRankId, handle,
                 CopyType::HOST_TO_REMOTE_DEVICE,
                 results[static_cast<int32_t>(CopyType::HOST_TO_REMOTE_DEVICE)]);
    auto ret = barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after H2RD, ret:" << ret << " rank:" << rankId);
}

void BandWidthCalculator::H2RH(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId,
                               uint32_t remoteRankId, BwTestResult *results)
{
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_HOST, SMEM_MEM_TYPE_HOST, remoteRankId, handle,
                 CopyType::HOST_TO_REMOTE_HOST,
                 results[static_cast<int32_t>(CopyType::HOST_TO_REMOTE_HOST)]);
    auto ret = barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after H2RH, ret:" << ret << " rank:" << rankId);
}

void BandWidthCalculator::D2RD(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId,
                               uint32_t remoteRankId, BwTestResult *results)
{
    if (cmdParam_.opType == SMEMB_DATA_OP_HOST_RDMA) {
        LOG_WARN("D2RD not support in HOST_RDMA mode");
        barrier->Barrier();
        return;
    }
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_DEVICE, SMEM_MEM_TYPE_DEVICE, remoteRankId, handle,
                 CopyType::DEVICE_TO_REMOTE_DEVICE,
                 results[static_cast<int32_t>(CopyType::DEVICE_TO_REMOTE_DEVICE)]);
    auto ret = barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after D2RD, ret:" << ret << " rank:" << rankId);
}

void BandWidthCalculator::D2RH(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId,
                               uint32_t remoteRankId, BwTestResult *results)
{
    BatchCopyPut(SMEM_MEM_TYPE_LOCAL_DEVICE, SMEM_MEM_TYPE_HOST, remoteRankId, handle,
                 CopyType::DEVICE_TO_REMOTE_HOST,
                 results[static_cast<int32_t>(CopyType::DEVICE_TO_REMOTE_HOST)]);
    auto ret = barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after D2RH, ret:" << ret << " rank:" << rankId);
}

void BandWidthCalculator::RH2D(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId,
                               uint32_t remoteRankId, BwTestResult *results)
{
    BatchCopyGet(SMEM_MEM_TYPE_LOCAL_DEVICE, SMEM_MEM_TYPE_HOST, remoteRankId, handle,
                 CopyType::REMOTE_HOST_TO_DEVICE,
                 results[static_cast<int32_t>(CopyType::REMOTE_HOST_TO_DEVICE)]);
    auto ret = barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after RH2D, ret:" << ret << " rank:" << rankId);
}

void BandWidthCalculator::RH2H(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId,
                               uint32_t remoteRankId, BwTestResult *results)
{
    BatchCopyGet(SMEM_MEM_TYPE_LOCAL_HOST, SMEM_MEM_TYPE_HOST, remoteRankId, handle,
                 CopyType::REMOTE_HOST_TO_HOST,
                 results[static_cast<int32_t>(CopyType::REMOTE_HOST_TO_HOST)]);
    auto ret = barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after RH2H, ret:" << ret << " rank:" << rankId);
}

void BandWidthCalculator::RD2D(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId,
                               uint32_t remoteRankId, BwTestResult *results)
{
    if (cmdParam_.opType == SMEMB_DATA_OP_HOST_RDMA) {
        LOG_WARN("RD2D not support in HOST_RDMA mode");
        barrier->Barrier();
        return;
    }
    BatchCopyGet(SMEM_MEM_TYPE_LOCAL_DEVICE, SMEM_MEM_TYPE_DEVICE, remoteRankId, handle,
                 CopyType::REMOTE_DEVICE_TO_DEVICE,
                 results[static_cast<int32_t>(CopyType::REMOTE_DEVICE_TO_DEVICE)]);
    auto ret = barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after RD2D, ret:" << ret << " rank:" << rankId);
}

void BandWidthCalculator::RD2H(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId,
                               uint32_t remoteRankId, BwTestResult *results)
{
    if (cmdParam_.opType == SMEMB_DATA_OP_HOST_RDMA) {
        LOG_WARN("RD2H not support in HOST_RDMA mode");
        barrier->Barrier();
        return;
    }
    BatchCopyGet(SMEM_MEM_TYPE_LOCAL_HOST, SMEM_MEM_TYPE_DEVICE, remoteRankId, handle,
                 CopyType::REMOTE_DEVICE_TO_HOST,
                 results[static_cast<int32_t>(CopyType::REMOTE_DEVICE_TO_HOST)]);
    auto ret = barrier->Barrier();
    CHECK_RET_VOID(ret, "barrier failed after RD2H, ret:" << ret << " rank:" << rankId);
}
