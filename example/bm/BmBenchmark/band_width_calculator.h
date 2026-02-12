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
#ifndef BAND_WIDTH_CALCULATOR_H
#define BAND_WIDTH_CALCULATOR_H

#include <string>
#include <vector>
#include <unistd.h>
#include <cstdint>
#include "barrier_util.h"
#include "common_def.h"
#include "acl/acl.h"
#include "smem.h"
#include "smem_bm.h"
#include "hybm_def.h"

enum class CopyType {
    HOST_TO_DEVICE = 0,
    DEVICE_TO_HOST,
    HOST_TO_REMOTE_DEVICE,
    HOST_TO_REMOTE_HOST,
    DEVICE_TO_REMOTE_DEVICE,
    DEVICE_TO_REMOTE_HOST,
    REMOTE_HOST_TO_DEVICE,
    REMOTE_HOST_TO_HOST,
    REMOTE_DEVICE_TO_DEVICE,
    REMOTE_DEVICE_TO_HOST,
    ALL_DIRECTION
};

typedef struct {
    void **localAddrs;
    void **globalAddrs;
    uint64_t *dataSizes;
    uint32_t batchSize;
} BatchCopyParam;

typedef struct {
    CopyType type{CopyType::HOST_TO_DEVICE};
    smem_bm_data_op_type opType{SMEMB_DATA_OP_BUTT};
    std::string ipPort{};
    std::string rdmaIpPort{"tcp://0.0.0.0/0:10005"};
    uint32_t worldRankSize{8};
    uint32_t localRankSize{8};
    uint32_t localRankStart{0};
    uint64_t copyCount{1000};
    uint64_t copySize{1024};
    uint64_t batchSize{1};
    uint32_t deviceId{0};
} BwTestParam;

typedef struct {
    int32_t flag;
    char typeName[32];
    uint32_t devId;
    uint32_t rankId;
    uint64_t totalTimeUs;
    uint64_t copySize;
    uint64_t batchSize;
    uint64_t copyCount;
    int64_t wrongNum;
} BwTestResult;

class BandWidthCalculator {
public:
    BandWidthCalculator() = default;
    explicit BandWidthCalculator(BwTestParam &param);
    ~BandWidthCalculator() = default;

    int32_t MultiProcessExecute();
    int32_t MultiThreadExecute();

private:
    int32_t PreInit(uint32_t deviceId, BarrierUtil *&barrier, uint32_t rankId, uint32_t rkSize, aclrtStream *stream);
    void FinalizeAll(uint32_t deviceId, BarrierUtil *&barrier, aclrtStream *stream);

    int32_t PrepareLocalMem(smem_bm_t handle, uint32_t rankId);
    int32_t PrepareCopyParam(smem_bm_mem_type localMemType, smem_bm_mem_type rmtMemType, uint32_t gvaRankId,
                             smem_bm_t handle, BatchCopyParam &param);
    int64_t CheckCopyResult(smem_bm_mem_type localMemType, smem_bm_mem_type rmtMemType, uint32_t gvaRankId,
                            smem_bm_t handle);

    void PrintResult(std::vector<BwTestResult> &results);
    void SendResult(BwTestResult *results, int32_t pipeFdWrite);

    int32_t Execute(uint32_t deviceId, uint32_t rankId, uint32_t localRankNum, uint32_t rkSize, int32_t pipeFdWrite);

    int32_t BandWidthCalculation(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId, uint32_t remoteRankId,
                              BwTestResult *results);

    void BatchCopyPut(smem_bm_mem_type localMemType, smem_bm_mem_type rmtMemType, uint32_t gvaRankId, smem_bm_t handle,
                      CopyType type, BwTestResult &result);
    void BatchCopyGet(smem_bm_mem_type localMemType, smem_bm_mem_type rmtMemType, uint32_t gvaRankId, smem_bm_t handle,
                      CopyType type, BwTestResult &result);

    void H2D(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId, uint32_t remoteRankId, BwTestResult *results);
    void D2H(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId, uint32_t remoteRankId, BwTestResult *results);
    void H2RD(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId, uint32_t remoteRankId, BwTestResult *results);
    void H2RH(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId, uint32_t remoteRankId, BwTestResult *results);
    void D2RD(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId, uint32_t remoteRankId, BwTestResult *results);
    void D2RH(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId, uint32_t remoteRankId, BwTestResult *results);
    void RH2D(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId, uint32_t remoteRankId, BwTestResult *results);
    void RH2H(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId, uint32_t remoteRankId, BwTestResult *results);
    void RD2D(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId, uint32_t remoteRankId, BwTestResult *results);
    void RD2H(BarrierUtil *barrier, smem_bm_t handle, uint32_t rankId, uint32_t remoteRankId, BwTestResult *results);

private:
    BwTestParam cmdParam_;
    bool isMaster_ = false;
    void *localDram_ = nullptr;
    void *localHbm_ = nullptr;
    void *registedLocalDram_ = nullptr;
    void *registedLocalHbm_ = nullptr;
};

#endif