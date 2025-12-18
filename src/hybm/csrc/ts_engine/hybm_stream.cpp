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
#include "hybm_stream.h"
#include "hybm_common_include.h"
#include "dl_hal_api.h"
#include "dl_hal_api_def.h"
#include "hybm_gva.h"
#include "hybm_logger.h"
#include "ptracer.h"
#include "hybm_ptracer.h"

namespace ock {
namespace mf {
constexpr uint32_t HYBM_SQE_PRINT_WIDTH = 8U;

HybmStream::HybmStream(uint32_t deviceId, uint32_t prio, uint32_t flags) noexcept
    : deviceId_{deviceId},
      prio_{prio},
      flags_{flags}
{
}

int HybmStream::Initialize() noexcept
{
    uint32_t ssid = 0;
    int32_t ret = 0;

    tsId_ = 0; // 当前仅支持0
    ret = AllocStreamId();
    BM_ASSERT_RETURN(ret == 0, ret);

    ret = AllocSqcq(ssid);
    BM_ASSERT_RETURN(ret == 0, ret);

    ret = AllocLogicCq();
    BM_ASSERT_RETURN(ret == 0, ret);

    BM_LOG_INFO("init stream ok, stream:" << streamId_ << " sq:" << sqId_ << " cq:" << cqId_
        << " logic:" << logicCq_ << " ssid:" << ssid);
    runningTaskCount_.store(0L);
    taskList_.resize(HYBM_SQCQ_DEPTH);
    inited_ = true;
    return BM_OK;
}

int32_t HybmStream::AllocStreamId()
{
    if (streamId_ != UINT32_MAX) {
        return BM_OK;
    }

    struct halResourceIdInputInfo resAllocInput{};
    struct halResourceIdOutputInfo resAllocOutput;

    resAllocInput.type = DRV_STREAM_ID;
    resAllocInput.tsId = tsId_;

    auto ret = DlHalApi::HalResourceIdAlloc(deviceId_, &resAllocInput, &resAllocOutput);
    if (ret != 0) {
        BM_LOG_ERROR("alloc stream id failed, ts_id:" << tsId_ << " ret: " << ret);
        return BM_ERROR;
    }

    streamId_ = static_cast<uint32_t>(resAllocOutput.resourceId);
    return BM_OK;
}

int32_t HybmStream::AllocSqcq(uint32_t ssid)
{
    halSqCqInputInfo input{};
    halSqCqOutputInfo output{};
    StreamAllocInfo *sinfo = (StreamAllocInfo *)input.info;

    input.type = DRV_NORMAL_TYPE;
    input.tsId = tsId_;
    input.sqeSize = 64U;
    input.cqeSize = 12U;
    input.sqeDepth = HYBM_SQCQ_DEPTH;
    input.cqeDepth = HYBM_SQCQ_DEPTH;
    input.grpId = 0;
    input.flag = flags_;
    input.cqId = 0;
    input.sqId = 0;
    input.res[SQCQ_RESV_LENGTH - 1] = ssid; // set ssid

    sinfo->streamId = streamId_;
    sinfo->priority = 0U;
    sinfo->satMode = 1U;
    sinfo->overflowEn = 0U;
    sinfo->threadDisableFlag = 1U;
    sinfo->shareSqId = UINT32_MAX;
    sinfo->tsSqType = 0U;

    auto ret = DlHalApi::HalSqCqAllocate(deviceId_, &input, &output);
    if (ret != 0) {
        BM_LOG_INFO("allocate sq_cq with ts_id:" << tsId_ << " failed: " << ret);
        return ret;
    }

    sqId_ = output.sqId;
    cqId_ = output.cqId;
    return BM_OK;
}

int32_t HybmStream::AllocLogicCq()
{
    halSqCqInputInfo input{};
    halSqCqOutputInfo output{};

    input.type = DRV_LOGIC_TYPE;
    input.tsId = tsId_;
    input.sqeSize = 0U;
    input.cqeSize = static_cast<uint32_t>(sizeof(rtLogicCqReport_t));
    input.sqeDepth = 0U;
    input.cqeDepth = 4096U;
    input.grpId = 0;
    input.flag = 0;
    input.cqId = 65535U;
    input.sqId = 0;
    input.info[0] = streamId_;

    pid_t realTid = syscall(SYS_gettid);
    if (realTid < 0) {
        BM_LOG_ERROR("get real tid failed " << realTid);
        return BM_ERROR;
    }
    input.info[1] = static_cast<uint32_t>(realTid);

    auto ret = DlHalApi::HalSqCqAllocate(deviceId_, &input, &output);
    if (ret != 0) {
        BM_LOG_INFO("allocate logic cq with ts_id:" << tsId_ << " failed: " << ret);
        return ret;
    }
    logicCq_ = output.cqId;

    struct halResourceIdInputInfo in = {};
    in.type = DRV_STREAM_ID;
    in.tsId = tsId_;
    in.resourceId = streamId_;
    in.res[1U] = 0;

    struct halResourceConfigInfo configInfo = {};
    configInfo.prop = DRV_STREAM_BIND_LOGIC_CQ;
    configInfo.value[0U] = logicCq_; // res[0]: logicCqId

    ret = DlHalApi::HalResourceConfig(deviceId_, &in, &configInfo);
    if (ret != 0) {
        BM_LOG_INFO("bind logic cq with ts_id:" << tsId_ << " failed: " << ret);
        halSqCqFreeInfo freeInfo{};
        freeInfo.type = DRV_LOGIC_TYPE;
        freeInfo.tsId = tsId_;
        freeInfo.sqId = output.sqId;
        freeInfo.cqId = output.cqId;
        DlHalApi::HalSqCqFree(deviceId_, &freeInfo);
        return ret;
    }
    return BM_OK;
}

void HybmStream::Destroy()
{
    if (!inited_.load()) {
        return;
    }

    halSqCqFreeInfo info{};
    info.type = DRV_NORMAL_TYPE;
    info.tsId = tsId_;
    info.sqId = sqId_;
    info.cqId = cqId_;
    info.flag = 0;

    auto ret = DlHalApi::HalSqCqFree(deviceId_, &info);
    if (ret != 0) {
        BM_LOG_ERROR("free sq_cq failed: " << ret);
        return;
    }

    struct halResourceIdInputInfo resFreeInput{};
    resFreeInput.type = DRV_STREAM_ID;
    resFreeInput.tsId = tsId_;
    resFreeInput.resourceId = streamId_;
    resFreeInput.res[1U] = 0U;
    ret = DlHalApi::HalResourceIdFree(deviceId_, &resFreeInput);
    if (ret != 0) {
        BM_LOG_ERROR("free stream id failed: " << ret);
        return;
    }

    tsId_ = std::numeric_limits<uint32_t>::max();
    sqId_ = 0;
    cqId_ = 0;
    streamId_ = UINT32_MAX;
    inited_ = false;
}

void PrintSqe(const rtStarsSqe_t *sqe)
{
    const uint32_t * const cmd = reinterpret_cast<const uint32_t *>(sqe);
    std::ostringstream info;
    for (size_t i = 0UL; i < (sizeof(rtStarsSqe_t) / sizeof(uint32_t)); i++) {
        info << " " << std::setw(HYBM_SQE_PRINT_WIDTH) << std::setfill('0') << std::hex << cmd[i];
    }
    BM_LOG_INFO("SQE:" << info.str());
}

int32_t HybmStream::SubmitTasks(const StreamTask &tasks) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(inited_, "stream not init!", BM_NOT_INITIALIZED);
    int ret = 0;
    if ((sqTail_ + UN40) % HYBM_SQCQ_DEPTH == sqHead_) {
        ret = Synchronize(sqHead_);
        BM_ASSERT_LOG_AND_RETURN(ret == BM_OK, "stream synchronize failed!", BM_ERROR);
    }

