/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <algorithm>
#include "smem_store_factory.h"
#include "smem_trans_store_helper.h"

namespace ock {
namespace smem {
namespace {
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
    remoteSliceLastTime_ = 0;
    remoteRankLastTime_ = 0;

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

int SmemStoreHelper::GenerateRankId(const smem_trans_config_t &cfg, uint16_t &rankId) noexcept
{
    std::string key = AUTO_RANK_KEY_PREFIX + name_;
    std::vector<uint8_t> rankIdValue(2);
    auto ret = store_->Get(key, rankIdValue, 0);
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
        rankIdValue[0] = static_cast<uint8_t>(rankId & 0xff);
        rankIdValue[1] = static_cast<uint8_t>(rankId >> 8);
        ret = store_->Set(key, rankIdValue);
        if (ret != SUCCESS) {
            SM_LOG_ERROR("set for key(" << key << ") failed: " << ret);
            return SM_ERROR;
        }
        SM_LOG_INFO("generate for engine(" << name_ << ") get rank: " << rankId);
    }

    if (ret == SUCCESS) {
        if (rankIdValue.size() != 2) {
            SM_LOG_ERROR("exist for key(" << key << ") value size = " << rankIdValue.size());
            return SM_ERROR;
        }

        rankId = (static_cast<uint16_t>(rankIdValue[0]) | (static_cast<uint16_t>(rankIdValue[1]) << 8));
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
    std::vector<uint8_t> value(info.desc, info.desc + info.descLen);
    auto ret = store_->Append(localKeys_.deviceInfo, value, totalSize);
    if (ret != 0) {
        SM_LOG_ERROR("store append device info for sender failed: " << ret);
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
    std::vector<uint8_t> value(sizeof(sliceInfo) + info.descLen);
    std::copy_n(reinterpret_cast<const uint8_t *>(&sliceInfo), sizeof(sliceInfo), value.data());
    std::copy_n(info.desc, info.descLen, value.data() + sizeof(sliceInfo));

    uint64_t totalSize = 0;
    SM_LOG_DEBUG("begin append(key=" << localKeys_.sliceInfo << ", value_size=" << value.size() << ")");
    auto ret = store_->Append(localKeys_.sliceInfo, value, totalSize);
    if (ret != 0) {
        SM_LOG_ERROR("store append slice info failed: " << ret);
        return SM_ERROR;
    }

    SM_LOG_DEBUG("success append(key=" << localKeys_.sliceCount << ", value_size=" << value.size()
                                       << "), total_size=" << totalSize);
    int64_t nowCount = 0;
    ret = store_->Add(localKeys_.sliceCount, 1L, nowCount);
    if (ret != 0) {
        SM_LOG_ERROR("store add count for slice info failed: " << ret);
        return SM_ERROR;
    }

    SM_LOG_DEBUG("now slice total count = " << nowCount);
    return SM_OK;
}

void SmemStoreHelper::FindNewRemoteRanks(const FindRanksCbFunc &cb) noexcept
{
    SM_ASSERT_RET_VOID(deviceExpSize_ != 0);

    int64_t totalValue = 0;
    auto ret = store_->Add(remoteKeys_.deviceCount, 0L, totalValue);
    if (ret != 0) {
        SM_LOG_ERROR("store add(0) for key(" << remoteKeys_.deviceCount << ") count failed: " << ret);
        return;
    }

    if (totalValue <= remoteRankLastTime_) {
        return;
    }

    std::vector<uint8_t> values;
    ret = store_->Get(remoteKeys_.deviceInfo, values);
    if (ret != 0) {
        SM_LOG_ERROR("store get devices info with key(" << remoteKeys_.deviceInfo << ") failed: " << ret);
        return;
    }

    auto increment = static_cast<uint32_t>(totalValue - remoteRankLastTime_);
    std::vector<hybm_exchange_info> deltaInfo(increment);
    for (auto i = 0U; i < increment; i++) {
        std::copy_n(values.data() + (remoteRankLastTime_ + i) * deviceExpSize_, deviceExpSize_, deltaInfo[i].desc);
        deltaInfo[i].descLen = deviceExpSize_;
    }

    ret = cb(deltaInfo);
    if (ret != 0) {
        SM_LOG_ERROR("find new ranks callback failed: " << ret);
        return;
    }

    remoteRankLastTime_ = totalValue;
}

void SmemStoreHelper::FindNewRemoteSlices(const FindSlicesCbFunc &cb) noexcept
{
    SM_ASSERT_RET_VOID(sliceExpSize_ != 0);

    int64_t totalValue = 0;
    auto ret = store_->Add(remoteKeys_.sliceCount, 0L, totalValue);
    if (ret != 0) {
        SM_LOG_ERROR("store add(0) for key(" << remoteKeys_.sliceCount << ") total count failed: " << ret);
        return;
    }

    if (totalValue <= remoteSliceLastTime_) {
        return;
    }

    std::vector<uint8_t> values;
    ret = store_->Get(remoteKeys_.sliceInfo, values);
    if (ret != 0) {
        SM_LOG_ERROR("store get for key(" << remoteKeys_.sliceInfo << ") all slices failed: " << ret);
        return;
    }

    auto increment = static_cast<uint32_t>(totalValue - remoteSliceLastTime_);
    std::vector<hybm_exchange_info> deltaInfo(increment);
    std::vector<const StoredSliceInfo *> storeSs(increment);
    auto itemOffsetBytes = (sizeof(StoredSliceInfo) + sliceExpSize_) * static_cast<uint64_t>(remoteSliceLastTime_);
    if (itemOffsetBytes + increment * (sizeof(StoredSliceInfo) + sliceExpSize_) > values.size()) {
        SM_LOG_ERROR("Buffer overflow detected in " << remoteKeys_.sliceInfo);
        return;
    }

    for (auto i = 0U; i < increment; i++) {
        storeSs[i] = (const StoredSliceInfo *)(const void *)(values.data() + itemOffsetBytes);
        std::copy_n(values.data() + itemOffsetBytes + sizeof(StoredSliceInfo), sliceExpSize_, deltaInfo[i].desc);
        deltaInfo[i].descLen = sliceExpSize_;
        itemOffsetBytes += (sliceExpSize_ + sizeof(StoredSliceInfo));
    }

    ret = cb(deltaInfo, storeSs);
    if (ret != 0) {
        SM_LOG_ERROR("find new slices callback failed: " << ret);
        return;
    }

    remoteSliceLastTime_ = totalValue;
}
}
}