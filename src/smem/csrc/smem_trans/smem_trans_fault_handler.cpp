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
SmemStoreFaultHandler& SmemStoreFaultHandler::GetInstance()
{
    static SmemStoreFaultHandler instance;
    return instance;
}

void SmemStoreFaultHandler::RegisterHandlerToStore(StorePtr store)
{
    if (store == nullptr) {
        return;
    }
    auto setHandler = [this] (const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                              const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore) {
        return BuildLinkIdToRankInfoMap(linkId, key, value, kvStore);
    };
    store->RegisterServerOpHandler(MessageType::SET, setHandler);

    auto getHandler = [this] (const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                              const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore) {
        return GetFromFaultInfo(linkId, key, value, kvStore);
    };
    store->RegisterServerOpHandler(MessageType::GET, getHandler);

    auto addHandler = [this] (const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                              const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore) {
        return AddRankInfoMap(linkId, key, value, kvStore);
    };
    store->RegisterServerOpHandler(MessageType::ADD, addHandler);

    auto writeHandler = [this] (const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                                const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore) {
        return WriteRankInfoMap(linkId, key, value, kvStore);
    };
    store->RegisterServerOpHandler(MessageType::WRITE, writeHandler);

    auto appendHandler = [this] (const uint32_t linkId, const std::string &key, std::vector<uint8_t> &value,
                                 const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore) {
        return AppendRankInfoMap(linkId, key, value, kvStore);
    };
    store->RegisterServerOpHandler(MessageType::APPEND, appendHandler);

    auto clearHandler = [this] (const uint32_t linkId,
                                std::unordered_map<std::string, std::vector<uint8_t>> &kvStore) {
        ClearFaultInfo(linkId, kvStore);
    };
    store->RegisterServerBrokenHandler(clearHandler);
}

int32_t SmemStoreFaultHandler::BuildLinkIdToRankInfoMap(const uint32_t linkId, const std::string &key,
                                                        std::vector<uint8_t> &value,
                                                        const std::unordered_map<std::string,
                                                        std::vector<uint8_t>> &kvStore)
{
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
                                              std::vector<uint8_t> &value,
                                              const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore)
{
    if (key.find(SENDER_COUNT_KEY) != std::string::npos ||
        key.find(RECEIVER_COUNT_KEY) != std::string::npos) {
        auto it = linkIdToRankInfoMap_.find(linkId);
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(),
                           "add rankInfo failed, not find key:" << key, SM_INVALID_PARAM);
        it->second.dInfo.deviceCountKey = key;
        SM_LOG_INFO("add linkIdToRankInfoMap success, key" << key << ", linkId:" << linkId
                    << ", deviceCountKey:" << key);
        return SM_OK;
    }
    
    if (key.find(SENDER_TOTAL_SLICE_COUNT_KEY) != std::string::npos ||
        key.find(RECEIVER_TOTAL_SLICE_COUNT_KEY) != std::string::npos) {
        auto it = linkIdToRankInfoMap_.find(linkId);
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(),
                           "add rankInfo failed, not find key:" << key, SM_INVALID_PARAM);
        it->second.sInfo.sliceCountKey = key;
        SM_LOG_INFO("add linkIdToRankInfoMap success, key" << key << ", linkId:" << linkId
                    << ", sliceCountKey:" << key);
        return SM_OK;
    }
    return SM_OK;
}

int32_t SmemStoreFaultHandler::WriteRankInfoMap(const uint32_t linkId, const std::string &key,
                                                std::vector<uint8_t> &value,
                                                const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore)
{
    uint32_t offset = *(reinterpret_cast<uint32_t *>(value.data()));
    size_t realValSize = value.size() - sizeof(uint32_t);
    uint16_t index = offset / realValSize;
   
    if (key.find(SENDER_DEVICE_INFO_KEY) != std::string::npos ||
        key.find(RECEIVER_DEVICE_INFO_KEY) != std::string::npos) {
        auto it = linkIdToRankInfoMap_.find(linkId);
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(),
                           "write rank info map failed, not find key:" << key, SM_INVALID_PARAM);
        it->second.dInfo.deviceInfoId = index;
        it->second.dInfo.deiviceInfoUint = realValSize;
        it->second.dInfo.deviceInfoKey = key;
        SM_LOG_INFO("write linkIdToRankInfoMap success, key" << key << ", linkId:" << linkId << ", deviceInfoKey:"
            << key << ", deviceInfoUint:" << realValSize << ", deviceInfoId:" << index);
        return SM_OK;
    }
    // 同一个client会注册多个slice info
    if (key.find(SENDER_SLICES_INFO_KEY) != std::string::npos ||
        key.find(RECEIVER_SLICES_INFO_KEY) != std::string::npos) {
        auto it = linkIdToRankInfoMap_.find(linkId);
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(),
                           "write rank info map failed, not find key:" << key, SM_INVALID_PARAM);
        it->second.sInfo.sliceInfoId.push_back(index);
        it->second.sInfo.sliceInfoUint = realValSize;
        it->second.sInfo.sliceInfoKey = key;
        SM_LOG_INFO("write linkIdToRankInfoMap success, key" << key << ", linkId:" << linkId << ", deviceCountKey:"
            << key << ", sliceInfoUint:" << realValSize << ", sliceInfoId:" << index);
        return SM_OK;
    }
    
    return SM_OK;
}

