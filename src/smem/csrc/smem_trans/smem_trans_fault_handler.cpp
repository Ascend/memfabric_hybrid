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
#include "smem_trans_store_helper.h"
#include "mf_str_util.h"
#include "smem_trans_fault_handler.h"
namespace ock {
namespace smem {
SmemStoreFaultHandler &SmemStoreFaultHandler::GetInstance()
{
    static SmemStoreFaultHandler instance;
    return instance;
}

void SmemStoreFaultHandler::RegisterHandlerToStore(StorePtr store)
{
    if (store == nullptr) {
        return;
    }
    StoreManagerPtr managerPtr = Convert<ConfigStore, ConfigStoreManager>(store);
    SM_ASSERT_RET_VOID(managerPtr != nullptr);
    auto setHandler = [this](const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                             const StoreBackendPtr &backend) {
        return BuildLinkIdToRankInfoMap(linkId, key, value, backend);
    };
    managerPtr->RegisterServerOpHandler(MessageType::SET, setHandler);

    auto getHandler = [this](const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                             const StoreBackendPtr &backend) { return GetFromFaultInfo(linkId, key, value, backend); };
    managerPtr->RegisterServerOpHandler(MessageType::GET, getHandler);

    auto addHandler = [this](const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                             const StoreBackendPtr &backend) { return AddRankInfoMap(linkId, key, value, backend); };
    managerPtr->RegisterServerOpHandler(MessageType::ADD, addHandler);

    auto writeHandler = [this](const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                               const StoreBackendPtr &backend) {
        return WriteRankInfoMap(linkId, key, value, backend);
    };
    managerPtr->RegisterServerOpHandler(MessageType::WRITE, writeHandler);

    auto appendHandler = [this](const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                                const StoreBackendPtr &backend) {
        return AppendRankInfoMap(linkId, key, value, backend);
    };
    managerPtr->RegisterServerOpHandler(MessageType::APPEND, appendHandler);

    auto clearHandler = [this](const uint32_t linkId, StoreBackendPtr &backend) { ClearFaultInfo(linkId, backend); };
    managerPtr->RegisterServerBrokenHandler(clearHandler);
}

int32_t SmemStoreFaultHandler::BuildLinkIdToRankInfoMap(const uint32_t linkId, const std::string &key,
                                                        std::vector<uint8_t> &value, const StoreBackendPtr &backend)
{
    (void)backend;
    if (key.find(AUTO_RANK_KEY_PREFIX) != std::string::npos) {
        SM_ASSERT_RETURN(value.size() == 2, SM_ERROR); // rankId占2字节
        const uint16_t BIT_SHIFT = 8;
        uint16_t rankId = (static_cast<uint16_t>(value[0]) | (static_cast<uint16_t>(value[1]) << BIT_SHIFT));
        RankInfo info(rankId, key);
        linkIdToRankInfoMap_[linkId] = info;
        SM_LOG_INFO("build linkIdToRankInfoMap success, key" << key << ",linkId:" << linkId << ", rankId:" << rankId);
    }
    return SM_OK;
}

int32_t SmemStoreFaultHandler::AddRankInfoMap(const uint32_t linkId, const std::string &key,
                                              std::vector<uint8_t> &value, const StoreBackendPtr &backend)
{
    if (key.find(SENDER_COUNT_KEY) != std::string::npos || key.find(RECEIVER_COUNT_KEY) != std::string::npos) {
        auto it = linkIdToRankInfoMap_.find(linkId);
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(), "add rankInfo failed, not find key:" << key,
                           SM_INVALID_PARAM);
        it->second.dInfo.deviceCountKey = key;
        SM_LOG_INFO("add linkIdToRankInfoMap success, key" << key << ", linkId:" << linkId
                                                           << ", deviceCountKey:" << key);
        return SM_OK;
    }

