/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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

    int RegisterMemory(uintptr_t buffer_addr, size_t capacity);

    // must be called before TransferAdapterPy::~TransferAdapterPy()
    int UnregisterMemory(uintptr_t buffer_addr);

    int BatchRegisterMemory(std::vector<uintptr_t> buffer_addrs, std::vector<size_t> capacities);

    void TransferDestroy();

    void UnInitialize();

private:
    smem_bm_t handle_;
};

#endif // PYTRANSFER_H
