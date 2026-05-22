/*
Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
ZBAL is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
     http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
*/
#ifndef ZBAL_KERNEL_DATA_OP_H
#define ZBAL_KERNEL_DATA_OP_H

#include "kernel_operator.h"

#define ZBAL_STARS_NOTIFY_ADDR_OFFSET (14 * 1024) // notify_addr offset (14KB)
#define ZBAL_SDMA_FLAG_LENGTH         64
#define ZBAL_MAX_AIV_PER_NPU          40
constexpr uint32_t UB_ALIGN_SIZE_64 = 64;
constexpr uint64_t ZBAL_DATA_CACHE_LINE_SIZE = 64;

struct sdma_config_t {
    uint64_t block_bytes;
    uint64_t per_core_bytes;
    uint32_t queue_num;
    uint32_t iter_num;
};

struct workspace_layout_t {
    __gm__ uint8_t *send_workspace;
    __gm__ uint8_t *recv_workspace;
    __gm__ uint8_t *remote_recv_workspace;
};

struct stars_channel_flag_info_t {
    uint32_t flag;
    uint32_t totalQueueNum;
    uint8_t reserved[56]; // 对齐到64字节
};

struct stars_channel_info_t {
    uint32_t sq_head;
    uint32_t sq_tail;
    uint64_t sq_base;     // SQ缓冲区基地址
    uint64_t sq_reg_base; // SQ寄存器基地址
    uint32_t sq_depth;
    uint32_t sq_id;
    uint32_t cq_id;
    uint32_t logic_cq_id;
    uint64_t cqe_addr;
    uint32_t report_cqe_num;
    uint32_t stream_id;
    uint32_t dev_id;
    uint8_t reserved[4]; // 对齐到64字节
};

struct stars_sqe_header_t {
    uint8_t type : 6;
    uint16_t res1 : 10;
    uint16_t block_dim;
    uint16_t rt_streamid;
    uint16_t task_id;
};

struct stars_sdma_sqe_t {
    stars_sqe_header_t header;
    /**** 8 bytpes ****/

    uint32_t res3;
    /* *******12 bytes********* */

    uint16_t res4;
    uint8_t kernel_credit;
    uint8_t ptr_mode : 1;
    uint8_t res5 : 7;
    /**** 16 bytpes ****/

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
    uint32_t res6 : 4;
    /**** 20 bytpes ****/

    uint16_t src_streamid;
    uint16_t src_sub_streamid;
    /**** 24 bytpes ****/

    uint16_t dst_streamid;
    uint16_t dst_sub_streamid;

    uint32_t length;
    /**** 32 bytpes ****/

    uint32_t src_addr_low;
    uint32_t src_addr_high;
    uint32_t dst_addr_low;
    uint32_t dst_addr_high;
    /**** 48 bytpes ****/

    uint8_t link_type;
    uint8_t resvered[3];
    uint32_t reslast[3];
    /**** 64 bytpes ****/
};

struct stars_notify_sqe_t {
    stars_sqe_header_t header;
    /**** 8 bytpes ****/

    uint32_t notify_id : 13;
    uint32_t res2 : 19;

    uint16_t res3;
    uint8_t kernel_credit;
    uint8_t res4;
    /**** 16 bytpes ****/

    uint32_t timeout;
    uint32_t res5[11];
    /**** 64 bytpes ****/
};

constexpr uint64_t SMEM_SHM_DEVICE_END_ADDR = 0x180000000000ULL - (1UL << 30UL);
constexpr uint64_t SMEM_SHM_DEVICE_PRE_META_SIZE = 128UL;                            // 128B
constexpr uint64_t SMEM_SHM_DEVICE_GLOBAL_META_SIZE = SMEM_SHM_DEVICE_PRE_META_SIZE; // 128B
constexpr uint64_t SMEM_OBJECT_NUM_MAX = 511UL;                                      // entity最大数量
constexpr uint64_t SMEM_SHM_DEVICE_META_SIZE =
    SMEM_SHM_DEVICE_PRE_META_SIZE * SMEM_OBJECT_NUM_MAX + SMEM_SHM_DEVICE_GLOBAL_META_SIZE; // 64K

