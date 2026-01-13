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
#ifndef MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H
#define MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H

#include <ostream>
#include "hybm_common_include.h"
#include "hybm_big_mem.h"

namespace ock {
namespace mf {

struct ExtOptions {
    uint32_t srcRankId;
    uint32_t destRankId;
    void *stream = nullptr;
    uint32_t flags;
    friend bool operator==(const ExtOptions &lhs, const ExtOptions &rhs)
    {
        return lhs.srcRankId == rhs.srcRankId && lhs.destRankId == rhs.destRankId;
    }
    friend bool operator!=(const ExtOptions &lhs, const ExtOptions &rhs)
    {
        return !(lhs == rhs);
    }
    friend std::ostream &operator<<(std::ostream &os, const ExtOptions &obj)
    {
        return os << "srcRankId: " << obj.srcRankId << " destRankId: " << obj.destRankId;
    }
};

struct ExtOptionsHash {
    ExtOptionsHash() = default;
    std::hash<std::string> strHash;
    size_t operator()(const ExtOptions &options) const
    {
        return strHash(std::to_string(options.srcRankId) + "-" + std::to_string(options.destRankId));
    }
};
typedef struct {
    std::vector<void *> localAddrs;
    std::vector<void *> globalAddrs;
    std::vector<uint64_t> counts;
} CopyDescriptor;

class DataOperator {
public:
    virtual Result Initialize() noexcept = 0;
    virtual void UnInitialize() noexcept = 0;

    virtual Result DataCopy(hybm_copy_params &params, hybm_data_copy_direction direction,
                            const ExtOptions &options) noexcept = 0;
    virtual Result BatchDataCopy(hybm_batch_copy_params &params, hybm_data_copy_direction direction,
                                 const ExtOptions &options) noexcept = 0;
    /*
     * 异步data copy
     * @return 0 if successful, > 0 is wait id, < 0 is error
     */
    virtual Result DataCopyAsync(hybm_copy_params &params, hybm_data_copy_direction direction,
                                 const ExtOptions &options) noexcept = 0;

    virtual Result Wait(int32_t waitId) noexcept = 0;

    virtual ~DataOperator() = default;

public:
    void UpdateGvaSpace(hybm_mem_type type, uint64_t gva, uint64_t localSpaceSize, uint64_t rankCount) noexcept
    {
        if (type == HYBM_MEM_TYPE_BUTT || localSpaceSize == 0) {
            BM_LOG_ERROR("type " << type << " error, local space " << localSpaceSize);
            return;
        }
        gva_[type] = gva;
        localSpaceSize_[type] = localSpaceSize;
        rankCount_[type] = rankCount;
        BM_LOG_INFO("update type " << type << " gva: " << std::hex << gva_[type] << ", space:" << localSpaceSize_[type]
                                   << ", rankCnt:" << rankCount_[type]);
    };

    uint32_t GetRankIdByGva(uint64_t gva) noexcept
    {
        for (auto type = 0; type < HYBM_MEM_TYPE_BUTT; ++type) {
            if (gva >= gva_[type] && gva < (gva_[type] + rankCount_[type] * localSpaceSize_[type])) {
                return (gva - gva_[type]) / localSpaceSize_[type];
            }
        }
        BM_LOG_ERROR("failed to get rank id by gva: " << std::hex << gva <<
            ", gva of device: " << std::hex << gva_[HYBM_MEM_TYPE_DEVICE] <<
            ", gva of host: " << std::hex << gva_[HYBM_MEM_TYPE_HOST]);
        return UINT32_MAX;
    }

    virtual void CleanUp() noexcept
    {
        BM_LOG_INFO("DataOperator not support");
        return;
    }

protected:
    static inline uint64_t gva_[HYBM_MEM_TYPE_BUTT] = {0};
    static inline uint64_t localSpaceSize_[HYBM_MEM_TYPE_BUTT] = {0};
    static inline uint64_t rankCount_[HYBM_MEM_TYPE_BUTT] = {0};
};

using DataOperatorPtr = std::shared_ptr<DataOperator>;
} // namespace mf
} // namespace ock

#endif // MEM_FABRIC_HYBRID_HYBM_DATA_ACTION_H