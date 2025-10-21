/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MF_HYBRID_SMEM_TRANS_STORE_HELPER_H
#define MF_HYBRID_SMEM_TRANS_STORE_HELPER_H

#include <array>
#include <string>
#include <functional>
#include "hybm_def.h"
#include "smem_common_includes.h"
#include "smem_config_store.h"
#include "smem_trans_def.h"

namespace ock {
namespace smem {
struct StoreKeys {
    std::string deviceCount;
    std::string sliceCount;
    std::string deviceInfo;
    std::string sliceInfo;

    StoreKeys() noexcept {}
    StoreKeys(std::string devCnt, std::string slcCnt, std::string devInfo, std::string slcInfo) noexcept
        : deviceCount{std::move(devCnt)},
          sliceCount{std::move(slcCnt)},
          deviceInfo{std::move(devInfo)},
          sliceInfo{std::move(slcInfo)}
    {
    }
};

struct WorkerUniqueId {
    net_addr_t address{};
    uint16_t port{0};
    uint16_t reserved{0};
};

using WorkerId = std::array<uint8_t, sizeof(WorkerUniqueId)>;

struct WorkerIdHash {
    size_t operator()(const WorkerId& id) const
    {
        return std::hash<std::string>()(
            std::string(id.begin(), id.end())
        );
    }
};

union WorkerIdUnion {
    WorkerUniqueId session;
    WorkerId workerId;

    explicit WorkerIdUnion(WorkerUniqueId ws) : session(ws) {}
    explicit WorkerIdUnion(WorkerId id) : workerId{id} {}
};

struct StoredSliceInfo {
    WorkerUniqueId session;
    const void *address;
    uint64_t size;
    uint8_t info[0];

    StoredSliceInfo(WorkerUniqueId ws, const void *a, uint64_t s) noexcept : session(std::move(ws)), address{a}, size{s}
    {
    }
};

using FindRanksCbFunc = std::function<int(const std::vector<hybm_exchange_info> &)>;
using FindSlicesCbFunc =
    std::function<int(const std::vector<hybm_exchange_info> &, const std::vector<const StoredSliceInfo *> &)>;

class SmemStoreHelper {
public:
    SmemStoreHelper(std::string name, std::string storeUrl, smem_trans_role_t role) noexcept;
    int Initialize(uint16_t entityId, int32_t maxRetry) noexcept;
    void Destroy() noexcept;
    void SetSliceExportSize(size_t sliceExportSize) noexcept;
    int GenerateRankId(const smem_trans_config_t &config, uint16_t &rankId) noexcept;
    int StoreDeviceInfo(const hybm_exchange_info &info) noexcept;
    int StoreSliceInfo(const hybm_exchange_info &info, const StoredSliceInfo &sliceInfo) noexcept;
    void FindNewRemoteRanks(const FindRanksCbFunc &cb) noexcept;
    void FindNewRemoteSlices(const FindSlicesCbFunc &cb) noexcept;

private:
    const std::string name_;
    const std::string storeURL_;
    const smem_trans_role_t transRole_;
    UrlExtraction urlExtraction_;
    StoreKeys localKeys_;
    StoreKeys remoteKeys_;
    StorePtr store_ = nullptr;
    size_t deviceExpSize_ = 0;
    size_t sliceExpSize_ = 0;
    int64_t remoteSliceLastTime_ = 0;
    int64_t remoteRankLastTime_ = 0;
};
}
}

#endif  // MF_HYBRID_SMEM_TRANS_STORE_HELPER_H