constexpr uint64_t SMEM_SHM_DEVICE_USER_CONTEXT_PRE_SIZE = 64UL * 1024UL; // 64K
constexpr uint64_t SMEM_SHM_DEVICE_INFO_SIZE = SMEM_SHM_DEVICE_USER_CONTEXT_PRE_SIZE * SMEM_OBJECT_NUM_MAX +
                                               SMEM_SHM_DEVICE_META_SIZE; // 元数据+用户context,总大小32M, 对齐2M
constexpr uint64_t SMEM_SHM_DEVICE_META_ADDR = SMEM_SHM_DEVICE_END_ADDR - SMEM_SHM_DEVICE_INFO_SIZE;
constexpr uint64_t SMEM_SHM_DEVICE_META_OBJ_ID_OFFSET = 0;
constexpr uint64_t SMEM_SHM_DEVICE_META_RANK_OFFSET = SMEM_SHM_DEVICE_META_OBJ_ID_OFFSET + sizeof(uint32_t);
constexpr uint64_t SMEM_SHM_DEVICE_META_RANK_SIZE_OFFSET = SMEM_SHM_DEVICE_META_RANK_OFFSET + sizeof(uint32_t);
constexpr uint64_t SMEM_SHM_DEVICE_META_CONTEXT_OFFSET = SMEM_SHM_DEVICE_META_RANK_SIZE_OFFSET + sizeof(uint32_t);
constexpr uint64_t SMEM_SHM_DEVICE_META_SYMM_OFFSET = SMEM_SHM_DEVICE_META_CONTEXT_OFFSET + sizeof(uint32_t);
constexpr uint64_t SMEM_SHM_DEVICE_META_QP_INFO_OFFSET = SMEM_SHM_DEVICE_META_SYMM_OFFSET + sizeof(uint64_t);
constexpr uint64_t SMEM_SHM_DEVICE_META_SDMA_WORKSPACE_OFFSET =
    SMEM_SHM_DEVICE_META_QP_INFO_OFFSET + sizeof(uint64_t); // sdma_workspace_addr: sdma aicpu和aiv的共享内存

constexpr uint8_t ZBAL_SQE_TYPE_SDMA = 11;
constexpr uint8_t ZBAL_SQE_TYPE_NOTIFY_RECORD = 6;
constexpr uint8_t ZBAL_STARS_DEFAULT_KERNEL_CREDIT = 240U;
constexpr uint8_t ZBAL_DEFAULT_KERNEL_CREDIT = 254U;

ZBAL_KERNEL void dcci_cacheline(__gm__ uint8_t *addr)
{
    using namespace AscendC;
    GlobalTensor<uint8_t> global;
    global.SetGlobalBuffer(addr);

    __asm__ __volatile__("");
    DataCacheCleanAndInvalid<uint8_t, CacheLine::SINGLE_CACHE_LINE, DcciDst::CACHELINE_OUT>(global);
    __asm__ __volatile__("");
}

ZBAL_KERNEL void dcci_cachelines(__gm__ uint8_t *addr, uint64_t length)
{
    __gm__ uint8_t *start = (__gm__ uint8_t *)((uint64_t)addr / ZBAL_DATA_CACHE_LINE_SIZE * ZBAL_DATA_CACHE_LINE_SIZE);
    __gm__ uint8_t *end =
        (__gm__ uint8_t *)(((uint64_t)addr + length) / ZBAL_DATA_CACHE_LINE_SIZE * ZBAL_DATA_CACHE_LINE_SIZE);
    AscendC::GlobalTensor<uint8_t> global;
    global.SetGlobalBuffer(start);
    for (uint64_t i = 0; i <= end - start; i += ZBAL_DATA_CACHE_LINE_SIZE) {
        __asm__ __volatile__("");
        AscendC::DataCacheCleanAndInvalid<uint8_t, AscendC::CacheLine::SINGLE_CACHE_LINE,
                                          AscendC::DcciDst::CACHELINE_OUT>(global[i]);
        __asm__ __volatile__("");
    }
}

ZBAL_KERNEL __gm__ void *zbal_get_sdma_workspace_address(uint32_t shmemId)
{
    if (shmemId >= SMEM_OBJECT_NUM_MAX) {
        return NULL;
    }

    uint64_t metaAddr =
        SMEM_SHM_DEVICE_META_ADDR + SMEM_SHM_DEVICE_GLOBAL_META_SIZE + shmemId * SMEM_SHM_DEVICE_PRE_META_SIZE;
    return *(__gm__ void **)(metaAddr + SMEM_SHM_DEVICE_META_SDMA_WORKSPACE_OFFSET);
}