    if (key.find(SENDER_TOTAL_SLICE_COUNT_KEY) != std::string::npos ||
        key.find(RECEIVER_TOTAL_SLICE_COUNT_KEY) != std::string::npos) {
        auto it = linkIdToRankInfoMap_.find(linkId);
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(), "add rankInfo failed, not find key:" << key,
                           SM_INVALID_PARAM);
        it->second.sInfo.sliceCountKey = key;
        SM_LOG_INFO("add linkIdToRankInfoMap success, key" << key << ", linkId:" << linkId
                                                           << ", sliceCountKey:" << key);
        return SM_OK;
    }
    return SM_OK;
}

int32_t SmemStoreFaultHandler::WriteRankInfoMap(const uint32_t linkId, const std::string &key,
                                                std::vector<uint8_t> &value, const StoreBackendPtr &backend)
{
    uint32_t offset = *(reinterpret_cast<uint32_t *>(value.data()));
    size_t realValSize = value.size() - sizeof(uint32_t);
    uint16_t index = offset / realValSize;

    if (key.find(SENDER_DEVICE_INFO_KEY) != std::string::npos ||
        key.find(RECEIVER_DEVICE_INFO_KEY) != std::string::npos) {
        auto it = linkIdToRankInfoMap_.find(linkId);
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(), "write rank info map failed, not find key:" << key,
                           SM_INVALID_PARAM);
        it->second.dInfo.deviceInfoId = index;
        it->second.dInfo.deiviceInfoUint = realValSize;
        it->second.dInfo.deviceInfoKey = key;
        SM_LOG_INFO("write linkIdToRankInfoMap success, key"
                    << key << ", linkId:" << linkId << ", deviceInfoKey:" << key << ", deviceInfoUint:" << realValSize
                    << ", deviceInfoId:" << index);
        return SM_OK;
    }
    // 同一个client会注册多个slice info
    if (key.find(SENDER_SLICES_INFO_KEY) != std::string::npos ||
        key.find(RECEIVER_SLICES_INFO_KEY) != std::string::npos) {
        auto it = linkIdToRankInfoMap_.find(linkId);
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(), "write rank info map failed, not find key:" << key,
                           SM_INVALID_PARAM);
        it->second.sInfo.sliceInfoId.push_back(index);
        it->second.sInfo.sliceInfoUint = realValSize;
        it->second.sInfo.sliceInfoKey = key;
        SM_LOG_INFO("write linkIdToRankInfoMap success, key"
                    << key << ", linkId:" << linkId << ", deviceCountKey:" << key << ", sliceInfoUint:" << realValSize
                    << ", sliceInfoId:" << index);
        return SM_OK;
    }

    return SM_OK;
}

int32_t SmemStoreFaultHandler::AppendRankInfoMap(const uint32_t linkId, const std::string &key,
                                                 std::vector<uint8_t> &value, const StoreBackendPtr &backend)
{
    std::vector<uint8_t> oldValue;
    auto ret = backend->Get(key, oldValue);
    SM_VALIDATE_RETURN(ret == SUCCESS, "kv store not find key:" << key, SM_INVALID_PARAM);
    uint16_t index = oldValue.size() / value.size() - 1;
    if (key.find(SENDER_DEVICE_INFO_KEY) != std::string::npos ||
        key.find(RECEIVER_DEVICE_INFO_KEY) != std::string::npos) {
        auto it = linkIdToRankInfoMap_.find(linkId);
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(), "append rank info map failed, not find key:" << key,
                           SM_INVALID_PARAM);
        it->second.dInfo.deviceInfoId = index;
        it->second.dInfo.deiviceInfoUint = value.size();
        it->second.dInfo.deviceInfoKey = key;
        SM_LOG_INFO("append linkIdToRankInfoMap success, key" << key << ",linkId:" << linkId << ", deviceInfoId:"
                                                              << index << ", deviceInfoUint:" << value.size());
        return SM_OK;
    }
    // 同一个client会注册多个slice info
    if (key.find(SENDER_SLICES_INFO_KEY) != std::string::npos ||
        key.find(RECEIVER_SLICES_INFO_KEY) != std::string::npos) {
        auto it = linkIdToRankInfoMap_.find(linkId);
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(), "append rank info map failed, not find key:" << key,
                           SM_INVALID_PARAM);
        it->second.sInfo.sliceInfoId.push_back(index);
        it->second.sInfo.sliceInfoUint = value.size();
        it->second.sInfo.sliceInfoKey = key;
        SM_LOG_INFO("append linkIdToRankInfoMap success, key"
                    << key << ",linkId:" << linkId << ", sliceInfoId:" << index << ", sliceInfoUint:" << value.size());
        return SM_OK;
    }

    return SM_OK;
}

