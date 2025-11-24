/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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

struct ServerFaultRankIndex {
    uint16_t rankId;
    uint16_t deviceInfoId;
    std::vector<uint16_t> sliceInfoIdVec;
};
class SmemStoreFaultHandler {
public:
    static SmemStoreFaultHandler &GetInstance();
    SmemStoreFaultHandler(const SmemStoreFaultHandler&) = delete;
    SmemStoreFaultHandler& operator=(const SmemStoreFaultHandler&) = delete;
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
    void ClearFaultInfo(const uint32_t linkId,
                           std::unordered_map<std::string, std::vector<uint8_t>> &kvStore);
    std::unordered_map<uint32_t, RankInfo> linkIdToRankInfoMap_;
    std::queue<ServerFaultRankIndex> faultRankIndexQueue_;
};
}
}
#endif
