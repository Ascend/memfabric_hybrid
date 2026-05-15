/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef ZBAL_COMM_GROUP_ID_H
#define ZBAL_COMM_GROUP_ID_H

#include "zbal_comm_types.h"

namespace zbal {
namespace operators {
/**
 * Auto release id back by bootstrap when the object instance deconstructing
 */
class AutoReleaseGroupId {
public:
    AutoReleaseGroupId() : maxGroupCount_(0), rankSize_(0), rankId_(UINT16_MAX), workRankId_(UINT16_MAX), groupName_("")
    {}

    AutoReleaseGroupId(uint32_t maxGroupCount, uint16_t rankSize, uint16_t rankId, uint16_t worldRankId,
                       const std::string &groupName)
        : maxGroupCount_(maxGroupCount), rankSize_(rankSize), rankId_(rankId), workRankId_(worldRankId),
          groupName_(groupName)
    {}

    ~AutoReleaseGroupId();

    /**
     * Acquire an unique id by bootstrap
     * @return 0 if successful
     */
    ZResult Acquire();

    /**
     * @brief Release the id back by bootstrap for other to use,
     * and set inner id to invalid one
     */
    void Release();

    /* delete default copy constructor */
    AutoReleaseGroupId(const AutoReleaseGroupId &obj) = delete;
    AutoReleaseGroupId &operator=(const AutoReleaseGroupId &obj) = delete;
    AutoReleaseGroupId(AutoReleaseGroupId &&other) = delete;
    AutoReleaseGroupId &operator=(AutoReleaseGroupId &&other) = delete;

    /**
     * @brief Exchange id with other
     *
     * @param other
     */
    void MoveIdAndGatheredInfo(AutoReleaseGroupId &other) noexcept;

    /**
     * @brief Get the id
     *
     * @return id
     */
    uint16_t Id() const noexcept;

    /**
     * @brief Get gathered group info
     *
     * @return info
     */
    const std::vector<CommGroupExchangeInfo> &GatheredGroupInfo() const noexcept;

    friend std::ostream &operator<<(std::ostream &os, const AutoReleaseGroupId &info)
    {
        os << "AutoReleaseGroupId [maxGroupCount_: " << info.maxGroupCount_ << ", rankSize_: " << info.rankSize_
           << ", rankId_: " << info.rankId_ << ", workRankId_: " << info.workRankId_
           << ", uniqueGroupId_: " << info.uniqueGroupId_ << ", groupName_: " << info.groupName_
           << ", gatheredGroupInfo_: [size: " << info.gatheredGroupInfo_.size() << ", items: [";

        for (auto &item : info.gatheredGroupInfo_) {
            os << item << ",";
        }

        os << "]]]";

        return os;
    }

private:
    static ZResult AcquireUniqueGroupId(uint32_t maxGroupCount, uint32_t &uniqueId);
    static ZResult ReleaseUniqueGroupId(uint32_t uniqueId);
    static ZResult AllGatherExchangeInfo(const std::string &groupName, const CommGroupExchangeInfo &myInfo,
                                         std::vector<CommGroupExchangeInfo> &allInfo, uint32_t groupSize);

private:
    std::mutex mutex_;                                     /* mutex for guard */
    const uint32_t maxGroupCount_;                         /* cap number of max group can be existed, in-mutable */
    const uint16_t rankSize_;                              /* rank size of the group, in-mutable  */
    const uint16_t rankId_;                                /* rank id in the group, in-mutable  */
    const uint16_t workRankId_;                            /* rank id in world, in-mutable  */
    uint16_t uniqueGroupId_ = UINT16_MAX;                  /* unique group id in the world */
    const std::string groupName_;                          /* group name, in-mutable  */
    std::vector<CommGroupExchangeInfo> gatheredGroupInfo_; /* allGathered info in the group */
};

inline uint16_t AutoReleaseGroupId::Id() const noexcept
{
    return uniqueGroupId_;
}

inline const std::vector<CommGroupExchangeInfo> &AutoReleaseGroupId::GatheredGroupInfo() const noexcept
{
    return gatheredGroupInfo_;
}
} // namespace operators
} // namespace zbal

#endif // ZBAL_COMM_GROUP_ID_H
