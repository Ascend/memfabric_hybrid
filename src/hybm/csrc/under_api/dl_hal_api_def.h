/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_DL_HAL_API_DEF_H
#define MF_HYBRID_DL_HAL_API_DEF_H

namespace ock {
namespace mf {

#define SQCQ_RTS_INFO_LENGTH 5
#define SQCQ_RESV_LENGTH 8
#define SQCQ_QUERY_INFO_LENGTH 8
#define RESOURCE_CONFIG_INFO_LENGTH 7
#define RESOURCEID_RESV_LENGTH  8

typedef enum tagDrvSqCqType {
    DRV_NORMAL_TYPE = 0,
    DRV_CALLBACK_TYPE,
    DRV_LOGIC_TYPE,
    DRV_SHM_TYPE,
    DRV_CTRL_TYPE,
    DRV_GDB_TYPE,
    DRV_INVALID_TYPE
} drvSqCqType_t;

typedef enum tagDrvSqCqPropType {
    DRV_SQCQ_PROP_SQ_STATUS = 0x0,
    DRV_SQCQ_PROP_SQ_HEAD,
    DRV_SQCQ_PROP_SQ_TAIL,
    DRV_SQCQ_PROP_SQ_DISABLE_TO_ENABLE,
    DRV_SQCQ_PROP_SQ_CQE_STATUS, /* read clear */
    DRV_SQCQ_PROP_SQ_REG_BASE,
    DRV_SQCQ_PROP_SQ_BASE,
    DRV_SQCQ_PROP_SQ_DEPTH,
    DRV_SQCQ_PROP_SQ_PAUSE,
    DRV_SQCQ_PROP_MAX
} drvSqCqPropType_t;

struct halTaskSendInfo {
    drvSqCqType_t type;
    uint32_t tsId;
    uint32_t sqId;
    int32_t timeout;  // send wait time
    uint8_t *sqe_addr;
    uint32_t sqe_num;
    uint32_t pos;                   /* output: first sqe pos */
    uint32_t res[SQCQ_RESV_LENGTH]; /* must zero out */
};

struct halReportRecvInfo {
    drvSqCqType_t type;
    uint32_t tsId;
    uint32_t cqId;
    int32_t timeout;  // recv wait time
    uint8_t *cqe_addr;
    uint32_t cqe_num;
    uint32_t report_cqe_num; /* output */
    uint32_t stream_id;
    uint32_t task_id; /* If this parameter is set to all 1, strict matching is not performed for taskid. */
    uint32_t res[SQCQ_RESV_LENGTH];
};

typedef struct {
    uint32_t streamId;
    uint32_t priority;
    uint32_t overflowEn : 1;
    uint32_t satMode : 1;
    uint32_t tsSqType : 1;
    uint32_t rsv : 29;
    uint32_t threadDisableFlag;
    uint32_t shareSqId;
} StreamAllocInfo;

struct halSqCqInputInfo {
    drvSqCqType_t type;  // normal : 0, callback : 1
    uint32_t tsId;
    /* The size and depth of each cqsq can be configured in normal mode, but this function is not yet supported */
    uint32_t sqeSize;   // normal : 64Byte
    uint32_t cqeSize;   // normal : 12Byte
    uint32_t sqeDepth;  // normal : 1024
    uint32_t cqeDepth;  // normal : 1024

    uint32_t grpId;  // runtime thread identifier,normal : 0
    uint32_t flag;   // ref to TSDRV_FLAG_*
    uint32_t cqId;   // if flag bit 0 is 0, don't care about it
    uint32_t sqId;   // if flag bit 1 is 0, don't care about it

    uint32_t info[SQCQ_RTS_INFO_LENGTH];  // inform to ts through the mailbox, consider single operator performance
    uint32_t res[SQCQ_RESV_LENGTH];
};

struct halSqCqOutputInfo {
    uint32_t sqId;                 // return to UMAX when there is no sq
    uint32_t cqId;                 // return to UMAX when there is cq
    unsigned long long queueVAddr; /* return shm sq addr */
    uint32_t flag;                 // ref to TSDRV_FLAG_*
    uint32_t res[SQCQ_RESV_LENGTH - 3];
};

struct halSqCqFreeInfo {
    drvSqCqType_t type;  // normal : 0, callback : 1
    uint32_t tsId;
    uint32_t sqId;
    uint32_t cqId;  // cqId to be freed, if flag bit 0 is 0, don't care about it
    uint32_t flag;  // bit 0 : whether cq is to be freed  0 : free, 1 : no free
    uint32_t res[SQCQ_RESV_LENGTH];
};

struct halSqCqQueryInfo {
    drvSqCqType_t type;
    uint32_t tsId;
    uint32_t sqId;
    uint32_t cqId;
    drvSqCqPropType_t prop;
    uint32_t value[SQCQ_QUERY_INFO_LENGTH];
};

typedef enum tagDrvIdType {
    DRV_STREAM_ID = 0,
    DRV_EVENT_ID,
    DRV_MODEL_ID,
    DRV_NOTIFY_ID,
    DRV_CMO_ID,
    DRV_CNT_NOTIFY_ID,    /* add start ascend910_95 */
    DRV_INVALID_ID,
} drvIdType_t;

typedef enum tagDrvResourceConfigType {
    DRV_STREAM_BIND_LOGIC_CQ = 0x0,
    DRV_STREAM_UNBIND_LOGIC_CQ,
    DRV_ID_RECORD,
    DRV_STREAM_ENABLE_EVENT,
    DRV_ID_RESET,
    DRV_RES_ID_CONFIG_MAX
} drvResourceConfigType_t;

struct halResourceConfigInfo {
    drvResourceConfigType_t prop;
    uint32_t value[RESOURCE_CONFIG_INFO_LENGTH];
};

struct halResourceIdInputInfo {
    drvIdType_t type;   // Resource Id Type
    uint32_t tsId;
    uint32_t resourceId;    // the id that will be freed, halResourceIdAlloc does not care about this variable
    uint32_t res[RESOURCEID_RESV_LENGTH];    // 0:stream pri, 1:flag
};

struct halResourceIdOutputInfo {
    uint32_t resourceId;
    uint32_t res[RESOURCEID_RESV_LENGTH];
};

constexpr uint32_t RT_MILAN_MAX_QUERY_CQE_NUM = 32U;

/**
 * @ingroup engine or starsEngine
 * @brief the type defination of logic cq for all chipType(total 32 bytes).
 */
struct rtLogicCqReport_t {
    volatile uint16_t streamId;
    volatile uint16_t taskId;
    volatile uint32_t errorCode;    // cqe acc_status/sq_sw_status
    volatile uint8_t errorType;     // bit0 ~ bit5 cqe stars_defined_err_code, bit 6 cqe warning bit
    volatile uint8_t sqeType;
    volatile uint16_t sqId;
    volatile uint16_t sqHead;
    volatile uint16_t matchFlag : 1;
    volatile uint16_t dropFlag : 1;
    volatile uint16_t errorBit : 1;
    volatile uint16_t accError : 1;
    volatile uint16_t reserved0 : 12;
    union {
        volatile uint64_t timeStamp;
        volatile uint16_t sqeIndex;
    } u1;
    /* Union description:
     *  Internal: enque_timestamp temporarily used as dfx
     *  External: reserved1
     */
    union {
        volatile uint64_t enqueTimeStamp;
        volatile uint64_t reserved1;
    } u2;
};

}
}

#endif  // MF_HYBRID_DL_HAL_API_DEF_H