int32_t SmemStoreFaultHandler::AppendRankInfoMap(const uint32_t linkId, const std::string &key,
                                                 std::vector<uint8_t> &value,
                                                 const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore)
{
    auto pos = kvStore.find(key);
    SM_VALIDATE_RETURN(pos != kvStore.end(), "kv store not find key:" << key, SM_INVALID_PARAM);
    uint16_t index = pos->second.size() / value.size() - 1;
    if (key.find(SENDER_DEVICE_INFO_KEY) != std::string::npos ||
        key.find(RECEIVER_DEVICE_INFO_KEY) != std::string::npos) {
        auto it = linkIdToRankInfoMap_.find(linkId);
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(),
                           "append rank info map failed, not find key:" << key, SM_INVALID_PARAM);
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
        SM_VALIDATE_RETURN(it != linkIdToRankInfoMap_.end(),
                           "append rank info map failed, not find key:" << key, SM_INVALID_PARAM);
        it->second.sInfo.sliceInfoId.push_back(index);
        it->second.sInfo.sliceInfoUint = value.size();
        it->second.sInfo.sliceInfoKey = key;
        SM_LOG_INFO("append linkIdToRankInfoMap success, key" << key << ",linkId:" << linkId << ", sliceInfoId:"
            << index << ", sliceInfoUint:" << value.size());
        return SM_OK;
    }
    
    return SM_OK;
}

int32_t SmemStoreFaultHandler::GetFromFaultInfo(const uint32_t linkId, const std::string &key,
                                                std::vector<uint8_t> &value,
                                                const std::unordered_map<std::string, std::vector<uint8_t>> &kvStore)
{
    if (faultRankIndexQueue_.empty()) {
        return SM_OBJECT_NOT_EXISTS;
    }
    if (key.find(AUTO_RANK_KEY_PREFIX) == std::string::npos) {
        return SM_OBJECT_NOT_EXISTS;
    }
    // 适配多个slice
    ServerFaultRankIndex &faultRankIndex = faultRankIndexQueue_.front();
    uint8_t *data = reinterpret_cast<uint8_t*>(&faultRankIndex);
    value.clear();
    value.insert(value.end(), data, data + sizeof(faultRankIndex.rankId) + sizeof(faultRankIndex.deviceInfoId));
    uint8_t *sliceData = reinterpret_cast<uint8_t*>(faultRankIndex.sliceInfoIdVec.data());
    uint32_t byteSize = sizeof(uint16_t) * faultRankIndex.sliceInfoIdVec.size();
    value.insert(value.end(), sliceData, sliceData + byteSize);
    faultRankIndexQueue_.pop();
    return SM_GET_OBJIECT;
}

void SmemStoreFaultHandler::ClearFaultInfo(const uint32_t linkId,
                                           std::unordered_map<std::string, std::vector<uint8_t>> &kvStore)
{
    auto linkIt = linkIdToRankInfoMap_.find(linkId);
    if (linkIt == linkIdToRankInfoMap_.end()) {
        return;
    }
    auto& rankInfo = linkIt->second;
    kvStore.erase(rankInfo.rankName);
    // 清理deviceInfo信息
    auto dInfoIt = kvStore.find(rankInfo.dInfo.deviceInfoKey);
    if (dInfoIt != kvStore.end()) {
        auto& dInfoValue = dInfoIt->second;
        uint32_t offset = rankInfo.dInfo.deiviceInfoUint * rankInfo.dInfo.deviceInfoId;
        SM_LOG_INFO("link broken, linkId: " << linkId << ", rankId: " << rankInfo.rankId
                    << ", deviceInfoId: " << rankInfo.dInfo.deviceInfoId);
        dInfoValue[offset] = DataStatusType::ABNORMAL; // deviceInfo标记为异常, 正常节点client检测到异常状态会做清理
    }
    auto dCntIt = kvStore.find(rankInfo.dInfo.deviceCountKey);
    if (dCntIt != kvStore.end()) {
        std::string valueStr{dCntIt->second.begin(), dCntIt->second.end()};
        long valueNum = 0;
        mf::StrUtil::String2Int(valueStr, valueNum);
        valueNum--;
        std::string valueStrNew = std::to_string(valueNum);
        dCntIt->second = std::vector<uint8_t>(valueStrNew.begin(), valueStrNew.end());
        SM_LOG_INFO("link broken, linkId: " << linkId << ", rankId: " << rankInfo.rankId
                    << ", new device count: " << valueNum);
    }
    // 清理sliceInfo信息
    auto sInfoIt = kvStore.find(rankInfo.sInfo.sliceInfoKey);
    if (sInfoIt != kvStore.end()) {
        auto& sInfoValue = sInfoIt->second;
        SM_LOG_INFO("link broken, linkId: " << linkId << ", rankId: " << rankInfo.rankId
                    << ", sliceInfoId size: " << rankInfo.sInfo.sliceInfoId.size());
        for (auto sliceInfo: rankInfo.sInfo.sliceInfoId) {
            uint32_t offset = rankInfo.sInfo.sliceInfoUint * sliceInfo;
            sInfoValue[offset] = DataStatusType::ABNORMAL; // clientInfo标记为异常, 正常节点client检测到异常状态会做清理
        }
    }
    auto sCntIt = kvStore.find(rankInfo.sInfo.sliceCountKey);
    if (sCntIt != kvStore.end()) {
        std::string valueStr{sCntIt->second.begin(), sCntIt->second.end()};
        long valueNum = 0;
        mf::StrUtil::String2Int(valueStr, valueNum);
        valueNum -=rankInfo.sInfo.sliceInfoId.size();
        std::string valueStrNew = std::to_string(valueNum);
        sCntIt->second = std::vector<uint8_t>(valueStrNew.begin(), valueStrNew.end());
        SM_LOG_INFO("link broken, linkId: " << linkId << ", rankId: " << rankInfo.rankId
                    << ", new slice count: " << valueNum);
    }
    faultRankIndexQueue_.push({rankInfo.rankId, rankInfo.dInfo.deviceInfoId, rankInfo.sInfo.sliceInfoId});
    linkIdToRankInfoMap_.erase(linkId);
}

}
}