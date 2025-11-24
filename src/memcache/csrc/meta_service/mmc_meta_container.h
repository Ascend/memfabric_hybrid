/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_MMC_META_CONTAINER_H
#define MF_HYBRID_MMC_META_CONTAINER_H

#include <memory>

#include "mmc_meta_common.h"
#include "mmc_ref.h"

namespace ock {
namespace mmc {

template <typename Key, typename Value> class MmcMetaContainer : public MmcReferable {
public:
    virtual ~MmcMetaContainer() = default;
    virtual Result Insert(const Key &key, const Value &value) = 0;
    virtual Result Get(const Key &key, Value &value) = 0;
    virtual Result Erase(const Key &key) = 0;
    virtual Result Erase(const Key& key, Value& value) = 0;
    virtual void EraseIf(std::function<bool(const Key&, const Value&)> matchFunc) = 0;
    virtual void IterateIf(std::function<bool(const Key&, const Value&)> matchFunc,
                           std::map<Key, Value>& matchedValues) = 0;
    virtual Result Promote(const Key& key) = 0;
    virtual void MultiLevelElimination(const uint16_t evictThresholdHigh, const uint16_t evictThresholdLow,
                                       const std::vector<MediaType>& needEvictList,
                                       std::function<EvictResult(const Key &, const Value &)> removeFunc) = 0;
    virtual void RegisterMedium(const MediaType mediumType) = 0;

    static MmcRef<MmcMetaContainer<Key, Value>> Create();
};
}  // namespace mmc
}  // namespace ock

#endif  // MF_HYBRID_MMC_META_CONTAINER_H
