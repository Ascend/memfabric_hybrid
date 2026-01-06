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

#ifndef MF_SMEM_TRANS_FAULT_H
#define MF_SMEM_TRANS_FAULT_H

#include <unordered_map>
#include <queue>
#include <string>
#include <vector>
#include "smem_config_store.h"
#include "smem_tcp_config_store_server.h"
namespace ock {
namespace smem {
struct ConfigServerDeviceInfo {
    uint16_t deviceInfoId{0};
    uint16_t deiviceInfoUint;
    std::string deviceInfoKey;
    std::string deviceCountKey;
};

struct ConfigServerSliceInfo {
    uint16_t sliceInfoUint;
    std::vector<uint16_t> sliceInfoId;
    std::string sliceInfoKey;
    std::string sliceCountKey;
};

struct RankInfo {
    uint16_t rankId;
    std::string rankName;
    ConfigServerDeviceInfo dInfo;
    ConfigServerSliceInfo sInfo;

    RankInfo() = default;
    RankInfo(uint16_t rId, std::string rName) : rankId(rId), rankName(rName) {};
};

class SmemStoreFaultHandler {
public:
    static SmemStoreFaultHandler &GetInstance();
    SmemStoreFaultHandler(const SmemStoreFaultHandler &) = delete;
    SmemStoreFaultHandler &operator=(const SmemStoreFaultHandler &) = delete;
    void RegisterHandlerToStore(StorePtr store);

private:
    SmemStoreFaultHandler() = default;
    int32_t BuildLinkIdToRankInfoMap(const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                                     const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore);
    int32_t AddRankInfoMap(const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                           const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore);
    int32_t WriteRankInfoMap(const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                             const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore);
    int32_t AppendRankInfoMap(const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                              const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore);
    int32_t GetFromFaultInfo(const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                             const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore);
    void ClearFaultInfo(const uint32_t linkId, std::unordered_map<std::string, std::vector<uint8_t>> &kvStore);

    void ClearDeviceInfo(uint32_t linkId, RankInfo &rankInfo,
                         std::unordered_map<std::string, std::vector<uint8_t>> &kvStore);

    void ClearSliceInfo(uint32_t linkId, RankInfo &rankInfo,
                        std::unordered_map<std::string, std::vector<uint8_t>> &kvStore);

    std::unordered_map<uint32_t, RankInfo> linkIdToRankInfoMap_;
    std::queue<uint16_t> faultRankIdQueue_;
    std::pair<std::queue<uint16_t>, std::queue<uint16_t>> faultDeviceIdQueue_; // first->sender, second->receiver
    std::pair<std::queue<uint16_t>, std::queue<uint16_t>> faultSliceIdQueue_;  // first->sender, second->receiver
};
} // namespace smem
} // namespace ock
#endif