template<typename T>
ZBAL_KERNEL void zbal_set_value(__gm__ uint8_t *addr, T x, AscendC::LocalTensor<T> &tmp_local, uint32_t sync_id)
{
    AscendC::GlobalTensor<T> gm_dst;
    gm_dst.SetGlobalBuffer((__gm__ T *)addr);
    tmp_local.SetValue(0, x);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopyExtParams cp_out_params{1, sizeof(T), 0, 0, 0};
    // ub2gm
    AscendC::DataCopyPad(gm_dst, tmp_local, cp_out_params);
    AscendC::SetFlag<AscendC::HardEvent::MTE3_MTE2>(sync_id);
    AscendC::WaitFlag<AscendC::HardEvent::MTE3_MTE2>(sync_id);
}

ZBAL_KERNEL void zbal_fill_sdma_sqe(__gm__ stars_channel_info_t *channel_info, __gm__ uint8_t *src, __gm__ uint8_t *dst,
                                    uint32_t length, uint32_t sq_tail, uint32_t task_id)
{
    __gm__ stars_sdma_sqe_t *sqe = (__gm__ stars_sdma_sqe_t *)((channel_info->sq_base));
    sqe += (sq_tail % channel_info->sq_depth);

    sqe->header.type = ZBAL_SQE_TYPE_SDMA;
    sqe->header.block_dim = 0;
    sqe->header.rt_streamid = channel_info->stream_id;
    sqe->header.task_id = task_id;

    sqe->kernel_credit = ZBAL_STARS_DEFAULT_KERNEL_CREDIT;
    sqe->ptr_mode = 0;

    sqe->opcode = 0;
    sqe->ie2 = 0;
    sqe->sssv = 1U;
    sqe->dssv = 1U;
    sqe->sns = 1U;
    sqe->dns = 1U;
    sqe->qos = 6; // 6 is HCCL QoS
    sqe->partid = 0U;
    sqe->mpam = 0;
    sqe->length = length;

    uint64_t src_addr = reinterpret_cast<uint64_t>(src);
    uint64_t dst_addr = reinterpret_cast<uint64_t>(dst);

    sqe->src_addr_low = static_cast<uint32_t>(src_addr & 0xFFFFFFFF);
    sqe->src_addr_high = static_cast<uint32_t>((src_addr >> 32) & 0xFFFFFFFF);
    sqe->dst_addr_low = static_cast<uint32_t>(dst_addr & 0xFFFFFFFF);
    sqe->dst_addr_high = static_cast<uint32_t>((dst_addr >> 32) & 0xFFFFFFFF);
    sqe->link_type = static_cast<uint8_t>(255U);
}

ZBAL_KERNEL void zbal_fill_notify_record_sqe(__gm__ stars_channel_info_t *channel_info, uint32_t sq_tail,
                                             uint32_t task_id, uint32_t notify_id)
{
    __gm__ stars_notify_sqe_t *sqe = (__gm__ stars_notify_sqe_t *)((channel_info->sq_base));
    sqe += (sq_tail % channel_info->sq_depth);

    sqe->header.type = ZBAL_SQE_TYPE_NOTIFY_RECORD;
    sqe->header.block_dim = 0;
    sqe->header.rt_streamid = channel_info->stream_id;
    sqe->header.task_id = task_id;
    sqe->notify_id = notify_id;
    sqe->kernel_credit = ZBAL_DEFAULT_KERNEL_CREDIT;
}

ZBAL_KERNEL __gm__ uint8_t *zbal_sdma_get_channel_base()
{
    __gm__ uint8_t *context_gm = reinterpret_cast<__gm__ uint8_t *>(zbal_get_sdma_workspace_address(0));
    return context_gm + sizeof(stars_channel_flag_info_t);
}

