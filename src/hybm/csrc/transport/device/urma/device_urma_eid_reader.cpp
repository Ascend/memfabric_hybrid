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

#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_map>
#include <array>

#include "hybm_logger.h"
#include "dl_hcomm_api.h"
#include "device_urma_eid_reader.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

Result GetDeviceUrmaEid(uint32_t phyDeviceId, uint32_t rankId, std::array<uint8_t, COMM_ADDR_EID_LEN> &eidData)
{
    const char *eidFilePath = std::getenv("MF_DEVICE_URMA_EID_FILE");
    if (eidFilePath == nullptr || eidFilePath[0] == '\0') {
        BM_LOG_ERROR("device_urma env MF_DEVICE_URMA_EID_FILE not set, rankId=" << rankId);
        return BM_INVALID_PARAM;
    }
    std::ifstream eidFile(eidFilePath);
    if (!eidFile.is_open()) {
        BM_LOG_ERROR("device_urma cannot open EID file: " << eidFilePath << ", rankId=" << rankId);
        return BM_ERROR;
    }
    std::unordered_map<uint32_t, std::array<uint8_t, COMM_ADDR_EID_LEN>> eidMap;
    std::string line;
    while (std::getline(eidFile, line)) {
        if (line.empty()) {
            continue;
        }
        const auto colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            BM_LOG_ERROR("device_urma invalid EID file format (missing colon), line: " << line
                                                                                       << ", file: " << eidFilePath);
            return BM_INVALID_PARAM;
        }
        std::string idStr = line.substr(0, colonPos);
        auto trimLeft = idStr.find_first_not_of(" \t");
        auto trimRight = idStr.find_last_not_of(" \t");
        if (trimLeft == std::string::npos) {
            BM_LOG_ERROR("device_urma invalid EID file format (empty devPhyId), line: " << line
                                                                                        << ", file: " << eidFilePath);
            return BM_INVALID_PARAM;
        }
        idStr = idStr.substr(trimLeft, trimRight - trimLeft + 1);
        char *end = nullptr;
        const auto devId = static_cast<uint32_t>(std::strtoul(idStr.c_str(), &end, 10));
        if (*end != '\0') {
            BM_LOG_ERROR("device_urma invalid EID file format (non-numeric devPhyId), line: " << line << ", file: "
                                                                                              << eidFilePath);
            return BM_INVALID_PARAM;
        }
        std::string hexStr = line.substr(colonPos + 1);
        trimLeft = hexStr.find_first_not_of(" \t");
        if (trimLeft == std::string::npos) {
            BM_LOG_ERROR("device_urma invalid EID file format (missing hex EID), line: " << line
                                                                                         << ", file: " << eidFilePath);
            return BM_INVALID_PARAM;
        }
        hexStr = hexStr.substr(trimLeft);
        trimRight = hexStr.find_last_not_of(" \t\r\n");
        if (trimRight != std::string::npos) {
            hexStr = hexStr.substr(0, trimRight + 1);
        }
        if (hexStr.length() != COMM_ADDR_EID_LEN * 2) {
            BM_LOG_ERROR("device_urma invalid EID hex length: " << hexStr.length() << " (expected "
                                                                << (COMM_ADDR_EID_LEN * 2) << "), line: " << line
                                                                << ", file: " << eidFilePath);
            return BM_INVALID_PARAM;
        }
        std::array<uint8_t, COMM_ADDR_EID_LEN> eid{};
        for (size_t i = 0; i < COMM_ADDR_EID_LEN; ++i) {
            auto byteStr = hexStr.substr(i * 2, 2);
            char *endp = nullptr;
            auto val = std::strtoul(byteStr.c_str(), &endp, 16);
            if (*endp != '\0') {
                BM_LOG_ERROR("device_urma invalid EID hex character at byte " << i << ", line: " << line
                                                                              << ", file: " << eidFilePath);
                return BM_INVALID_PARAM;
            }
            eid[i] = static_cast<uint8_t>(val & 0xFF);
        }
        eidMap[devId] = eid;
    }
    const auto eidIt = eidMap.find(phyDeviceId);
    if (eidIt == eidMap.end()) {
        BM_LOG_ERROR("device_urma devPhyId " << phyDeviceId << " not found in EID file: " << eidFilePath
                                             << ", rankId=" << rankId);
        return BM_INVALID_PARAM;
    }
    eidData = eidIt->second;
    return 0;
}

} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock
