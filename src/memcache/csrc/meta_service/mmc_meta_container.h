/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MF_HYBRID_MMC_META_CONTAINER_H
#define MF_HYBRID_MMC_META_CONTAINER_H

#include "mmc_meta_common.h"
#include "mmc_ref.h"
#include <memory>

namespace ock {
namespace mmc {

template <typename Key, typename Value> class MmcMetaContainer : public MmcReferable {
public:
    class MetaIteratorBase {
    public:
        virtual ~MetaIteratorBase() = default;
        virtual MetaIteratorBase &operator++() = 0;
        virtual bool operator!=(const MetaIteratorBase &other) const = 0;
        virtual std::pair<Key, Value> operator*() const = 0;
    };

    virtual ~MmcMetaContainer() = default;
    virtual std::unique_ptr<MetaIteratorBase> Begin() = 0;
    virtual std::unique_ptr<MetaIteratorBase> End() = 0;
    virtual Result Insert(const Key &key, const Value &value) = 0;
    virtual Result Get(const Key &key, Value &value) = 0;
    virtual Result Erase(const Key &key) = 0;
    virtual Result Promote(const Key &key) = 0;
    virtual std::vector<Key> TopKeys(const uint16_t percent) = 0;

    static MmcRef<MmcMetaContainer<Key, Value>> Create();
};
}  // namespace mmc
}  // namespace ock

#endif  // MF_HYBRID_MMC_META_CONTAINER_H