ZBAL_KERNEL void zbal_stars_submit_notify_record(AscendC::LocalTensor<uint32_t> &tmp_local, AscendC::TEventID sync_id)
{
    const auto cur_block_idx = AscendC::GetBlockIdx();
    const uint32_t queue_num = 1;

    __gm__ uint8_t *channel_base = zbal_sdma_get_channel_base();
    __gm__ stars_channel_info_t *batch_write_channel_info =
        (__gm__ stars_channel_info_t *)(channel_base) + cur_block_idx * queue_num;
    __gm__ uint32_t *notify_addr = reinterpret_cast<__gm__ uint32_t *>(
        channel_base - sizeof(stars_channel_flag_info_t) + ZBAL_STARS_NOTIFY_ADDR_OFFSET);

    for (uint32_t queue_id = 0U; queue_id < queue_num; ++queue_id) {
        __gm__ stars_channel_info_t *channel_info = batch_write_channel_info + queue_id;
        dcci_cacheline(((__gm__ uint8_t *)channel_info) + 4);
        uint32_t sq_tail = *((__gm__ uint32_t *)(((__gm__ uint8_t *)channel_info) + 4));

        zbal_fill_notify_record_sqe(channel_info, sq_tail, sq_tail - channel_info->sq_head,
                                    notify_addr[cur_block_idx * queue_num + queue_id]);

        sq_tail = (sq_tail + 1) % (channel_info->sq_depth);

        // Flush data cache to ensure all SQEs are written back to HBM
        AscendC::GlobalTensor<uint8_t> write_info;
        write_info.SetGlobalBuffer((__gm__ uint8_t *)(channel_info->sq_base), sizeof(stars_notify_sqe_t));
        AscendC::DataCacheCleanAndInvalid<uint8_t, AscendC::CacheLine::ENTIRE_DATA_CACHE,
                                          AscendC::DcciDst::CACHELINE_OUT>(write_info);

        // Ring Doorbell
        zbal_set_value<uint32_t>((__gm__ uint8_t *)(channel_info->sq_reg_base) + 8, sq_tail, tmp_local, sync_id);
        zbal_set_value<uint32_t>(((__gm__ uint8_t *)channel_info) + 4, sq_tail, tmp_local, sync_id);
    }
}

template<typename T>
ZBAL_KERNEL void zbal_sdma_notify_record(AscendC::LocalTensor<T> &buf, uint32_t sync_id)
{
    AscendC::LocalTensor<uint32_t> ub_tensor;
    ub_tensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECOUT);
    ub_tensor.address_.bufferAddr = reinterpret_cast<uint64_t>(buf.GetPhyAddr());
    ub_tensor.address_.dataLen = UB_ALIGN_SIZE_64;

    zbal_stars_submit_notify_record(ub_tensor, sync_id);
}

