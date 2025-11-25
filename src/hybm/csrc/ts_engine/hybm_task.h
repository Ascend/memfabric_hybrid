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
#ifndef MF_HYBRID_HYBM_TASK_H
#define MF_HYBRID_HYBM_TASK_H

#include <cstdint>

namespace ock {
namespace mf {

constexpr uint8_t RT_STARS_SQE_TYPE_NOTIFY_RECORD = 6U;
constexpr uint8_t RT_STARS_SQE_TYPE_NOTIFY_WAIT = 7U;
constexpr uint8_t RT_STARS_SQE_TYPE_WRITE_VALUE = 8U;
constexpr uint8_t RT_STARS_SQE_TYPE_SDMA = 11U;

constexpr uint8_t RT_STARS_DEFAULT_KERNEL_CREDIT = 254U;
constexpr uint8_t RT_STARS_NEVER_TIMEOUT_KERNEL_CREDIT = 255U;
constexpr uint32_t UINT32_BIT_NUM = 32U;
constexpr uint32_t MASK_17_BIT = 0x0001FFFFU;
constexpr uint32_t MASK_32_BIT = 0xFFFFFFFFU;
constexpr uint32_t HYBM_SQCQ_DEPTH = 2048U;

/* stars send interrupt direction */
enum RtStarsSqeIntDirType {
    RT_STARS_SQE_INT_DIR_NO           = 0, // send no interrupt
    RT_STARS_SQE_INT_DIR_TO_TSCPU     = 1, // to tscpu
    RT_STARS_SQE_INT_DIR_TO_CTRLCPU   = 2, // to ctrlcpu
    RT_STARS_SQE_INT_DIR_TO_HOST      = 3, // to host
    RT_STARS_SQE_INT_DIR_END          = 4
};

enum StreamTaskType : uint32_t {
    STREAM_TASK_TYPE_SDMA = 1,
    STREAM_TASK_TYPE_RDMA = 2,
    STREAM_TASK_TYPE_NOTIFY = 3,
};

#pragma pack(push)
#pragma pack (1)
struct rtStarsSqeHeader_t {
    uint8_t type : 6;
    uint8_t l1_lock : 1;
    uint8_t l1_unlock : 1;

    uint8_t ie : 2;
    uint8_t pre_p : 2;
    uint8_t post_p : 2;
    uint8_t wr_cqe : 1;
    uint8_t reserved : 1;

    uint16_t block_dim;  // block_dim or res

    uint16_t rt_stream_id;
    uint16_t task_id;
};

struct rtStarsMemcpyAsyncSqe_t {
    rtStarsSqeHeader_t header;

    uint32_t res3;
    /********12 bytes**********/

    uint16_t res4; // max_retry(u8) retry_cnt(u8)
    uint8_t kernelCredit;
    uint8_t ptrMode : 1;
    uint8_t res5 : 7;
    /********16 bytes**********/

    uint32_t opcode : 8;
    uint32_t ie2 : 1;
    uint32_t sssv : 1;
    uint32_t dssv : 1;
    uint32_t sns : 1;
    uint32_t dns : 1;
    uint32_t qos : 4;
    uint32_t sro : 1;
    uint32_t dro : 1;
    uint32_t partid : 8;
    uint32_t mpam : 1;
    uint32_t d2dOffsetFlag : 1;
    uint32_t res6 : 3;
    /********20 bytes**********/

    uint16_t src_streamid;
    uint16_t src_sub_streamid;
    uint16_t dst_streamid;
    uint16_t dstSubStreamId;
    /********28 bytes**********/

    uint32_t length;
    uint32_t src_addr_low;
    uint32_t src_addr_high;
    uint32_t dst_addr_low;
    uint32_t dst_addr_high;

    uint32_t srcOffsetLow;
    uint32_t dstOffsetLow;
    uint16_t srcOffsetHigh;
    uint16_t dstOffsetHigh;
    uint32_t resLast[1];
};

struct rtStarsWriteValueSqe_t {
    rtStarsSqeHeader_t header;

    uint32_t res3;

    uint32_t res4 : 16;
    uint32_t kernel_credit : 8;
    uint32_t res5 : 8;

    uint32_t write_addr_low;

    uint32_t write_addr_high : 17;
    uint32_t res6 : 3;
    uint32_t awsize : 3;
    uint32_t snoop : 1;
    uint32_t awcache : 4;
    uint32_t awprot : 3;
    uint32_t va : 1; // 1 /* 1: virtual address; 0: phy addr */

    uint32_t res7; // event_id for event reset task
    uint32_t sub_type;

    uint32_t write_value_part0;
    uint32_t write_value_part1;
    uint32_t write_value_part2;
    uint32_t write_value_part3;
    uint32_t write_value_part4;
    uint32_t write_value_part5;
    uint32_t write_value_part6;
    uint32_t write_value_part7;
};

struct rtStarsNotifySqe_t {
    rtStarsSqeHeader_t header;

    uint32_t notify_id : 13;
    uint32_t res2 : 19;

    uint16_t res3;
    uint8_t kernel_credit;
    uint8_t res4;
    uint32_t timeout;
    uint32_t res5[11];
};

#pragma pack(pop)

union rtStarsSqe_t {
    rtStarsMemcpyAsyncSqe_t memcpyAsyncSqe;
    rtStarsWriteValueSqe_t writeValueSqe;
    rtStarsNotifySqe_t notifySqe;
};

struct StreamTask {
    StreamTaskType type;
    rtStarsSqe_t sqe{};
};

}
}

#endif  // MF_HYBRID_HYBM_TASK_H
