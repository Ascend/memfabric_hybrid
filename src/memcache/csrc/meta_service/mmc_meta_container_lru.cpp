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
        typename std::list<Key>::iterator lruIter_;
    };
    std::unordered_map<Key, ValueLruItem> metaMap_;
    std::list<Key> lruList_;
    ock::mf::ReadWriteLock lruLock_;
    ock::mf::ReadWriteLock metaLock_;

    using IterBase = typename MmcMetaContainer<Key, Value>::MetaIteratorBase;

    class MetaIterator : public MmcMetaContainer<Key, Value>::MetaIteratorBase {
        using MapIterator = typename std::unordered_map<Key, ValueLruItem>::iterator;
        MapIterator it_;
        MapIterator end_;

    public:
        MetaIterator(MapIterator it, MapIterator end) : it_(it), end_(end) {}
        IterBase &operator++() override
        {
            ++it_;
            return *this;
        };

        bool operator!=(const IterBase &other) const override
        {
            const MetaIterator *derived = dynamic_cast<const MetaIterator *>(&other);
            return derived && (it_ != derived->it_);
        }

        std::pair<Key, Value> operator*() const override
        {
            return {it_->first, it_->second.value_};
        }
    };

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
        lruList_.push_front(key);
        ValueLruItem lruItem{value, lruList_.begin()};

        // insert into map
        auto ret = metaMap_.emplace(key, lruItem);
        if (ret.second) {
            return MMC_OK;
        }
        lruList_.erase(lruItem.lruIter_);
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
            lruList_.erase(lruIter);
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
                lruList_.erase(iter->second.lruIter_);
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

    std::vector<Key> EvictCandidates(const uint16_t evictThresholdHigh, const uint16_t evictThresholdLow)
    {
        if (evictThresholdLow == 0 || evictThresholdHigh <= evictThresholdLow) {
            MMC_LOG_ERROR("Evict threshold invalid, high: " << evictThresholdHigh << ", low: " << evictThresholdLow);
            return {};
        }
        ock::mf::ReadGuard lockGuard(lruLock_);

        uint32_t numEvictObjs = std::max(std::min(
            lruList_.size() * (evictThresholdHigh - evictThresholdLow) / evictThresholdHigh,
            lruList_.size()), static_cast<size_t>(1));
        std::vector<Key> candidates;
        candidates.reserve(numEvictObjs);
        auto iter = lruList_.rbegin();
        for (size_t i = 0; i < numEvictObjs && iter != lruList_.rend(); ++i, ++iter) {
            candidates.push_back(*iter);
        }
        MMC_LOG_INFO("Touched threshold evict, total size: " << lruList_.size()
            << ", evict size: " << candidates.size());
        return candidates;
    }

    bool EvictOneLeastRecentlyUsed(std::function<bool(const Key &, const Value &)> moveFunc, Key &key)
    {
        ock::mf::WriteGuard lockGuard(metaLock_);
        ock::mf::WriteGuard lruLockGuard(lruLock_);
        if (lruList_.empty()) {
            return false;
        }
        auto iter = std::prev(lruList_.end());
        key = *iter;

        // 1、查找value
        auto mapIter = metaMap_.find(key);
        if (mapIter == metaMap_.end()) {
            lruList_.erase(iter);
            MMC_LOG_INFO("Key " << key << " not found in MmcMetaContainer.");
            return true;
        }

        // 2、删除map和lru
        Value& value = mapIter->second.value_;

        // 3、回调处理key，value
        if (moveFunc(key, value)) {
            metaMap_.erase(mapIter);
            lruList_.erase(iter);
            MMC_LOG_INFO("Key " << key << " removed by evict.");
            return true;
        }
        return false;
    }

    std::vector<Key> MultiLevelElimination(const uint16_t evictThresholdHigh, const uint16_t evictThresholdLow,
                                           std::function<bool(const Key &, const Value &)> moveFunc)
    {
        lruLock_.LockRead();
        size_t oriNum = lruList_.size();
        lruLock_.UnLock();

        uint32_t numEvictObjs =
            std::max(std::min(oriNum * (evictThresholdHigh - evictThresholdLow) / evictThresholdHigh, oriNum),
                     static_cast<size_t>(1));
        std::vector<Key> removed;
        removed.reserve(numEvictObjs);

        for (size_t i = 0; i < numEvictObjs; ++i) {
            Key lastKey;
            if (EvictOneLeastRecentlyUsed(moveFunc, lastKey)) {
                removed.push_back(lastKey);
            }
        }

        if (!removed.empty()) {
            MMC_LOG_WARN("evict items from " << oriNum << " to " << lruList_.size()
                                             << ", evict size: " << removed.size());
        }
        return removed;
    }

private:
    void UpdateLRU(const Key &key, ValueLruItem &lruItem)
    {
        ock::mf::WriteGuard lockGuard(lruLock_);
        lruList_.erase(lruItem.lruIter_);
        lruList_.push_front(key);
        lruItem.lruIter_ = lruList_.begin();
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