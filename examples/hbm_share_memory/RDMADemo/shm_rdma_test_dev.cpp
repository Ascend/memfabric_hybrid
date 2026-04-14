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
#include "kernel_operator.h"
#include "smem_shm_aicore_base_api.h"
#include "shm_rdma_test_dev.h"

constexpr int32_t RANK_SIZE_MAX = 32;
constexpr int32_t BLOCK_LEN = SMEM_SHM_ALIGN_SIZE / sizeof(int64_t);
constexpr int64_t FLAG_MAGIC = 3285742LL;

constexpr uint32_t MESSAGE_SIZE = 64;
constexpr uint32_t UB_ALIGN_SIZE = 32;
constexpr uint32_t DEBUG_PRINT_SIZE = 120;
constexpr uint32_t PACKET_NUM = 10000;

extern "C" __global__ __aicore__ void shm_rdma_pollcq_test(GM_ADDR gva, uint64_t heap_size)
{
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECOUT> buf;
    pipe.InitBuffer(buf, UB_ALIGN_SIZE * 2);
    AscendC::LocalTensor<uint32_t> ubLocal32 = buf.GetWithOffset<uint32_t>(UB_ALIGN_SIZE / sizeof(uint32_t), 0);
    AscendC::LocalTensor<uint64_t> ubLocal64 =
        buf.GetWithOffset<uint64_t>(UB_ALIGN_SIZE / sizeof(uint64_t), UB_ALIGN_SIZE);
    auto myRank = smem_shm_get_global_rank();
    auto totalRank = smem_shm_get_global_rank_size();
    for (int i = 0; i < totalRank; i++) {
        if (i == myRank) {
            continue;
        }
        smem_shm_roce_pollcq_test(gva + myRank * heap_size + myRank * MESSAGE_SIZE,
                                  gva + i * heap_size + myRank * MESSAGE_SIZE, i, 0, MESSAGE_SIZE, ubLocal64, ubLocal32,
                                  gva + myRank * heap_size + totalRank * MESSAGE_SIZE + i * DEBUG_PRINT_SIZE);
    }
}

void shm_rdma_pollcq_test_do(void *stream, uint8_t *gva, uint64_t heap_size)
{
    shm_rdma_pollcq_test<<<1, nullptr, stream>>>(gva, heap_size);
}

extern "C" __global__ __aicore__ void shm_rdma_read_test(GM_ADDR gva, uint64_t heap_size)
{
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECOUT> buf;
    pipe.InitBuffer(buf, UB_ALIGN_SIZE * 2);
    AscendC::LocalTensor<uint32_t> ubLocal32 = buf.GetWithOffset<uint32_t>(UB_ALIGN_SIZE / sizeof(uint32_t), 0);
    AscendC::LocalTensor<uint64_t> ubLocal64 =
        buf.GetWithOffset<uint64_t>(UB_ALIGN_SIZE / sizeof(uint64_t), UB_ALIGN_SIZE);
    auto myRank = smem_shm_get_global_rank();
    auto totalRank = smem_shm_get_global_rank_size();
    for (int i = 0; i < totalRank; i++) {
        if (i == myRank) {
            continue;
        }
        smem_shm_roce_read(gva + i * heap_size + myRank * MESSAGE_SIZE,
                           gva + myRank * heap_size + myRank * MESSAGE_SIZE, i, 0, MESSAGE_SIZE, ubLocal64, ubLocal32);
    }
}

void shm_rdma_read_test_do(void *stream, uint8_t *gva, uint64_t heap_size)
{
    shm_rdma_read_test<<<1, nullptr, stream>>>(gva, heap_size);
}

extern "C" __global__ __aicore__ void shm_rdma_write_test(GM_ADDR gva, uint64_t heap_size)
{
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECOUT> buf;
    pipe.InitBuffer(buf, UB_ALIGN_SIZE * 2);
    AscendC::LocalTensor<uint32_t> ubLocal32 = buf.GetWithOffset<uint32_t>(UB_ALIGN_SIZE / sizeof(uint32_t), 0);
    AscendC::LocalTensor<uint64_t> ubLocal64 =
        buf.GetWithOffset<uint64_t>(UB_ALIGN_SIZE / sizeof(uint64_t), UB_ALIGN_SIZE);
    auto myRank = smem_shm_get_global_rank();
    auto totalRank = smem_shm_get_global_rank_size();
    for (int i = 0; i < totalRank; i++) {
        if (i == myRank) {
            continue;
        }
        for (uint32_t packetIdx = 0; packetIdx < PACKET_NUM; packetIdx++) {
            smem_shm_roce_write(gva + myRank * heap_size + myRank * MESSAGE_SIZE,
                                gva + i * heap_size + myRank * MESSAGE_SIZE, i, 0, MESSAGE_SIZE, ubLocal64, ubLocal32);
        }
    }
}

void shm_rdma_write_test_do(void *stream, uint8_t *gva, uint64_t heap_size)
{
    shm_rdma_write_test<<<1, nullptr, stream>>>(gva, heap_size);
}

extern "C" __global__ __aicore__ void shm_rdma_get_qpinfo_test(GM_ADDR gva, uint32_t rankId)
{
    smem_shm_roce_qpinfo_test(gva, rankId, 0);
}

void shm_rdma_get_qpinfo_test_do(void *stream, uint8_t *gva, uint32_t rankId)
{
    shm_rdma_get_qpinfo_test<<<1, nullptr, stream>>>(gva, rankId);
}