ZBAL_KERNEL void zbal_sdma_submit_data_sqes(__gm__ stars_channel_info_t *batch_write_channel_info,
                                            __gm__ uint8_t *send_buffer, __gm__ uint8_t *recv_buffer,
                                            const sdma_config_t &config, uint32_t *sq_tail)
{
    for (uint32_t idx = 0U; idx < config.iter_num; ++idx) {
        uint32_t queue_idx = idx % config.queue_num;
        __gm__ stars_channel_info_t *channel_info = batch_write_channel_info + queue_idx;

        // This transfer data length
        uint32_t transfer_bytes = config.block_bytes;
        if (idx == config.iter_num - 1) {
            transfer_bytes = config.per_core_bytes - idx * config.block_bytes;
        }

        __gm__ uint8_t *src_addr = send_buffer + idx * config.block_bytes;
        __gm__ uint8_t *dst_addr = recv_buffer + idx * config.block_bytes;

        zbal_fill_sdma_sqe(channel_info, src_addr, dst_addr, transfer_bytes, sq_tail[queue_idx],
                           sq_tail[queue_idx] - channel_info->sq_head);

        sq_tail[queue_idx] = (sq_tail[queue_idx] + 1) % (channel_info->sq_depth);
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}

ZBAL_KERNEL void zbal_sdma_post_send(__gm__ uint8_t *recv_buffer, __gm__ uint8_t *send_buffer, uint64_t message_len,
                                     AscendC::LocalTensor<uint32_t> &tmp_local, uint32_t sync_id)
{
    __gm__ uint8_t *channel_base = zbal_sdma_get_channel_base();

    const auto cur_block_idx = AscendC::GetBlockIdx();
    // 1. Initialize per-core parameters, including channel count, etc.
    sdma_config_t config;
    config.queue_num = 1;
    config.block_bytes = 1024 * 1024; // 1MB per SQE
    config.per_core_bytes = message_len;
    config.iter_num = (config.per_core_bytes + config.block_bytes - 1) / config.block_bytes;

    if (config.iter_num == 0 || cur_block_idx >= (ZBAL_MAX_AIV_PER_NPU / config.queue_num)) {
        AscendC::PipeBarrier<PIPE_ALL>();
        return;
    }

    // 2. Calculate base channel index for the current core
    __gm__ stars_channel_info_t *batch_write_channel_base = (__gm__ stars_channel_info_t *)(channel_base);
    __gm__ stars_channel_info_t *batch_write_channel_info = batch_write_channel_base + cur_block_idx * config.queue_num;

    // 3. Calculate base addresses of send and receive flags
    __gm__ uint8_t *workspace = channel_base + ZBAL_MAX_AIV_PER_NPU * sizeof(stars_channel_info_t);
    workspace_layout_t workspace_layout;
    uint64_t per_core_workspace_size = config.queue_num * ZBAL_SDMA_FLAG_LENGTH;
    __gm__ uint8_t *my_workspace = workspace + ZBAL_SDMA_FLAG_LENGTH + (cur_block_idx * per_core_workspace_size);

    workspace_layout.send_workspace = workspace;
    workspace_layout.recv_workspace = my_workspace;
    workspace_layout.remote_recv_workspace = my_workspace + ZBAL_MAX_AIV_PER_NPU * ZBAL_SDMA_FLAG_LENGTH;

    // Set the send workspace address, notify remote peer to start sending data
    zbal_set_value<uint32_t>((__gm__ uint8_t *)workspace_layout.send_workspace, config.queue_num, tmp_local, sync_id);

    // 4. Initialize the sq_tail array
    uint32_t sq_tail[ZBAL_MAX_AIV_PER_NPU] = {0};
    for (uint32_t queue_id = 0U; queue_id < config.queue_num; ++queue_id) {
        __gm__ stars_channel_info_t *channel_info = batch_write_channel_info + queue_id;
        // Load sq_tail field at 4-byte offset
        dcci_cacheline(((__gm__ uint8_t *)channel_info) + 4);
        sq_tail[queue_id] = *((__gm__ uint32_t *)(((__gm__ uint8_t *)channel_info) + 4));
    }

    // 5. Submit data transfer SQE
    zbal_sdma_submit_data_sqes(batch_write_channel_info, send_buffer, recv_buffer, config, sq_tail);

    // 6. Ring doorbell
    auto item_size = config.iter_num * sizeof(stars_sdma_sqe_t);
    for (uint8_t queue_id = 0; queue_id < config.queue_num; queue_id++) {
        __gm__ stars_channel_info_t *channel_info = batch_write_channel_info + queue_id;

        // Flush data cache to ensure all SQEs are written back to HBM
        AscendC::GlobalTensor<uint8_t> write_info;
        write_info.SetGlobalBuffer((__gm__ uint8_t *)(channel_info->sq_base), item_size);
        AscendC::DataCacheCleanAndInvalid<uint8_t, AscendC::CacheLine::ENTIRE_DATA_CACHE,
                                          AscendC::DcciDst::CACHELINE_OUT>(write_info);

        // Ring Doorbell
        zbal_set_value<uint32_t>((__gm__ uint8_t *)(channel_info->sq_reg_base) + 8, sq_tail[queue_id], tmp_local,
                                 sync_id);
        zbal_set_value<uint32_t>(((__gm__ uint8_t *)channel_info) + 4, sq_tail[queue_id], tmp_local, sync_id);
    }

    AscendC::PipeBarrier<PIPE_ALL>();
}

template<typename T>
ZBAL_KERNEL void zbal_sdma_get_nbi(__gm__ T *dst, __gm__ T *src, __ubuf__ T *buf, uint32_t ub_size, uint32_t elem_size,
                                   uint32_t sync_id)
{
    // Create LocalTensor from buf pointer and ub_size
    AscendC::LocalTensor<uint32_t> ub_tensor;
    ub_tensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECOUT);
    ub_tensor.address_.bufferAddr = reinterpret_cast<uint64_t>(buf);
    ub_tensor.address_.dataLen = ub_size;

    zbal_sdma_post_send((__gm__ uint8_t *)dst, (__gm__ uint8_t *)src, elem_size * sizeof(T), ub_tensor, sync_id);
}

