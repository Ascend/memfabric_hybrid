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
#include "hybm_stream_notify.h"
#include "hybm_task.h"
#include "dl_hal_api.h"
#include "dl_hal_api_def.h"
#include "hybm_logger.h"

namespace ock {
namespace mf {

int32_t HybmStreamNotify::Init()
{
    BM_ASSERT_LOG_AND_RETURN(stream_ != nullptr, "stream is null!", BM_ERROR);

    auto ret = NotifyIdAlloc(stream_->GetDevId(), stream_->GetTsId(), notifyid_);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "create notify id failed.", ret);

    drvNotifyInfo drvInfo = {};
    drvInfo.tsId = stream_->GetTsId();
    drvInfo.notifyId = notifyid_;
    ret = DlHalApi::DrvNotifyIdAddrOffset(stream_->GetDevId(), &drvInfo);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "get notify offset failed.", ret);
    offset_ = drvInfo.devAddrOffset;
    return BM_OK;
}

int32_t HybmStreamNotify::NotifyIdAlloc(uint32_t devId, uint32_t tsId, uint32_t &notifyId)
{
    struct halResourceIdInputInfo resAllocInput{};
    struct halResourceIdOutputInfo resAllocOutput;

    resAllocInput.type = DRV_NOTIFY_ID;
    resAllocInput.tsId = tsId;
    resAllocInput.res[1U] = 0U;
    resAllocOutput.resourceId = 0U;

    auto ret = DlHalApi::HalResourceIdAlloc(devId, &resAllocInput, &resAllocOutput);
    if (ret != 0) {
        BM_LOG_ERROR("alloc stream id failed, ts_id:" << tsId << " dev_id:" << devId << " ret: " << ret);
        return BM_ERROR;
    }

    notifyId = resAllocOutput.resourceId;
    BM_LOG_DEBUG("create notify, ts_id:" << tsId << " dev_id:" << devId << " id: " << notifyid_);
    return BM_OK;
}

int32_t HybmStreamNotify::Wait()
{
    StreamTask task{};
    task.type = STREAM_TASK_TYPE_NOTIFY;
    rtStarsNotifySqe_t *const sqe = &(task.sqe.notifySqe);
    sqe->header.type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
    sqe->header.ie = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.pre_p = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.post_p = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.wr_cqe = stream_->GetWqeFlag();
    sqe->kernel_credit = RT_STARS_NEVER_TIMEOUT_KERNEL_CREDIT;
    sqe->header.rt_stream_id = static_cast<uint16_t>(stream_->GetId());
    sqe->notify_id = notifyid_;
    sqe->header.task_id = 0U;
    sqe->res2 = 0U;
    sqe->res3 = 0U;
    sqe->timeout = TASK_WAIT_TIME_OUT;

    auto ret = stream_->SubmitTasks(task);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "submit notify wait task failed.", ret);
    ret = stream_->Synchronize();
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "notify wait task do failed.", ret);
    return BM_OK;
}

}
}