int32_t SmemStoreFaultHandler::GetFromFaultInfo(const uint32_t linkId, const std::string &key,
                                                std::vector<uint8_t> &value, const StoreBackendPtr &backend)
{
    (void)backend;
    uint16_t id;
    if (!faultRankIdQueue_.empty() && key.find(AUTO_RANK_KEY_PREFIX) != std::string::npos) {
        id = faultRankIdQueue_.front();
        faultRankIdQueue_.pop();
    } else if (!faultDeviceIdQueue_.first.empty() && key.find(SENDER_GET_DEVICE_ID_KEY) != std::string::npos) {
        id = faultDeviceIdQueue_.first.front();
        faultDeviceIdQueue_.first.pop();
    } else if (!faultDeviceIdQueue_.second.empty() && key.find(RECEIVER_GET_DEVICE_ID_KEY) != std::string::npos) {
        id = faultDeviceIdQueue_.second.front();
        faultDeviceIdQueue_.second.pop();
    } else if (!faultSliceIdQueue_.first.empty() && key.find(SENDER_GET_SLICES_ID_KEY) != std::string::npos) {
        id = faultSliceIdQueue_.first.front();
        faultSliceIdQueue_.first.pop();
    } else if (!faultSliceIdQueue_.second.empty() && key.find(RECEIVER_GET_SLICES_ID_KEY) != std::string::npos) {
        id = faultSliceIdQueue_.second.front();
        faultSliceIdQueue_.second.pop();
    } else {
        return SM_OBJECT_NOT_EXISTS;
    }
    const size_t bitShift = 8;
    value.push_back(static_cast<uint8_t>(id & 0xff));
    value.push_back(static_cast<uint8_t>(id >> bitShift));
    return SM_GET_OBJIECT;
}

void SmemStoreFaultHandler::ClearFaultInfo(const uint32_t linkId, StoreBackendPtr &backend)
{
    auto linkIt = linkIdToRankInfoMap_.find(linkId);
    if (linkIt == linkIdToRankInfoMap_.end()) {
        return;
    }
    RankInfo &rankInfo = linkIt->second;
    ClearDeviceInfo(linkId, rankInfo, backend);
    ClearSliceInfo(linkId, rankInfo, backend);
    (void)backend->Delete(rankInfo.rankName);
    faultRankIdQueue_.push(rankInfo.rankId);
    linkIdToRankInfoMap_.erase(linkId);
}

