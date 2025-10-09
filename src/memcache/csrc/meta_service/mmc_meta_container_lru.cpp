/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MF_HYBRID_MMC_META_CONTAINER_LRU_H
#define MF_HYBRID_MMC_META_CONTAINER_LRU_H

#include <list>
#include <memory>
#include <unordered_map>

#include "mmc_mem_obj_meta.h"
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
    mutable std::mutex mutex_;

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
    std::unique_ptr<IterBase> Begin() override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return std::unique_ptr<IterBase>(new (std::nothrow) MetaIterator(metaMap_.begin(), metaMap_.end()));
    }

    std::unique_ptr<IterBase> End() override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return std::unique_ptr<IterBase>(new (std::nothrow) MetaIterator(metaMap_.end(), metaMap_.end()));
    }

    Result Insert(const Key &key, const Value &value) override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto iter = metaMap_.find(key);
        if (iter != metaMap_.end()) {
            MMC_LOG_WARN("Fail to insert "
                          << key << " into MmcMetaContainer. Key already exists. ErrCode: " << MMC_DUPLICATED_OBJECT);
            return MMC_DUPLICATED_OBJECT;
        }

        // insert into LRU and create lruItem
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
        std::lock_guard<std::mutex> guard(mutex_);
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
        std::lock_guard<std::mutex> guard(mutex_);
        auto iter = metaMap_.find(key);
        if (iter == metaMap_.end()) {
            MMC_LOG_INFO("Key " << key << " not found in MmcMetaContainer. ErrCode: " << MMC_UNMATCHED_KEY);
            return MMC_UNMATCHED_KEY;
        }
        auto lruIter = iter->second.lruIter_;
        value = iter->second.value_;
        if (metaMap_.erase(key) > 0) {
            lruList_.erase(lruIter);
            return MMC_OK;
        }
        MMC_LOG_ERROR("Fail to erase " << key << " from MmcMetaContainer. ErrCode: " << MMC_ERROR);
        return MMC_ERROR;
    }

    void EraseIf(std::function<bool(const Key&, const Value&)> matchFunc) override
    {
        std::lock_guard<std::mutex> guard(mutex_);
        for (auto iter = metaMap_.begin(); iter != metaMap_.end();) {
            if (matchFunc(iter->first, iter->second.value_)) {
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
        std::lock_guard<std::mutex> guard(mutex_);
        for (auto iter = metaMap_.begin(); iter != metaMap_.end();) {
            if (matchFunc(iter->first, iter->second.value_)) {
                matchedValues.emplace(std::make_pair(iter->first, iter->second.value_));
            }
            ++iter;
        }
    }

    Result Promote(const Key &key)
    {
        std::lock_guard<std::mutex> guard(mutex_);
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
        std::lock_guard<std::mutex> guard(mutex_);

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

    std::vector<Key> MultiLevelElimination(const uint16_t evictThresholdHigh, const uint16_t evictThresholdLow,
                                           std::function<bool(const Key&, const Value&)> moveFunc)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        size_t oriNum = lruList_.size();
        uint32_t numEvictObjs = std::max(std::min(
            oriNum * (evictThresholdHigh - evictThresholdLow) / evictThresholdHigh,
            oriNum), static_cast<size_t>(1));
        std::vector<Key> removed;
        removed.reserve(numEvictObjs);
        auto iter = lruList_.end();
        for (size_t i = 0; i < numEvictObjs && iter != lruList_.begin(); ++i) {
            --iter; // iter前移
            // 1、查找value
            Key& key = *iter;
            auto mapIter = metaMap_.find(key);
            if (mapIter == metaMap_.end()) {
                MMC_LOG_INFO("Key " << key << " not found in MmcMetaContainer.");
                iter = lruList_.erase(iter);
                continue;
            }
            // 2、删除map和lru
            Value& value = mapIter->second.value_;

            // 3、回调处理key，value
            if (moveFunc(key, value)) {
                MMC_LOG_INFO("Key " << key << " removed by evict.");
                metaMap_.erase(mapIter);
                iter = lruList_.erase(iter);
                removed.push_back(key);
            }
        }

        if (!removed.empty()) {
            MMC_LOG_INFO("evict items from " << oriNum << " to " << lruList_.size()
                                             << ", evict size: " << removed.size());
        }
        return removed;
    }

private:
    void UpdateLRU(const Key &key, ValueLruItem &lruItem)
    {
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