template<typename T>
ZBAL_KERNEL void zbal_sdma_get_nbi(AscendC::GlobalTensor<T> &dst, AscendC::GlobalTensor<T> &src,
                                   AscendC::LocalTensor<T> &buf, uint32_t elem_size, uint32_t sync_id)
{
    AscendC::LocalTensor<uint32_t> ub_tensor;
    ub_tensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECOUT);
    ub_tensor.address_.bufferAddr = reinterpret_cast<uint64_t>(buf.GetPhyAddr());
    ub_tensor.address_.dataLen = UB_ALIGN_SIZE_64;

    zbal_sdma_post_send((__gm__ uint8_t *)(dst.GetPhyAddr()), (__gm__ uint8_t *)(src.GetPhyAddr()),
                        elem_size * sizeof(T), ub_tensor, sync_id);
}

ZBAL_KERNEL void zbal_sdma_submit_flag_sqes(__gm__ stars_channel_info_t *batch_write_channel_info,
                                            const workspace_layout_t &layout, AscendC::LocalTensor<uint32_t> &tmp_local,
                                            uint32_t sync_id)
{
    const uint32_t queue_num = 1;
    for (uint32_t queue_id = 0U; queue_id < queue_num; ++queue_id) {
        __gm__ stars_channel_info_t *channel_info = batch_write_channel_info + queue_id;
        dcci_cachelines((__gm__ uint8_t *)channel_info + 4, 4);
        uint32_t sq_tail = *((__gm__ uint32_t *)((__gm__ uint8_t *)channel_info + 4));

        const uint32_t flag_size = 8;

        zbal_fill_sdma_sqe(channel_info, layout.send_workspace, // send flag buffer of current core
                           layout.remote_recv_workspace +
                               queue_id * ZBAL_SDMA_FLAG_LENGTH, // write flag location for this channel
                           flag_size, sq_tail, sq_tail - channel_info->sq_head);

        sq_tail = (sq_tail + 1) % (channel_info->sq_depth);

        AscendC::GlobalTensor<uint8_t> write_info;
        write_info.SetGlobalBuffer((__gm__ uint8_t *)(channel_info->sq_base), sizeof(stars_channel_info_t));
        AscendC::DataCacheCleanAndInvalid<uint8_t, AscendC::CacheLine::ENTIRE_DATA_CACHE,
                                          AscendC::DcciDst::CACHELINE_OUT>(write_info);

        // Ring Doorbell
        zbal_set_value<uint32_t>((__gm__ uint8_t *)(channel_info->sq_reg_base) + 8, sq_tail, tmp_local, sync_id);
        zbal_set_value<uint32_t>(((__gm__ uint8_t *)channel_info) + 4, sq_tail, tmp_local, sync_id);
    }
}

template<typename T>
ZBAL_KERNEL void copy_gm_to_gm(__gm__ uint8_t *dst, __gm__ uint8_t *src, uint32_t size,
                               AscendC::LocalTensor<T> &tmp_local, uint32_t sync_id)
{
    AscendC::GlobalTensor<T> gm_src;
    AscendC::GlobalTensor<T> gm_dst;
    gm_src.SetGlobalBuffer((__gm__ T *)src, size);
    gm_dst.SetGlobalBuffer((__gm__ T *)dst, size);

    uint32_t cp_len = size * sizeof(T);
    AscendC::DataCopyExtParams cp_params{1, cp_len, 0, 0, 0};
    AscendC::DataCopyPadExtParams<T> pad_params{false, 0, 0, 0};
    // gm2ub
    AscendC::DataCopyPad(tmp_local, gm_src, cp_params, pad_params);
    AscendC::PipeBarrier<PIPE_ALL>();
    // ub2gm
    AscendC::DataCopyPad(gm_dst, tmp_local, cp_params);
    AscendC::SetFlag<AscendC::HardEvent::MTE3_MTE2>(sync_id);
    AscendC::WaitFlag<AscendC::HardEvent::MTE3_MTE2>(sync_id);
}