    BM_ASSERT_LOG_AND_RETURN(((sqTail_ + 1U) % HYBM_SQCQ_DEPTH != sqHead_), "stream if full!", BM_NOT_INITIALIZED);
    uint32_t taskId = sqTail_;
    sqTail_ = (sqTail_ + 1U) % HYBM_SQCQ_DEPTH;

    taskList_[taskId] = tasks;
    taskList_[taskId].sqe.memcpyAsyncSqe.header.task_id = taskId;

    halTaskSendInfo info{};
    info.type = DRV_NORMAL_TYPE;
    info.sqe_addr = (uint8_t *)(ptrdiff_t)(const void *)&(taskList_[taskId].sqe);
    info.sqe_num = 1U;
    info.tsId = tsId_;
    info.sqId = sqId_;

    ret = DlHalApi::HalSqTaskSend(deviceId_, &info);
    if (ret != 0) {
        BM_LOG_ERROR("SQ send task failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

bool HybmStream::GetCqeStatus()
{
    struct halSqCqQueryInfo queryInfoIn = {};
    queryInfoIn.type = DRV_NORMAL_TYPE;
    queryInfoIn.tsId = tsId_;
    queryInfoIn.sqId = sqId_;
    queryInfoIn.cqId = 0U;
    queryInfoIn.prop = DRV_SQCQ_PROP_SQ_CQE_STATUS;

    TP_TRACE_BEGIN(TP_HYBM_SDMA_G2G_HAL_QUERY_SQ_STATUS);
    auto ret = DlHalApi::HalSqCqQuery(deviceId_, &queryInfoIn);
    TP_TRACE_END(TP_HYBM_SDMA_G2G_HAL_QUERY_SQ_STATUS, ret);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "HalSqCqQuery failed! ret:" << ret, false);
    return (queryInfoIn.value[0] != 0U);
}

int32_t HybmStream::GetSqHead(uint32_t &head)
{
    struct halSqCqQueryInfo queryInfoIn = {};
    queryInfoIn.type = DRV_NORMAL_TYPE;
    queryInfoIn.tsId = tsId_;
    queryInfoIn.sqId = sqId_;
    queryInfoIn.cqId = 0U;
    queryInfoIn.prop = DRV_SQCQ_PROP_SQ_HEAD;

    TP_TRACE_BEGIN(TP_HYBM_SDMA_G2G_HAL_QUERY_SQ_HEAD);
    auto ret = DlHalApi::HalSqCqQuery(deviceId_, &queryInfoIn);
    TP_TRACE_END(TP_HYBM_SDMA_G2G_HAL_QUERY_SQ_HEAD, ret);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "HalSqCqQuery failed! ret:" << ret, BM_ERROR);
    head = static_cast<uint16_t>(queryInfoIn.value[0] & 0xFFFFU);
    return (head != 0xffff ? BM_OK : BM_ERROR);
}

static constexpr auto SDMA_CQE_ERROR_MAX = 16;

static std::string GetCqeErrorStr(rtLogicCqReport_t &cqe)
{
    static std::string sdmaCqeError[] = {
        "normal",                                      // 0
        "read response error or sqe invalid opcode",   // 1
        "bit ecc",                                     // 2
        "transfer page error, smmu return terminate",  // 3
        "meeting TLBI",                                // 4
        "non safe access",                             // 5
        "DAW, MSD or address error",                   // 6
        "operation fail",                              // 7
        "sdma move DDRC ERROR",                        // 8
        "sdma move COMPERR ERROR",                     // 9
        "sdma move COMPDATAERR ERROR",                 // 10
        "reduce overflow",                             // 11
        "reduce float infinity",                       // 12
        "reduce source data NaN",                      // 13
        "reduce dest data NaN",                        // 14
        "reduce both source and dest data NaN",        // 15
        "data is not equal"                            // 16
    };

    if (cqe.sqeType == RT_STARS_SQE_TYPE_SDMA) {
        if (cqe.errorCode <= SDMA_CQE_ERROR_MAX) {
            return sdmaCqeError[cqe.errorCode];
        }
    }

    return "unknown";
}

int32_t HybmStream::ReceiveCqe(uint32_t &lastTask)
{
    int32_t retFlag = BM_OK;
    uint32_t revNum = 0;
    while (true) {
        halReportRecvInfo info{};
        rtLogicCqReport_t reportInfo[RT_MILAN_MAX_QUERY_CQE_NUM] = {};
        info.type = DRV_LOGIC_TYPE;
        info.tsId = tsId_;
        info.cqId = logicCq_;
        info.timeout = 0;
        info.cqe_addr = reinterpret_cast<uint8_t *>(reportInfo);
        info.cqe_num = RT_MILAN_MAX_QUERY_CQE_NUM;
        info.stream_id = streamId_;
        info.task_id = UINT16_MAX;
        info.report_cqe_num = RT_MILAN_MAX_QUERY_CQE_NUM;
        auto ret = DlHalApi::HalCqReportRecv(deviceId_, &info);
        if (ret != 0) {
            BM_LOG_ERROR("HalCqReportRecv failed: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
        if (info.report_cqe_num > RT_MILAN_MAX_QUERY_CQE_NUM) {
            BM_LOG_ERROR("Invalid report_cqe_num");
            return BM_INVALID_PARAM;
        }

        for (uint32_t idx = 0; idx < info.report_cqe_num; idx++) {
            lastTask = reportInfo[idx].taskId;
            if (reportInfo[idx].errorCode != 0) {
                BM_LOG_ERROR("task exec failed, stream:"
                             << reportInfo[idx].streamId
                             << " sqeType:" << static_cast<uint32_t>(reportInfo[idx].sqeType)
                             << " cqeErrorCode:" << reportInfo[idx].errorCode << "(" << GetCqeErrorStr(reportInfo[idx])
                             << ") cqeErrorType:" << static_cast<uint32_t>(reportInfo[idx].errorType));
                retFlag = BM_ERROR;
                PrintSqe(&taskList_[reportInfo[idx].taskId].sqe);
            }
        }

        revNum += info.report_cqe_num;
        if (info.report_cqe_num == 0) {
            break;
        }
    }
    BM_LOG_DEBUG("receive task count: " << revNum << " ret:" << retFlag << " last:" << lastTask);
    return retFlag;
}

int HybmStream::Synchronize(uint32_t task) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(inited_, "stream not init!", BM_NOT_INITIALIZED);
    constexpr int MAX_RETRY = 1000000;
    int retry = 0;
    int ret = BM_OK;
    while (sqHead_ != sqTail_ && TaskInRange(task) && retry < MAX_RETRY) {
        uint32_t head = UINT16_MAX;
        ret = GetSqHead(head);
        BM_ASSERT_LOG_AND_RETURN(ret == 0, "GetSqHead failed! ret:" << ret, ret);
        BM_ASSERT_LOG_AND_RETURN(head < HYBM_SQCQ_DEPTH, "GetSqHead invalid! head:" << head, BM_ERROR);

        if (!GetCqeStatus()) { // no cqe
            while (sqHead_ != head) {
                BM_LOG_DEBUG("finished task, task_Id:" << sqHead_ << " sqTail:" << sqTail_ << " task_type:"
                                                       << static_cast<int32_t>(taskList_[sqHead_].type));
                sqHead_ = (sqHead_ + 1U) % HYBM_SQCQ_DEPTH;
            }
        } else {
            uint32_t lastTask = UINT16_MAX;
            ret = ReceiveCqe(lastTask);
            if (lastTask != UINT16_MAX) {
                sqHead_ = (lastTask + 1U) % HYBM_SQCQ_DEPTH;
            }
            BM_ASSERT_LOG_AND_RETURN(ret == 0, "ReceiveCqe failed! ret:" << ret, ret);
        }
        retry++;
        usleep(1U);
    }

    return ret;
}
}
}