void SmemStoreFaultHandler::ClearDeviceInfo(uint32_t linkId, RankInfo &rankInfo, StoreBackendPtr &backend)
{
    // 清理deviceInfo信息
    std::vector<uint8_t> oldValue;
    auto ret = backend->Get(rankInfo.dInfo.deviceInfoKey, oldValue);
    if (ret == SUCCESS) {
        auto &dInfoValue = oldValue;
        uint32_t offset = rankInfo.dInfo.deiviceInfoUint * rankInfo.dInfo.deviceInfoId;
        if (rankInfo.dInfo.deviceInfoKey.find(SENDER_DEVICE_INFO_KEY) != std::string::npos) {
            SM_LOG_DEBUG("add sender device id: " << rankInfo.dInfo.deviceInfoId
                                                  << ", deviceInfoKey:" << rankInfo.dInfo.deviceInfoKey);
            faultDeviceIdQueue_.first.push(rankInfo.dInfo.deviceInfoId);
        } else {
            SM_LOG_DEBUG("add receiver device id: " << rankInfo.dInfo.deviceInfoId
                                                    << ", deviceInfoKey:" << rankInfo.dInfo.deviceInfoKey);
            faultDeviceIdQueue_.second.push(rankInfo.dInfo.deviceInfoId);
        }
        SM_LOG_INFO("link broken, linkId: " << linkId << ", rankId: " << rankInfo.rankId
                                            << ", deviceInfoId: " << rankInfo.dInfo.deviceInfoId);
        dInfoValue[offset] = DataStatusType::ABNORMAL; // deviceInfo标记为异常, 正常节点client检测到异常状态会做清理
    }
    ret = backend->Get(rankInfo.dInfo.deviceCountKey, oldValue);
    if (ret == SUCCESS) {
        std::string valueStr{oldValue.begin(), oldValue.end()};
        long valueNum;
        bool isCovert = mf::StrUtil::String2Int<long>(valueStr, valueNum);
        if (isCovert) {
            valueNum--;
            std::string valueStrNew = std::to_string(valueNum);
            ret = backend->Put(rankInfo.dInfo.deviceCountKey,
                               std::vector<uint8_t>(valueStrNew.begin(), valueStrNew.end()), 0);
            SM_ASSERT_RET_VOID(ret == SUCCESS);
            SM_LOG_INFO("link broken, linkId: " << linkId << ", rankId: " << rankInfo.rankId
                                                << ", new device count: " << valueNum);
        } else {
            SM_LOG_ERROR("link broken, linkId: " << linkId << ", rankId: " << rankInfo.rankId << ", String2Int failed");
        }
    }
}

void SmemStoreFaultHandler::ClearSliceInfo(uint32_t linkId, RankInfo &rankInfo, StoreBackendPtr &backend)
{
    // 清理sliceInfo信息
    std::vector<uint8_t> oldValue;
    auto ret = backend->Get(rankInfo.sInfo.sliceInfoKey, oldValue);
    if (ret == SUCCESS) {
        auto &sInfoValue = oldValue;
        SM_LOG_INFO("link broken, linkId: " << linkId << ", rankId: " << rankInfo.rankId
                                            << ", sliceInfoId size: " << rankInfo.sInfo.sliceInfoId.size());
        for (auto id : rankInfo.sInfo.sliceInfoId) {
            uint32_t offset = rankInfo.sInfo.sliceInfoUint * id;
            sInfoValue[offset] = DataStatusType::ABNORMAL; // clientInfo标记为异常, 正常节点client检测到异常状态会做清理
            if (rankInfo.sInfo.sliceInfoKey.find(SENDER_SLICES_INFO_KEY) != std::string::npos) {
                SM_LOG_DEBUG("add sender slice id: " << id << ", sliceInfoKey:" << rankInfo.sInfo.sliceInfoKey);
                faultSliceIdQueue_.first.push(id);
            } else {
                SM_LOG_DEBUG("add receiver slice id: " << id << ", sliceInfoKey:" << rankInfo.sInfo.sliceInfoKey);
                faultSliceIdQueue_.second.push(id);
            }
        }
    }
    ret = backend->Get(rankInfo.sInfo.sliceCountKey, oldValue);
    if (ret == SUCCESS) {
        std::string valueStr{oldValue.begin(), oldValue.end()};
        long valueNum;
        bool isCovert = mf::StrUtil::String2Int<long>(valueStr, valueNum);
        if (isCovert) {
            valueNum -= rankInfo.sInfo.sliceInfoId.size();
            std::string valueStrNew = std::to_string(valueNum);
            ret = backend->Put(rankInfo.sInfo.sliceCountKey,
                               std::vector<uint8_t>(valueStrNew.begin(), valueStrNew.end()), 0);
            SM_ASSERT_RET_VOID(ret == SUCCESS);
            SM_LOG_INFO("link broken, linkId: " << linkId << ", rankId: " << rankInfo.rankId
                                                << ", new slice count: " << valueNum);
        } else {
            SM_LOG_ERROR("link broken, linkId: " << linkId << ", rankId: " << rankInfo.rankId << ", String2Int failed");
        }
    }
}

} // namespace smem
} // namespace ock