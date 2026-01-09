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
#ifndef MEM_FABRIC_HYBRID_HYBM_ENTITY_TAG_INFO_H
#define MEM_FABRIC_HYBRID_HYBM_ENTITY_TAG_INFO_H

#include <memory>
#include <unordered_map>
#include <shared_mutex>

#include "hybm_common_include.h"

namespace ock {
namespace mf {

class HybmEntityTagInfo {
public:
    Result TagInfoInit(hybm_options option);
    /**
     * AddTagOpInfo
     * @param info eg: tag0:opType:tag1,tag0:opType:tag2
     * opType should in (DEVICE_SDMA, DEVICE_RDMA, HOST_RDMA, HOST_TCP, HOST_URMA)
     * @return 0 if Success
     */
    Result AddTagOpInfo(const std::string &info);

    /**
     * AddRankTag
     * @param rankId rankId
     * @param tag eg: support a~zA~Z_
     * @return 0 if Success
     */
    Result AddRankTag(uint32_t rankId, const std::string &tag);
    Result RemoveRankTag(uint32_t rankId, const std::string &tag);
    std::string GetTagByRank(uint32_t rankId);
    uint32_t GetTag2TagOpType(const std::string &tag1, const std::string &tag2);
    uint32_t GetRank2RankOpType(uint32_t rankId1, uint32_t rankId2);
    uint32_t GetAllOpType();
    static std::string GetOpTypeStr(hybm_data_op_type opType);

private:
    Result AddOneTagOpInfo(const std::string &info);

private:
    std::shared_mutex mutex_;
    std::unordered_map<uint32_t, std::string> rankTagInfo_;
    std::unordered_map<std::string, uint32_t> tagOpInfo_;
};

using HybmEntityTagInfoPtr = std::shared_ptr<HybmEntityTagInfo>;
} // namespace mf
} // namespace ock
#endif // MEM_FABRIC_HYBRID_HYBM_ENTITY_TAG_INFO_H
