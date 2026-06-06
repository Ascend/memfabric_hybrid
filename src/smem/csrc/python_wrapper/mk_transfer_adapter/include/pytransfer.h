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
inline auto to_uintptr(const void *p) -> uintptr_t
{
    return reinterpret_cast<uintptr_t>(p);
}
#else
using uintptr_t = fallback_uintptr;
inline auto to_uintptr(const void *p) -> fallback_uintptr
{
    return fallback_uintptr(p);
}
#endif

class TransferAdapterPy {
public:
    enum class TransferOpcode { READ = 0, WRITE = 1 };
    enum class TransDataOpType {
        SDMA = SMEMB_DATA_OP_SDMA,
        DEVICE_RDMA = SMEMB_DATA_OP_DEVICE_RDMA,
        DEVICE_URMA = SMEMB_DATA_OP_DEVICE_URMA
    };

public:
    TransferAdapterPy();

    ~TransferAdapterPy();

    int Initialize(const char *storeUrl, const char *uniqueId, const char *role, uint32_t deviceId,
                   TransDataOpType dataOpType);

    std::string GetRpcPort();

    int TransferSyncWrite(const char *destUniqueId, uintptr_t buffer, uintptr_t peer_buffer_address, size_t length,
                          uint32_t flags);

    int BatchTransferSyncWrite(const char *destUniqueId, std::vector<uintptr_t> buffers,
                               std::vector<uintptr_t> peer_buffer_addresses, std::vector<size_t> lengths,
                               uint32_t flags);

    int TransferSyncRead(const char *destUniqueId, uintptr_t buffer, uintptr_t peer_buffer_address, size_t length,
                         uint32_t flags);

    int BatchTransferSyncRead(const char *destUniqueId, std::vector<uintptr_t> buffers,
                              std::vector<uintptr_t> peer_buffer_addresses, std::vector<size_t> lengths,
                              uint32_t flags);

    int TransferAsyncReadSubmit(const char *destUniqueId, uintptr_t buffer, uintptr_t peer_buffer_address,
                                size_t length, uintptr_t stream, uint32_t flags);

    int TransferAsyncWriteSubmit(const char *destUniqueId, uintptr_t buffer, uintptr_t peer_buffer_address,
                                 size_t length, uintptr_t stream, uint32_t flags);

    int BatchTransferAsyncWriteSubmit(const char *destUniqueId,
                                      std::vector<uintptr_t> buffers,
                                      std::vector<uintptr_t> peer_buffer_addresses,
                                      std::vector<size_t> lengths,
                                      uintptr_t stream, uint32_t flags);

    int BatchTransferAsyncReadSubmit(const char *destUniqueId,
                                     std::vector<uintptr_t> buffers,
                                     std::vector<uintptr_t> peer_buffer_addresses,
                                     std::vector<size_t> lengths,
                                     uintptr_t stream, uint32_t flags);

    int BatchTransferWriteWithQuant(const char *destUniqueId,
                                    std::vector<uintptr_t> buffers,
                                    std::vector<uintptr_t> peer_buffer_addresses,
                                    std::vector<size_t> lengths,
                                    std::vector<uintptr_t> scale_addresses,
                                    std::vector<uintptr_t> offset_addresses,
                                    uint32_t unit_num,
                                    uint32_t input_type,
                                    uintptr_t stream, uint32_t flags);

    int RegisterMemory(uintptr_t buffer_addr, size_t capacity);

    // must be called before TransferAdapterPy::~TransferAdapterPy()
    int UnregisterMemory(uintptr_t buffer_addr);

    int BatchRegisterMemory(std::vector<uintptr_t> buffer_addrs, std::vector<size_t> capacities);

    uintptr_t TransMalloc(size_t capacity);

    int TransFree(uintptr_t buffer_addr);

    void TransferDestroy();

    void UnInitialize();

private:
    smem_bm_t handle_;
    int sockfd_;
};

#endif // PYTRANSFER_H
