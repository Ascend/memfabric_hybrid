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

#ifndef MF_HYBRID_SMEM_TRANS_STORE_HELPER_H
#define MF_HYBRID_SMEM_TRANS_STORE_HELPER_H

#include <array>
#include <string>
#include <functional>
#include <queue>
#include <utility>
#include "hybm_def.h"
#include "mf_net.h"
#include "smem_common_includes.h"
#include "smem_config_store.h"
#include "smem_trans_def.h"

namespace ock {
namespace smem {
const std::string AUTO_RANK_KEY_PREFIX = "auto_ranking_key_";  // 每个rank的key公共前辍，用于记录对应的rankId
const std::string CLUSTER_RANKS_INFO_KEY = "cluster_ranks_info";  // rank的基本信息，用于抢占rankId

const std::string SENDER_COUNT_KEY = "count_for_senders";
const std::string SENDER_DEVICE_INFO_KEY = "devices_info_for_senders";
const std::string RECEIVER_COUNT_KEY = "receiver_for_senders";
const std::string RECEIVER_DEVICE_INFO_KEY = "devices_info_for_receivers";

const std::string RECEIVER_TOTAL_SLICE_COUNT_KEY = "receivers_total_slices_count";
const std::string RECEIVER_SLICES_INFO_KEY = "receivers_all_slices_info";

const std::string SENDER_TOTAL_SLICE_COUNT_KEY = "senders_total_slices_count";
const std::string SENDER_SLICES_INFO_KEY = "senders_all_slices_info";
enum DataStatusType : uint8_t { ABNORMAL = 0, NORMAL};
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
    ock::mf::net_addr_t address{};
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
    uint16_t rankId;
    uint8_t info[0];

    StoredSliceInfo(WorkerUniqueId ws, const void *a, uint64_t s, uint16_t rId) noexcept
        : session(std::move(ws)), address{a}, size{s}, rankId{rId}
    {
    }
};

struct RemoteLocalInfo {
    bool isValid{false};
    uint16_t deviceIdInRemote_;
    std::queue<uint16_t> sliceIdInRemote_;
};


using FindRanksCbFunc = std::function<int(const std::vector<hybm_exchange_info> &)>;
using FindSlicesCbFunc =
    std::function<int(const std::vector<hybm_exchange_info> &,
                      const std::vector<const StoredSliceInfo *> &,
                      const std::vector<const StoredSliceInfo *> &)>;

class SmemStoreHelper {
public:
    SmemStoreHelper(std::string name, std::string storeUrl, smem_trans_role_t role) noexcept;
    int Initialize(uint16_t entityId, int32_t maxRetry) noexcept;
    void Destroy() noexcept;
    void SetSliceExportSize(size_t sliceExportSize) noexcept;
    int GenerateRankId(const smem_trans_config_t &config, uint16_t &rankId) noexcept;
    int StoreDeviceInfo(const hybm_exchange_info &info) noexcept;
    int StoreSliceInfo(const hybm_exchange_info &info, const StoredSliceInfo &sliceInfo) noexcept;
    int ReStoreDeviceInfo() noexcept;
    int ReStoreSliceInfo() noexcept;
    void FindNewRemoteRanks(const FindRanksCbFunc &cb) noexcept;
    void FindNewRemoteSlices(const FindSlicesCbFunc &cb) noexcept;
    int ReRegisterToServer(uint16_t rankId) noexcept;
    int CheckServerStatus() noexcept;
    void AlterServerStatus(bool status) noexcept;
    int ReConnect() noexcept;
    void RegisterBrokenHandler(const ConfigStoreClientBrokenHandler &handler);

private:
    int RecoverRankInformation(std::vector<uint8_t> rankIdValue, uint16_t &rankId,
                               const smem_trans_config_t &cfg, std::string key,
                               bool &isRestore) noexcept;
    void CompareAndUpdateDeviceInfo(uint32_t minCount, std::vector<uint8_t> &values,
                                    std::vector<hybm_exchange_info> &addInfo) noexcept;
    void CompareAndUpdateSliceInfo(uint32_t minCount, std::vector<uint8_t> &values,
                                   std::vector<hybm_exchange_info> &addInfo,
                                   std::vector<const StoredSliceInfo *> &addStoreSs,
                                   std::vector<const StoredSliceInfo *> &removeStoreSs) noexcept;
    void ExtraDeviceChangeInfo(std::vector<uint8_t> &values,
                                                std::vector<hybm_exchange_info> &addInfo) noexcept;
    void ExtraSliceChangeInfo(std::vector<uint8_t> &values,
                              std::vector<hybm_exchange_info> &addInfo,
                              std::vector<const StoredSliceInfo *> &addStoreSs,
                              std::vector<const StoredSliceInfo *> &removeStoreSs) noexcept;
    const std::string name_;
    const std::string storeURL_;
    const smem_trans_role_t transRole_;
    UrlExtraction urlExtraction_;
    StoreKeys localKeys_;
    StoreKeys remoteKeys_;
    StorePtr store_ = nullptr;
    size_t deviceExpSize_ = 0;
    size_t sliceExpSize_ = 0;

    std::pair<uint16_t, std::vector<uint8_t>> storeRankIdInfo_;
    std::pair<uint16_t, std::vector<uint8_t>> storeDeviceInfo_;
    std::vector<std::pair<uint16_t, std::vector<uint8_t>>> storeSliceInfo_;
    RemoteLocalInfo remoteRankInfo_;

    std::vector<uint8_t> remoteDeviceInfoLastTime_;
    std::vector<uint8_t> remoteSlicesInfoLastTime_;
};
}
}

#endif  // MF_HYBRID_SMEM_TRANS_STORE_HELPER_H
