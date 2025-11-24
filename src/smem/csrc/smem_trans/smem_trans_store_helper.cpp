/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <algorithm>
#include "smem_store_factory.h"
#include "smem_trans_store_helper.h"

namespace ock {
namespace smem {
namespace {

const StoreKeys senderStoreKeys{SENDER_COUNT_KEY, SENDER_TOTAL_SLICE_COUNT_KEY, SENDER_DEVICE_INFO_KEY,
                                SENDER_SLICES_INFO_KEY};
const StoreKeys receiveStoreKeys{RECEIVER_COUNT_KEY, RECEIVER_TOTAL_SLICE_COUNT_KEY, RECEIVER_DEVICE_INFO_KEY,
                                 RECEIVER_SLICES_INFO_KEY};
}

SmemStoreHelper::SmemStoreHelper(std::string name, std::string storeUrl, smem_trans_role_t role) noexcept
    : name_{std::move(name)},
      storeURL_{std::move(storeUrl)},
      transRole_{role}
{
}

int SmemStoreHelper::Initialize(uint16_t entityId, int32_t maxRetry) noexcept
{
    if (transRole_ == SMEM_TRANS_SENDER) {
        localKeys_ = senderStoreKeys;
        remoteKeys_ = receiveStoreKeys;
    } else if (transRole_ == SMEM_TRANS_RECEIVER) {
        localKeys_ = receiveStoreKeys;
        remoteKeys_ = senderStoreKeys;
    } else {
        SM_LOG_ERROR("invalid role : " << transRole_);
        return SM_INVALID_PARAM;
    }

    if (urlExtraction_.ExtractIpPortFromUrl(storeURL_) != SM_OK) {
        SM_LOG_AND_SET_LAST_ERROR("invalid store url. ");
        return SM_INVALID_PARAM;
    }

    auto tmpStore = StoreFactory::CreateStore(urlExtraction_.ip, urlExtraction_.port, false, 0, maxRetry);
    SM_VALIDATE_RETURN(tmpStore != nullptr, "create store client with url failed.", SM_NEW_OBJECT_FAILED);
    store_ = StoreFactory::PrefixStore(tmpStore, std::string("/trans/").append(std::to_string(entityId).append("/")));
    SM_ASSERT_RETURN(store_ != nullptr, SM_ERROR);

    return SM_OK;
}

void SmemStoreHelper::Destroy() noexcept
{
    store_ = nullptr;
    StoreFactory::DestroyStore(urlExtraction_.ip, urlExtraction_.port);
}

void SmemStoreHelper::SetSliceExportSize(size_t sliceExportSize) noexcept
{
    sliceExpSize_ = sliceExportSize;
}

int SmemStoreHelper::CheckServerStatus() noexcept
{
    return store_->GetConnectStatus();
}

void SmemStoreHelper::AlterServerStatus(bool status) noexcept
{
    store_->SetConnectStatus(status);
}

int SmemStoreHelper::ReConnect() noexcept
{
    int reconnectRetryTimes = 3;
    if (store_->ReConnectAfterBroken(reconnectRetryTimes) != SM_OK) {
        return SM_ERROR;
    }
    return SM_RECONNECT;
}

void SmemStoreHelper::RegisterBrokenHandler(const ConfigStoreClientBrokenHandler &handler)
{
    store_->RegisterClientBrokenHandler(handler);
}

int SmemStoreHelper::RecoverRankInformation(std::vector<uint8_t> rankIdValue, uint16_t &rankId,
                                            const smem_trans_config_t &cfg, std::string key,
                                            bool &isRestore) noexcept
{
    const uint16_t BIT_SHIFT = 8;
    const size_t RANK_ID_SIZE = 2;
    uint16_t* dataPtr = reinterpret_cast<uint16_t *>(rankIdValue.data());
    rankId = *(dataPtr++);
    // 更新device sliceid
    remoteRankInfo_.isValid = true;
    remoteRankInfo_.deviceIdInRemote_ = *(dataPtr++);
    uint16_t item = rankIdValue.size() / sizeof(uint16_t) - 2U;
    SM_LOG_INFO("get info from server(" << name_ << ") rank id: " << rankId
                << ", device id: " << remoteRankInfo_.deviceIdInRemote_);
    std::queue<uint16_t> tmpQueue;
    remoteRankInfo_.sliceIdInRemote_.swap(tmpQueue);
    for (uint16_t i = 0; i < item; i++) {
        remoteRankInfo_.sliceIdInRemote_.emplace(*(dataPtr++));
        SM_LOG_INFO("get info from server(" << name_ << ") rank id: " << rankId
                    << ", slice id: " << remoteRankInfo_.sliceIdInRemote_.back());
    }
    std::vector<uint8_t> value((const uint8_t *)(const void *)&cfg,
                               (const uint8_t *)(const void *)&cfg + sizeof(cfg));
    
    auto ret = store_->Write(CLUSTER_RANKS_INFO_KEY, value, rankId * value.size());
    if (ret != SUCCESS) {
        SM_LOG_ERROR("write for key(" << key << ") failed: " << ret);
        return ret;
    }
    storeRankIdInfo_.first = rankId;
    storeRankIdInfo_.second = value;
    std::vector<uint8_t> rankIdValueNew(RANK_ID_SIZE);
    rankIdValueNew[0] = static_cast<uint8_t>(rankId & 0xff);
    rankIdValueNew[1] = static_cast<uint8_t>(rankId >> BIT_SHIFT);
    ret = store_->Set(key, rankIdValueNew);
    if (ret != SUCCESS) {
        SM_LOG_ERROR("set for key(" << key << ") failed: " << ret);
        return ret;
    }
    isRestore = true;
    SM_LOG_INFO("generate for engine(" << name_ << ") get rank: " << rankId);
    return SUCCESS;
}

int SmemStoreHelper::GenerateRankId(const smem_trans_config_t &cfg, uint16_t &rankId) noexcept
{
    const uint16_t BIT_SHIFT = 8;
    const size_t RANK_ID_SIZE = 2;
    std::string key = AUTO_RANK_KEY_PREFIX + name_;
    std::vector<uint8_t> rankIdValue(RANK_ID_SIZE);
    auto ret = store_->Get(key, rankIdValue, 0);
    bool isRestore = false;
    if (LIKELY(ret == NOT_EXIST)) {
        std::vector<uint8_t> value((const uint8_t *)(const void *)&cfg,
                                   (const uint8_t *)(const void *)&cfg + sizeof(cfg));
        uint64_t currentSize = 0;
        ret = store_->Append(CLUSTER_RANKS_INFO_KEY, value, currentSize);
        if (ret != SUCCESS) {
            SM_LOG_ERROR("append for key(" << CLUSTER_RANKS_INFO_KEY << ") failed: " << ret);
            return SM_ERROR;
        }

        rankId = static_cast<uint16_t>(currentSize / sizeof(cfg)) - 1U;
        storeRankIdInfo_.first = rankId;
        storeRankIdInfo_.second = value;
        rankIdValue[0] = static_cast<uint8_t>(rankId & 0xff);
        rankIdValue[1] = static_cast<uint8_t>(rankId >> BIT_SHIFT);
        ret = store_->Set(key, rankIdValue);
        if (ret != SUCCESS) {
            SM_LOG_ERROR("set for key(" << key << ") failed: " << ret);
            return SM_ERROR;
        }
        SM_LOG_INFO("generate for engine(" << name_ << ") get rank: " << rankId);
    } else if (ret == RESTORE) {
        ret = RecoverRankInformation(rankIdValue, rankId, cfg, key, isRestore);
        if (ret != SUCCESS) {
            return SM_ERROR;
        }
    }

    if (ret == SUCCESS) {
        if (rankIdValue.size() != RANK_ID_SIZE && !isRestore) {
            SM_LOG_ERROR("exist for key(" << key << ") value size = " << rankIdValue.size());
            return SM_ERROR;
        }

        rankId = (static_cast<uint16_t>(rankIdValue[0]) | (static_cast<uint16_t>(rankIdValue[1]) << BIT_SHIFT));
        SM_LOG_INFO("generate for engine(" << name_ << ") success, rank: " << rankId);
        return SM_OK;
    }

    SM_LOG_ERROR("get for key(" << key << ") failed: " << ret);
    return SM_ERROR;
}

int SmemStoreHelper::StoreDeviceInfo(const hybm_exchange_info &info) noexcept
{
    int64_t totalValue = 0;
    uint64_t totalSize = 0;
    deviceExpSize_ = info.descLen;
    std::vector<uint8_t> value(1 + info.descLen); // data status + data
    value[0] = DataStatusType::NORMAL;
    std::copy_n(info.desc, info.descLen, value.data() + 1);
    Result ret;
    if (!remoteRankInfo_.isValid) {
        SM_LOG_DEBUG("begin append(key=" << localKeys_.deviceInfo << ", value_size=" << value.size() << ")");
        ret = store_->Append(localKeys_.deviceInfo, value, totalSize);
        if (ret != 0) {
            SM_LOG_ERROR("store append device info for sender failed: " << ret);
            return SM_ERROR;
        }
        uint16_t offset = totalSize / value.size() - 1;
        storeDeviceInfo_.first = offset;
        storeDeviceInfo_.second = value;
    } else {
        // 理论上只进一次
        SM_LOG_DEBUG("begin write(key=" << localKeys_.deviceInfo << ", value_size=" << value.size()
            << ", device_id=" << remoteRankInfo_.deviceIdInRemote_ << ")");
        uint32_t offset = remoteRankInfo_.deviceIdInRemote_ * value.size();
        ret = store_->Write(localKeys_.deviceInfo, value, offset);
        if (ret != 0) {
            SM_LOG_ERROR("store write device info for sender failed: " << ret);
            return SM_ERROR;
        }
        storeDeviceInfo_.first = remoteRankInfo_.deviceIdInRemote_;
        storeDeviceInfo_.second = value;
        remoteRankInfo_.isValid = false;
    }

    ret = store_->Add(localKeys_.deviceCount, 1L, totalValue);
    if (ret != 0) {
        SM_LOG_ERROR("store add sender count failed: " << ret);
        return SM_ERROR;
    }

    return SM_OK;
}

int SmemStoreHelper::ReStoreDeviceInfo() noexcept
{
    int64_t totalValue = 0;
    SM_LOG_INFO("begin recover device info, size = " << storeDeviceInfo_.second.size()
                << ", key = " << localKeys_.deviceInfo);
    uint32_t offset = storeDeviceInfo_.first * storeDeviceInfo_.second.size();
    auto ret = store_->Write(localKeys_.deviceInfo, storeDeviceInfo_.second, offset);
    if (ret != 0) {
        SM_LOG_ERROR("store recover device info for sender failed: " << ret);
        return SM_ERROR;
    }

    ret = store_->Add(localKeys_.deviceCount, 1L, totalValue);
    if (ret != 0) {
        SM_LOG_ERROR("store add sender count failed: " << ret);
        return SM_ERROR;
    }

    return SM_OK;
}

int SmemStoreHelper::StoreSliceInfo(const hybm_exchange_info &info, const StoredSliceInfo &sliceInfo) noexcept
{
    Result ret;
    std::vector<uint8_t> value(1 + sizeof(sliceInfo) + info.descLen); // data status + data
    value[0] = DataStatusType::NORMAL; // default is normal
    uint32_t offset = 1;
    std::copy_n(reinterpret_cast<const uint8_t *>(&sliceInfo), sizeof(sliceInfo), value.data() + offset);
    offset += sizeof(sliceInfo);
    std::copy_n(info.desc, info.descLen, value.data() + offset);
    if (remoteRankInfo_.sliceIdInRemote_.empty()) {
        uint64_t totalSize = 0;
        SM_LOG_DEBUG("begin append(key=" << localKeys_.sliceInfo << ", value_size=" << value.size() << ")");
        ret = store_->Append(localKeys_.sliceInfo, value, totalSize);

        uint16_t valueOffset = totalSize / value.size() - 1;
        storeSliceInfo_.emplace_back(valueOffset, value);
        if (ret != 0) {
            SM_LOG_ERROR("store append slice info failed: " << ret);
            return SM_ERROR;
        }
        SM_LOG_DEBUG("success append(key=" << localKeys_.sliceCount << ", value_size=" << value.size()
                                        << "), total_size=" << totalSize);
    } else {
        uint16_t sliceId = remoteRankInfo_.sliceIdInRemote_.front();
        remoteRankInfo_.sliceIdInRemote_.pop();
        SM_LOG_DEBUG("begin write(key=" << localKeys_.sliceInfo << ", value_size=" << value.size()
            << ", slice_id=" << sliceId << ")");
        ret =store_->Write(localKeys_.sliceInfo, value, sliceId * value.size());
        if (ret != 0) {
            SM_LOG_ERROR("store write slice info failed: " << ret);
            return SM_ERROR;
        }
        storeSliceInfo_.emplace_back(sliceId, value);
    }
    int64_t nowCount = 0;
    ret = store_->Add(localKeys_.sliceCount, 1L, nowCount);
    if (ret != 0) {
        SM_LOG_ERROR("store add count for slice info failed: " << ret);
        return SM_ERROR;
    }

    SM_LOG_DEBUG("now slice total count = " << nowCount);
    return SM_OK;
}

int SmemStoreHelper::ReStoreSliceInfo() noexcept
{
    int64_t totalValue = 0;
    SM_LOG_INFO("begin recover slice info, size = " << storeSliceInfo_.size() << ", key = " << localKeys_.sliceInfo);
    for (auto &singleInfo: storeSliceInfo_) {
        uint32_t offset = singleInfo.first * singleInfo.second.size();
        auto ret = store_->Write(localKeys_.sliceInfo, singleInfo.second, offset);
        if (ret != 0) {
            SM_LOG_ERROR("store recover slice info failed: " << ret);
            return SM_ERROR;
        }

        ret = store_->Add(localKeys_.sliceCount, 1L, totalValue);
        if (ret != 0) {
            SM_LOG_ERROR("store add count for slice info failed: " << ret);
            return SM_ERROR;
        }
    }

    return SM_OK;
}

void SmemStoreHelper::FindNewRemoteRanks(const FindRanksCbFunc &cb) noexcept
{
    SM_ASSERT_RET_VOID(deviceExpSize_ != 0);

    std::vector<uint8_t> values;
    int64_t totalValue = 0;
    auto ret = store_->Add(remoteKeys_.deviceCount, 0L, totalValue);
    if (ret != 0) {
        SM_LOG_ERROR("store add(0) for key(" << remoteKeys_.deviceCount << ") count failed: " << ret);
        return;
    }
    if (totalValue == 0 && remoteDeviceInfoLastTime_.size() == 0) {
        SM_LOG_DEBUG("remote device count is 0, local device count is 0, no need to find new device");
        return;
    }
    ret = store_->Get(remoteKeys_.deviceInfo, values);
    if (ret != 0) {
        SM_LOG_ERROR("store get devices info with key(" << remoteKeys_.deviceInfo << ") failed: " << ret);
        return;
    }
    SM_LOG_DEBUG("FindNewRemoteRanks deal key(" << remoteKeys_.deviceInfo
                << ", role: " << transRole_ << ", remote device info size:" << values.size()
                << ", last local device info size:" << remoteDeviceInfoLastTime_.size());

    std::vector<hybm_exchange_info> addInfo;
    ExtraDeviceChangeInfo(values, addInfo);
    ret = cb(addInfo);
    if (ret != 0) {
        SM_LOG_ERROR("find new ranks callback failed: " << ret);
        return;
    }
}

void SmemStoreHelper::FindNewRemoteSlices(const FindSlicesCbFunc &cb) noexcept
{
    SM_ASSERT_RET_VOID(sliceExpSize_ != 0);
    std::vector<uint8_t> values;
    int64_t totalValue = 0;
    auto ret = store_->Add(remoteKeys_.sliceCount, 0L, totalValue);
    if (ret != 0) {
        SM_LOG_ERROR("store add(0) for key(" << remoteKeys_.sliceCount << ") total count failed: " << ret);
        return;
    }
    if (totalValue == 0 && remoteSlicesInfoLastTime_.size() == 0) {
        SM_LOG_DEBUG("remote slice count is 0, local slice count is 0, no need to find new slices");
        return;
    }
    ret = store_->Get(remoteKeys_.sliceInfo, values);
    if (ret != 0) {
        SM_LOG_ERROR("store get for key(" << remoteKeys_.sliceInfo << ") all slices failed: " << ret);
        return;
    }
    std::vector<hybm_exchange_info> addInfo;
    std::vector<const StoredSliceInfo *> addStoreSs;
    std::vector<const StoredSliceInfo *> removeStoreSs;
    ExtraSliceChangeInfo(values, addInfo, addStoreSs, removeStoreSs);
    SM_LOG_DEBUG("FindNewRemoteSlices deal key(" << remoteKeys_.sliceInfo
                << ", role: " << transRole_ << ", remote slice info size:" <<
                values.size() << ", last local slice info size:" << remoteSlicesInfoLastTime_.size());
    ret = cb(addInfo, addStoreSs, removeStoreSs);
    if (ret != 0) {
        SM_LOG_ERROR("find new slices callback failed: " << ret);
        return;
    }
}

void SmemStoreHelper::CompareAndUpdateDeviceInfo(uint32_t minCount, std::vector<uint8_t> &values,
                                                 std::vector<hybm_exchange_info> &addInfo) noexcept
{
    // client故障：newCount >= curCount
    for (uint32_t i = 0; i < minCount; i++) {
        uint8_t *curPtr = (remoteDeviceInfoLastTime_.data() + i * (deviceExpSize_ + 1));
        uint8_t *newPtr = (values.data() + i * (deviceExpSize_ + 1));
        uint8_t *curDataPtr = curPtr + 1;
        uint8_t *newDataPtr = newPtr + 1;
        if (*curPtr == DataStatusType::NORMAL && *newPtr == DataStatusType::NORMAL) {
            if (memcmp(curDataPtr,  newDataPtr, deviceExpSize_) != 0) {
                // 有卡重新拉起注册到server
                SM_LOG_INFO("local device is normal, remote device is normal, i=" << i
                             << " but data is not same, will do add");
                hybm_exchange_info info;
                info.descLen = deviceExpSize_;
                std::copy_n(newDataPtr, deviceExpSize_, info.desc);
                std::copy_n(newDataPtr, deviceExpSize_, curDataPtr);
                addInfo.emplace_back(std::move(info));
            }
        } else if (*curPtr == DataStatusType::NORMAL && *newPtr != DataStatusType::NORMAL) {
            // 有卡故障
            SM_LOG_INFO("local device is normal, remote device is not normal, i=" << i << " will do remove");
            hybm_exchange_info info;
            info.descLen = deviceExpSize_;
            std::copy_n(curDataPtr, deviceExpSize_, info.desc);
            *curPtr = DataStatusType::ABNORMAL;
        } else if (*curPtr != DataStatusType::NORMAL && *newPtr == DataStatusType::NORMAL) {
            // 故障卡重新拉起注册到server
            SM_LOG_INFO("local device is not normal, remote device is normal, i=" << i << " will do add");
            hybm_exchange_info info;
            info.descLen = deviceExpSize_;
            std::copy_n(newDataPtr, deviceExpSize_, info.desc);
            *curPtr = DataStatusType::NORMAL;
            std::copy_n(newDataPtr, deviceExpSize_, curDataPtr);
            addInfo.emplace_back(std::move(info));
        }
        // 其他情况不处理
    }
}

void SmemStoreHelper::ExtraDeviceChangeInfo(std::vector<uint8_t> &values,
                                            std::vector<hybm_exchange_info> &addInfo) noexcept
{
    if (remoteDeviceInfoLastTime_ == values) {
        SM_LOG_DEBUG("remote device info has no change, remoteDeviceInfoLastTime_ size="
                     << remoteDeviceInfoLastTime_.size());
        return; // 无变化
    }
    uint32_t newCount = values.size() / (deviceExpSize_ + 1); // 有一位为标志位，表示数据状态
    uint32_t curCount = remoteDeviceInfoLastTime_.size() / (deviceExpSize_ + 1);
    uint32_t minCount = std::min(newCount, curCount);
    SM_LOG_INFO("ExtraDeviceChangeInfo newCount= " << newCount << ", curCount=" << curCount
                << ", minCount=" << minCount);
    CompareAndUpdateDeviceInfo(minCount, values, addInfo);

    if (newCount > curCount) {
        // 扩容新增client：newCount > curCount
        remoteDeviceInfoLastTime_.resize(values.size(), 0);
        for (uint32_t i = minCount; i < newCount; i++) {
            uint8_t *curPtr = (remoteDeviceInfoLastTime_.data() + i * (deviceExpSize_ + 1));
            uint8_t *newPtr = (values.data() + i * (deviceExpSize_ + 1));
            uint8_t *curDataPtr = curPtr + 1;
            uint8_t *newDataPtr = newPtr + 1;
            if (*newPtr == DataStatusType::NORMAL) {
                // 新增client
                SM_LOG_DEBUG("local device is none, remote device is normal,  i=" << i << " will do add");
                hybm_exchange_info info;
                info.descLen = deviceExpSize_;
                std::copy_n(newDataPtr, deviceExpSize_, info.desc);
                *curPtr = DataStatusType::NORMAL;
                std::copy_n(newDataPtr, deviceExpSize_, curDataPtr);
                addInfo.emplace_back(std::move(info));
            } else {
                *curPtr = DataStatusType::ABNORMAL;
                std::copy_n(newDataPtr, deviceExpSize_, curDataPtr);
            }
        }
    }
    // server重拉：newCount > curCount不清理本地缓存
}

void SmemStoreHelper::CompareAndUpdateSliceInfo(uint32_t minCount, std::vector<uint8_t> &values,
                                                std::vector<hybm_exchange_info> &addInfo,
                                                std::vector<const StoredSliceInfo *> &addStoreSs,
                                                std::vector<const StoredSliceInfo *> &removeStoreSs) noexcept
{
    // client故障：newCount >= curCount
    for (uint32_t i = 0; i < minCount; i++) {
        uint8_t *curPtr = (remoteSlicesInfoLastTime_.data() + i * (sliceExpSize_ + sizeof(StoredSliceInfo) + 1));
        uint8_t *newPtr = (values.data() + i * (sliceExpSize_ + sizeof(StoredSliceInfo) + 1));
        uint8_t *curDataPtr = curPtr + 1;
        uint8_t *newDataPtr = newPtr + 1;
        if (*curPtr == DataStatusType::NORMAL && *newPtr == DataStatusType::NORMAL) {
            if (memcmp(curDataPtr,  newDataPtr, sliceExpSize_ + sizeof(StoredSliceInfo)) != 0) {
                // 有卡重新拉起注册到server
                SM_LOG_INFO("local slice is normal, remote slice is normal, i=" << i
                             << " but data is not same, will do add");
                addStoreSs.emplace_back((const StoredSliceInfo *)(const void *)newDataPtr);
                hybm_exchange_info info;
                info.descLen = sliceExpSize_;
                std::copy_n(newDataPtr + sizeof(StoredSliceInfo), sliceExpSize_, info.desc);
                std::copy_n(newDataPtr, sliceExpSize_ + sizeof(StoredSliceInfo), curDataPtr);
                addInfo.emplace_back(std::move(info));
            }
        } else if (*curPtr == DataStatusType::NORMAL && *newPtr != DataStatusType::NORMAL) {
            // 有卡故障
            SM_LOG_INFO("local slice is normal, remote slice is not normal, i=" << i << " will do remove");
            removeStoreSs.emplace_back((const StoredSliceInfo *)(const void *)curDataPtr);
            *curPtr = DataStatusType::ABNORMAL;
        } else if (*curPtr != DataStatusType::NORMAL && *newPtr == DataStatusType::NORMAL) {
            // 故障卡重新拉起注册到server
            SM_LOG_INFO("local slice is not normal, remote slice is normal, i=" << i << " will do add");
            addStoreSs.emplace_back((const StoredSliceInfo *)(const void *)newDataPtr);
            hybm_exchange_info info;
            info.descLen = sliceExpSize_;
            std::copy_n(newDataPtr + sizeof(StoredSliceInfo), sliceExpSize_, info.desc);
            *curPtr = DataStatusType::NORMAL;
            std::copy_n(newDataPtr, sliceExpSize_ + sizeof(StoredSliceInfo), curDataPtr);
            addInfo.emplace_back(std::move(info));
        }
    }
}

void SmemStoreHelper::ExtraSliceChangeInfo(std::vector<uint8_t> &values,
                                           std::vector<hybm_exchange_info> &addInfo,
                                           std::vector<const StoredSliceInfo *> &addStoreSs,
                                           std::vector<const StoredSliceInfo *> &removeStoreSs) noexcept
{
    if (remoteSlicesInfoLastTime_ == values) {
        SM_LOG_DEBUG("remote slice info has no change, remoteSlicesInfoLastTime_ szie="
                     << remoteSlicesInfoLastTime_.size());
        return; // 无变化
    }
    uint32_t newCount = values.size() / (sliceExpSize_ + sizeof(StoredSliceInfo) + 1); // 有一位为标志位，表示数据状态
    uint32_t curCount = remoteSlicesInfoLastTime_.size() / (sliceExpSize_ + sizeof(StoredSliceInfo) + 1);
    uint32_t minCount = std::min(newCount, curCount);
    SM_LOG_INFO("ExtraSliceChangeInfo newCount= " << newCount << ", curCount=" << curCount
                << ", minCount=" << minCount);
    // client故障：newCount >= curCount
    CompareAndUpdateSliceInfo(minCount, values, addInfo, addStoreSs, removeStoreSs);
    // 新增client：newCount > curCount
    if (newCount > curCount) {
        remoteSlicesInfoLastTime_.resize(values.size(), 0);
        for (uint32_t i = minCount; i < newCount; i++) {
            uint8_t *curPtr = (remoteSlicesInfoLastTime_.data() + i * (sliceExpSize_ + sizeof(StoredSliceInfo) + 1));
            uint8_t *newPtr = (values.data() + i * (sliceExpSize_ + sizeof(StoredSliceInfo) + 1));
            uint8_t *curDataPtr = curPtr + 1;
            uint8_t *newDataPtr = newPtr + 1;
            if (*newPtr == DataStatusType::NORMAL) {
                // 新增client
                SM_LOG_DEBUG("local slice is none, remote slice is normal, i=" << i << " will do add");
                addStoreSs.emplace_back((const StoredSliceInfo *)(const void *)newDataPtr);
                hybm_exchange_info info;
                info.descLen = sliceExpSize_;
                std::copy_n(newDataPtr + sizeof(StoredSliceInfo), sliceExpSize_, info.desc);
                *curPtr = DataStatusType::NORMAL;
                std::copy_n(newDataPtr, sliceExpSize_ + sizeof(StoredSliceInfo), curDataPtr);
                addInfo.emplace_back(std::move(info));
            } else {
                *curPtr = DataStatusType::ABNORMAL;
                std::copy_n(newDataPtr, sliceExpSize_ + sizeof(StoredSliceInfo), curDataPtr);
            }
        }
    }
    // server重拉：newCount > curCount不清理本地缓存
}

int SmemStoreHelper::ReRegisterToServer(uint16_t rankId) noexcept
{
    const uint16_t BIT_SHIFT = 8;
    const size_t RANK_ID_SIZE = 2;
    std::string key = AUTO_RANK_KEY_PREFIX + name_;
    std::vector<uint8_t> rankIdValue(RANK_ID_SIZE);
    auto ret = store_->Get(key, rankIdValue, 0);
    if (LIKELY(ret == NOT_EXIST)) {
        uint32_t offset = storeRankIdInfo_.second.size() * storeRankIdInfo_.first;
        SM_LOG_INFO("ReRegisterToServer storeRankIdInfo_ size:" << storeRankIdInfo_.second.size()
                    << ", offset:" << offset);
        ret = store_->Write(CLUSTER_RANKS_INFO_KEY, storeRankIdInfo_.second, offset);
        if (ret != SUCCESS) {
            SM_LOG_ERROR("recover for key(" << CLUSTER_RANKS_INFO_KEY << ") failed: " << ret);
            return SM_ERROR;
        }
        rankIdValue[0] = static_cast<uint8_t>(rankId & 0xff);
        rankIdValue[1] = static_cast<uint8_t>(rankId >> BIT_SHIFT);
        ret = store_->Set(key, rankIdValue);
        if (ret != SUCCESS) {
            SM_LOG_ERROR("set for key(" << key << ") failed: " << ret);
            return SM_ERROR;
        }
        SM_LOG_INFO("generate for engine(" << name_ << ") get rank: " << rankId);
    }
    if (ret == SUCCESS) {
        if (rankIdValue.size() != RANK_ID_SIZE) {
            SM_LOG_ERROR("exist for key(" << key << ") value size = " << rankIdValue.size());
            return SM_ERROR;
        }
        return SM_OK;
    }

    SM_LOG_ERROR("recover for key(" << key << ") failed: " << ret);
    return SM_ERROR;
}
}
}