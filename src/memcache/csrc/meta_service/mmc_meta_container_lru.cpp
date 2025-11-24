/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MF_HYBRID_MMC_META_CONTAINER_LRU_H
#define MF_HYBRID_MMC_META_CONTAINER_LRU_H

#include <list>
#include <memory>
#include <unordered_map>

#include "mmc_mem_obj_meta.h"
#include "mf_rwlock.h"
#include "mmc_meta_container.h"

namespace ock {
namespace mmc {

template <typename Key, typename Value> class MmcMetaContainerLRU : public MmcMetaContainer<Key, Value> {
private:
    struct ValueLruItem {
        Value value_;
        MediaType mediaType_;
        typename std::list<Key>::iterator lruIter_;
    };
    std::unordered_map<Key, ValueLruItem> metaMap_;
    std::list<Key> lruLists_[MEDIA_NONE];
    MediaType topLayer_ = MEDIA_NONE;
    ock::mf::ReadWriteLock lruLock_;
    ock::mf::ReadWriteLock metaLock_;

public:
    Result Insert(const Key &key, const Value &value) override
    {
        ock::mf::WriteGuard lockGuard(metaLock_);
        auto iter = metaMap_.find(key);
        if (iter != metaMap_.end()) {
            MMC_LOG_WARN("Fail to insert "
                          << key << " into MmcMetaContainer. Key already exists. ErrCode: " << MMC_DUPLICATED_OBJECT);
            return MMC_DUPLICATED_OBJECT;
        }

        // insert into LRU and create lruItem
        ock::mf::WriteGuard lrucLockGuard(lruLock_);
        if (topLayer_ == MEDIA_NONE) {
            MMC_LOG_ERROR("Failed to Insert, no medium registered. ");
            return MMC_ERROR;
        }
        lruLists_[topLayer_].push_front(key);
        ValueLruItem lruItem{value, topLayer_, lruLists_[topLayer_].begin()};

        // insert into map
        auto ret = metaMap_.emplace(key, lruItem);
        if (ret.second) {
            return MMC_OK;
        }
        lruLists_[topLayer_].erase(lruItem.lruIter_);
        MMC_LOG_ERROR("Fail to insert " << key << " into MmcMetaContainer. ErrCode: " << MMC_ERROR);
        return MMC_ERROR;
    }

    Result Get(const Key &key, Value &value) override
    {
        ock::mf::ReadGuard lockGuard(metaLock_);
        auto iter = metaMap_.find(key);
        if (iter != metaMap_.end()) {
            value = iter->second.value_;
            return MMC_OK;
        }
        MMC_LOG_DEBUG("Get: Key " << key << " not found in MmcMetaContainer. ErrCode: " << MMC_UNMATCHED_KEY);
        return MMC_UNMATCHED_KEY;
    }

    Result Erase(const Key& key) override
    {
        Value value;
        return Erase(key, value);
    }

    Result Erase(const Key& key, Value& value) override
    {
        ock::mf::WriteGuard lockGuard(metaLock_);
        auto iter = metaMap_.find(key);
        if (iter == metaMap_.end()) {
            MMC_LOG_INFO("Key " << key << " not found in MmcMetaContainer. ErrCode: " << MMC_UNMATCHED_KEY);
            return MMC_UNMATCHED_KEY;
        }
        auto lruIter = iter->second.lruIter_;
        value = iter->second.value_;
        if (metaMap_.erase(key) > 0) {
            ock::mf::WriteGuard lockGuard(lruLock_);
            lruLists_[iter->second.mediaType_].erase(lruIter);
            return MMC_OK;
        }
        MMC_LOG_ERROR("Fail to erase " << key << " from MmcMetaContainer. ErrCode: " << MMC_ERROR);
        return MMC_ERROR;
    }

    void EraseIf(std::function<bool(const Key&, const Value&)> matchFunc) override
    {
        ock::mf::WriteGuard lockGuard(metaLock_);
        for (auto iter = metaMap_.begin(); iter != metaMap_.end();) {
            if (matchFunc(iter->first, iter->second.value_)) {
                ock::mf::WriteGuard lockGuard(lruLock_);
                lruLists_[iter->second.mediaType_].erase(iter->second.lruIter_);
                iter = metaMap_.erase(iter);
            } else {
                ++iter;
            }
        }
    }

    void IterateIf(std::function<bool(const Key&, const Value&)> matchFunc,
                   std::map<Key, Value>& matchedValues) override
    {
        ock::mf::ReadGuard lockGuard(metaLock_);
        for (auto iter = metaMap_.begin(); iter != metaMap_.end();) {
            if (matchFunc(iter->first, iter->second.value_)) {
                matchedValues.emplace(std::make_pair(iter->first, iter->second.value_));
            }
            ++iter;
        }
    }

    Result Promote(const Key &key)
    {
        ock::mf::ReadGuard lockGuard(metaLock_);
        auto iter = metaMap_.find(key);
        if (iter != metaMap_.end()) {
            UpdateLRU(iter->first, iter->second);
            return MMC_OK;
        }
        MMC_LOG_INFO("Promote: Key " << key << " not found in MmcMetaContainer. ErrCode: " << MMC_UNMATCHED_KEY);
        return MMC_UNMATCHED_KEY;
    }

    bool EvictOneLeastRecentlyUsed(std::function<EvictResult(const Key &, const Value &)> moveFunc,
                                   const MediaType mediaType)
    {
        if (mediaType == MEDIA_NONE) {
            MMC_LOG_ERROR("Invalid mediaType: " << mediaType);
            return false;
        }
        ock::mf::WriteGuard lockGuard(metaLock_);
        ock::mf::WriteGuard lruLockGuard(lruLock_);
        if (lruLists_[mediaType].empty()) {
            MMC_LOG_ERROR("Unable to evict, LRU list is empty");
            return false;
        }
        auto iter = std::prev(lruLists_[mediaType].end());
        Key key = *iter;

        // 1、查找value
        auto mapIter = metaMap_.find(key);
        if (mapIter == metaMap_.end()) {
            lruLists_[mediaType].erase(iter);
            MMC_LOG_INFO("Key " << key << " not found in MmcMetaContainer.");
            return true;
        }

        // 2、删除map和lru
        Value& value = mapIter->second.value_;

        // 3、回调处理key，value
        EvictResult res = moveFunc(key, value);
        if (res == EvictResult::REMOVE) {
            metaMap_.erase(mapIter);
            lruLists_[mediaType].erase(iter);
            MMC_LOG_INFO("Key " << key << " will be evicted by remove.");
            return true;
        }
        if (res == EvictResult::MOVE_DOWN) {
            auto targetMedium = MoveDown(mediaType);
            if (targetMedium == MEDIA_NONE) {
                MMC_LOG_ERROR("Invalid target medium: " << targetMedium);
                return false;
            }
            lruLists_[mediaType].erase(iter);
            lruLists_[targetMedium].push_front(key);
            mapIter->second.mediaType_ = targetMedium;
            mapIter->second.lruIter_ = lruLists_[targetMedium].begin();
            MMC_LOG_INFO("Key " << key << " will be evicted from " << mediaType << " to " << targetMedium << ".");
            return true;
        }
        MMC_LOG_ERROR("Failed to evict Key " << key << ", moveFunc failed.");
        return false;
    }

    void MultiLevelElimination(const uint16_t evictThresholdHigh, const uint16_t evictThresholdLow,
                               const std::vector<MediaType>& needEvictList,
                               std::function<EvictResult(const Key &, const Value &)> moveFunc)
    {
        for (const MediaType mediaType : needEvictList) {
            if (mediaType == MEDIA_NONE) {
                MMC_LOG_ERROR("Invalid mediaType: " << mediaType);
                continue;
            }

            lruLock_.LockRead();
            size_t oriNum = lruLists_[mediaType].size();
            lruLock_.UnLock();

            const size_t numEvictObjs = std::max(
                std::min(oriNum * (evictThresholdHigh - evictThresholdLow) / evictThresholdHigh, oriNum),
                static_cast<size_t>(1));

            for (size_t i = 0; i < numEvictObjs; ++i) {
                EvictOneLeastRecentlyUsed(moveFunc, mediaType);
            }

            MMC_LOG_INFO("Evicted " << numEvictObjs << " keys from " << mediaType <<
                ". Number of keys: " << oriNum << " => " << (oriNum - numEvictObjs) << ".");
        }
    }

    void RegisterMedium(const MediaType mediaType) override
    {
        topLayer_ = std::min(topLayer_, mediaType);
        MMC_LOG_DEBUG("top layer: " << topLayer_);
    }

private:
    void UpdateLRU(const Key &key, ValueLruItem &lruItem)
    {
        // 只支持在同层级内部更新
        ock::mf::WriteGuard lockGuard(lruLock_);
        lruLists_[lruItem.mediaType_].erase(lruItem.lruIter_);
        lruLists_[lruItem.mediaType_].push_front(key);
        lruItem.lruIter_ = lruLists_[lruItem.mediaType_].begin();
    }
};

// 显式实例化常用类型
template class MmcMetaContainer<std::string, MmcMemObjMetaPtr>;

// 工厂函数实现
template <typename Key, typename Value> MmcRef<MmcMetaContainer<Key, Value>> MmcMetaContainer<Key, Value>::Create()
{
    return MmcMakeRef<MmcMetaContainerLRU<Key, Value>>().Get();
}

}  // namespace mmc
}  // namespace ock

#endif  // MF_HYBRID_MMC_META_CONTAINER_LRU_H