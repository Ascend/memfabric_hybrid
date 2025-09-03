/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_SMEM_TRANS_STORE_HELPER_H
#define MF_HYBRID_SMEM_TRANS_STORE_HELPER_H

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
    uint32_t address{0};
    uint16_t port{0};
    uint16_t reserved{0};
};

union WorkerIdUnion {
    WorkerUniqueId session;
    uint64_t workerId;

    explicit WorkerIdUnion(WorkerUniqueId ws) : session(ws) {}
    explicit WorkerIdUnion(uint64_t id) : workerId{id} {}
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