ZBAL_KERNEL void zbal_sdma_poll_for_completion(const workspace_layout_t &layout,
                                               AscendC::LocalTensor<uint32_t> &tmp_local, uint32_t sync_id)
{
    uint32_t queue_num = 1;
    const uint32_t max_times = 1000000;
    for (uint8_t queue_id = 0; queue_id < queue_num; queue_id++) {
        auto local_recv_workspace = layout.recv_workspace + queue_id * ZBAL_SDMA_FLAG_LENGTH;
        auto remote_recv_workspace = layout.remote_recv_workspace + queue_id * ZBAL_SDMA_FLAG_LENGTH;

        uint32_t send_value = 0;
        uint32_t times = 0;

        // Poll until flag is received or timeout occurs
        while (send_value == 0 && times < max_times) {
            copy_gm_to_gm<uint32_t>(local_recv_workspace, remote_recv_workspace, 1, tmp_local, sync_id);
            dcci_cacheline(local_recv_workspace);
            send_value = *((__gm__ uint32_t *)local_recv_workspace);
            times++;
        }

        // Clear
        zbal_set_value<uint32_t>(remote_recv_workspace, 0, tmp_local, sync_id);
        zbal_set_value<uint32_t>(local_recv_workspace, 0, tmp_local, sync_id);
    }
}

template<typename T>
ZBAL_KERNEL void zbal_sdma_quiet(AscendC::LocalTensor<T> &buf, uint32_t sync_id)
{
    AscendC::LocalTensor<uint32_t> ub_tensor;
    ub_tensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECOUT);
    ub_tensor.address_.bufferAddr = reinterpret_cast<uint64_t>(buf.GetPhyAddr());
    ub_tensor.address_.dataLen = UB_ALIGN_SIZE_64;

    const uint32_t queue_num = 1;
    const auto cur_block_idx = AscendC::GetBlockIdx();
    __gm__ uint8_t *channel_base = zbal_sdma_get_channel_base();
    __gm__ stars_channel_info_t *batch_write_channel_info =
        (__gm__ stars_channel_info_t *)(channel_base) + cur_block_idx * queue_num;

    workspace_layout_t layout;
    layout.send_workspace = channel_base + ZBAL_MAX_AIV_PER_NPU * sizeof(stars_channel_info_t);
    layout.recv_workspace = layout.send_workspace + ZBAL_SDMA_FLAG_LENGTH * (cur_block_idx * queue_num + 1);
    layout.remote_recv_workspace = layout.recv_workspace + ZBAL_MAX_AIV_PER_NPU * ZBAL_SDMA_FLAG_LENGTH;

    zbal_sdma_submit_flag_sqes(batch_write_channel_info, layout, ub_tensor, sync_id);
    zbal_sdma_poll_for_completion(layout, ub_tensor, sync_id);
}

template<typename T>
ZBAL_KERNEL void zbal_sdma_quiet(__ubuf__ T *buf, uint32_t ub_size, uint32_t sync_id)
{
    AscendC::LocalTensor<uint32_t> ub_tensor;
    ub_tensor.address_.logicPos = static_cast<uint8_t>(AscendC::TPosition::VECOUT);
    ub_tensor.address_.bufferAddr = reinterpret_cast<uint64_t>(buf);
    ub_tensor.address_.dataLen = ub_size;

    const uint32_t queue_num = 1;
    const auto cur_block_idx = AscendC::GetBlockIdx();
    __gm__ uint8_t *channel_base = zbal_sdma_get_channel_base();
    __gm__ stars_channel_info_t *batch_write_channel_info =
        (__gm__ stars_channel_info_t *)(channel_base) + cur_block_idx * queue_num;

    workspace_layout_t layout;
    layout.send_workspace = channel_base + ZBAL_MAX_AIV_PER_NPU * sizeof(stars_channel_info_t);
    layout.recv_workspace = layout.send_workspace + ZBAL_SDMA_FLAG_LENGTH * (cur_block_idx * queue_num + 1);
    layout.remote_recv_workspace = layout.recv_workspace + ZBAL_MAX_AIV_PER_NPU * ZBAL_SDMA_FLAG_LENGTH;

    zbal_sdma_submit_flag_sqes(batch_write_channel_info, layout, ub_tensor, sync_id);
    zbal_sdma_poll_for_completion(layout, ub_tensor, sync_id);
}

#endif // ZBAL_KERNEL_DATA_OP_H