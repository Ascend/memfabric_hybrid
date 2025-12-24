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
#ifndef PYTRANSFER_H
#define PYTRANSFER_H

#include <pybind11/pybind11.h>
#include <mutex>
#include "smem_bm_def.h"
#include "smem_trans.h"

#ifdef UINTPTR_MAX
using uintptr_t = ::uintptr_t;
inline auto to_uintptr(const void* p) -> uintptr_t
{
    return reinterpret_cast<uintptr_t>(p);
}
#else
using uintptr_t = fallback_uintptr;
inline auto to_uintptr(const void* p) -> fallback_uintptr
{
    return fallback_uintptr(p);
}
#endif

class TransferAdapterPy {
public:
    enum class TransferOpcode { READ = 0, WRITE = 1 };
    enum class TransDataOpType {
        SDMA = SMEMB_DATA_OP_SDMA,
        DEVICE_RDMA = SMEMB_DATA_OP_DEVICE_RDMA
    };
public:
    TransferAdapterPy();

    ~TransferAdapterPy();

    int Initialize(const char *storeUrl, const char *uniqueId, const char *role, uint32_t deviceId,
                   TransDataOpType dataOpType);

    int GetRpcPort();

    int TransferSyncWrite(const char *destUniqueId, uintptr_t buffer,
                            uintptr_t peer_buffer_address, size_t length);

    int BatchTransferSyncWrite(const char *destUniqueId,
                            std::vector<uintptr_t> buffers,
                            std::vector<uintptr_t> peer_buffer_addresses,
                            std::vector<size_t> lengths);

    int TransferSyncRead(const char *destUniqueId, uintptr_t buffer,
                         uintptr_t peer_buffer_address, size_t length);

    int BatchTransferSyncRead(const char *destUniqueId,
                              std::vector<uintptr_t> buffers,
                              std::vector<uintptr_t> peer_buffer_addresses,
                              std::vector<size_t> lengths);

    int TransferAsyncReadSubmit(const char *destUniqueId,
                                uintptr_t buffer,
                                uintptr_t peer_buffer_address,
                                size_t length,
                                uintptr_t stream);

    int TransferAsyncWriteSubmit(const char *destUniqueId,
                                 uintptr_t buffer,
                                 uintptr_t peer_buffer_address,
                                 size_t length,
                                 uintptr_t stream);

    int RegisterMemory(uintptr_t buffer_addr, size_t capacity);

    // must be called before TransferAdapterPy::~TransferAdapterPy()
    int UnregisterMemory(uintptr_t buffer_addr);

    int BatchRegisterMemory(std::vector<uintptr_t> buffer_addrs, std::vector<size_t> capacities);

    void TransferDestroy();

    void UnInitialize();

private:
    smem_bm_t handle_;
    int sockfd_;
};

#endif // PYTRANSFER_H
