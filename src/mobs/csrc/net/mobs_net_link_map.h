/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MOBS_NET_LINK_H
#define MOBS_NET_LINK_H

#include "mobs_common_includes.h"

namespace ock {
namespace mobs {
/**
 * Concurrent map link map
 */
template <typename LINK_PTR>
class NetLinkMap : public MOReferable {
public:
    /* *
     * @brief Add link into this map
     *
     * @param id               [in] peer service id
     * @param link             [in] link
     * @return true if found, and hold the reference of link
     */
    bool Find(const uint16_t &id, TcpLinkPtr &link);

    /* *
     * @brief Add a link into this map
     *
     * @param id             [in] peer service id
     * @param link           [in] link
     * @return true if added
     */
    bool Add(const uint16_t &id, const TcpLinkPtr &link);

    /* *
     * @brief Remove a link from this map
     *
     * @param id             [in] peer service id
     * @return true if removed
     */
    bool Remove(const uint16_t &id);

    /* *
     * @brief Remove all links in this map
     */
    void Clear();

private:
    const static int32_t gSubMapCount = 7L;

private:
    std::mutex mapMutex_[gSubMapCount];
    std::map<int32_t, LINK_PTR> linkMaps_[gSubMapCount];
};

template <typename LINK_PTR>
inline bool NetLinkMap::Find(const uint16_t &id, LINK_PTR &link)
{
    auto bucket = id.whole % gSubMapCount;
    {
        std::lock_guard<std::mutex> guard(mapMutex_[bucket]);
        auto iter = linkMaps_[bucket].find(id.whole);
        if (iter != linkMaps_[bucket].end()) {
            link = iter->second;
            return true;
        }
    }
    return false;
}

template <typename LINK_PTR>
inline bool NetLinkMap::Add(const uint16_t &id, const LINK_PTR &link)
{
    auto bucket = id.whole % gSubMapCount;
    {
        std::lock_guard<std::mutex> guard(mapMutex_[bucket]);
        return linkMaps_[bucket].emplace(id.whole, link).second;
    }
}

inline bool NetLinkMap::Remove(const uint16_t &id)
{
    auto bucket = id.whole % gSubMapCount;
    {
        std::lock_guard<std::mutex> guard(mapMutex_[bucket]);
        return linkMaps_[bucket].erase(id.whole) != 0;
    }
}

inline void NetLinkMap::Clear()
{
    for (int32_t bucket = 0; bucket < gSubMapCount; bucket++) {
        std::lock_guard<std::mutex> guard(mapMutex_[bucket]);
        linkMaps_[bucket].clear();
    }
}
}
}

#endif //MOBS_NET_LINK_H
