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
#include "hybm_entity_tag_info.h"

#include <regex>

namespace ock {
namespace mf {

Result HybmEntityTagInfo::TagInfoInit(hybm_options option)
{
    rankTagInfo_.reserve(option.rankCount);
    auto ret = AddRankTag(option.rankId, option.tag);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to add rankId:" << option.rankId << " tag:" << option.tag);
        return ret;
    }
    ret = AddTagOpInfo(option.tagOpInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Failed to add tagOpInfo:" << option.tagOpInfo);
        return ret;
    }
    return BM_OK;
}

Result HybmEntityTagInfo::AddRankTag(uint32_t rankId, const std::string &tag)
{
    if (tag.empty()) {
        BM_LOG_WARN("Add an empty tag for rankId:" << rankId);
        return BM_OK;
    }
    static const std::regex tagPattern(R"(^[a-zA-Z0-9_]{1,30}$)");
    std::smatch match;
    if (!std::regex_match(tag, match, tagPattern)) {
        BM_LOG_ERROR("Failed to check tag:" << tag << " is invalid must match (^[a-zA-Z0-9_]{1,30}$)");
        return BM_INVALID_PARAM;
    }
    std::unique_lock lock(mutex_);
    rankTagInfo_[rankId] = tag;
    BM_LOG_INFO("Success to add tag:" << tag << " rankId:" << rankId);
    return BM_OK;
}

Result HybmEntityTagInfo::AddOneTagOpInfo(const std::string &tagOpInfo)
{
    // tag:opType:tag
    static const std::regex tagOpInfoPattern(R"(^([a-zA-Z0-9_]{1,30}):([A-Z_]{8,12}):([a-zA-Z0-9_]{1,30})$)");
    static const std::map<std::string, hybm_data_op_type> str2OpTypeMap = {
        {"DEVICE_SDMA", HYBM_DOP_TYPE_SDMA},    {"DEVICE_RDMA", HYBM_DOP_TYPE_DEVICE_RDMA},
        {"HOST_RDMA", HYBM_DOP_TYPE_HOST_RDMA}, {"HOST_TCP", HYBM_DOP_TYPE_HOST_TCP},
        {"HOST_URMA", HYBM_DOP_TYPE_HOST_URMA}, {"DEVICE_MTE", HYBM_DOP_TYPE_MTE},
    };

    std::smatch match;
    if (!std::regex_match(tagOpInfo, match, tagOpInfoPattern)) {
        BM_LOG_ERROR("Failed to check tagOpInfo:"
                     << tagOpInfo
                     << " is invalid must match (^([a-zA-Z0-9_]{1,30):([a-zA-Z0-9_]{1,30}):([A-Z_]{8,12})$)");
        return BM_INVALID_PARAM;
    }
    auto tag1 = match[1].str();
    auto opTypeStr = match[2].str();
    auto tag2 = match[3].str();
    auto it = str2OpTypeMap.find(opTypeStr);
    if (it == str2OpTypeMap.end()) {
        BM_LOG_ERROR("Failed to check opType:"
                     << opTypeStr << " should be in (DEVICE_SDMA, DEVICE_RDMA, HOST_RDMA, HOST_TCP, HOST_URMA)");
        return BM_INVALID_PARAM;
    }
    auto opType = GetTag2TagOpType(tag1, tag2);
    auto key = tag1 + ":" + tag2;
    std::unique_lock lock(mutex_);
    tagOpInfo_[key] = (opType | it->second);
    BM_LOG_INFO("Success to update tag1:" << tag1 << " tag2:" << tag2 << " op:" << tagOpInfo_[key]);
    return BM_OK;
}

Result HybmEntityTagInfo::AddTagOpInfo(const std::string &tagOpInfo)
{
    if (tagOpInfo.empty()) {
        BM_LOG_WARN("Add an empty tagOpInfo.");
        return BM_OK;
    }
    // tag:opType:tag,tag:opType:tag,tag:opType:tag
    auto tagOpInfoVec = StrUtil::Split(tagOpInfo, ',');
    for (const auto &item : tagOpInfoVec) {
        auto ret = AddOneTagOpInfo(item);
        if (ret != BM_OK) {
            return ret;
        }
    }
    return BM_OK;
}

Result HybmEntityTagInfo::RemoveRankTag(uint32_t rankId, const std::string &tag)
{
    std::unique_lock lock(mutex_);
    auto it = rankTagInfo_.find(rankId);
    if (it != rankTagInfo_.end()) {
        rankTagInfo_.erase(it);
    }
    return BM_OK;
}

std::string HybmEntityTagInfo::GetTagByRank(uint32_t rankId)
{
    std::shared_lock lock(mutex_);
    auto it = rankTagInfo_.find(rankId);
    if (it != rankTagInfo_.end()) {
        return it->second;
    }
    BM_LOG_WARN("Not find tagInfo by rankId:" << rankId);
    return "";
}

uint32_t HybmEntityTagInfo::GetTag2TagOpType(const std::string &tag1, const std::string &tag2)
{
    std::string key;
    key.reserve(tag1.size() + tag2.size() + 1);
    key.append(tag1);
    key.append(":");
    key.append(tag2);
    std::string flipKey;
    flipKey.reserve(tag1.size() + tag2.size() + 1);
    flipKey.append(tag2);
    flipKey.append(":");
    flipKey.append(tag1);
    std::shared_lock lock(mutex_);
    auto it = tagOpInfo_.find(key);
    if (it != tagOpInfo_.end()) {
        return it->second;
    }
    it = tagOpInfo_.find(flipKey);
    if (it != tagOpInfo_.end()) {
        return it->second;
    }
    BM_LOG_WARN("Not find opType from tag1:" << tag1 << " to tag2:" << tag2);
    return HYBM_DOP_TYPE_DEFAULT;
}

uint32_t HybmEntityTagInfo::GetRank2RankOpType(uint32_t rankId1, uint32_t rankId2)
{
    auto it = rankTagInfo_.find(rankId1);
    if (it == rankTagInfo_.end()) {
        BM_LOG_WARN("Not find tag rank1:" << rankId1);
        return HYBM_DOP_TYPE_DEFAULT;
    }
    auto tag1 = it->second;
    it = rankTagInfo_.find(rankId2);
    if (it == rankTagInfo_.end()) {
        BM_LOG_WARN("Not find tag rank2:" << rankId2);
        return HYBM_DOP_TYPE_DEFAULT;
    }
    auto tag2 = it->second;
    return GetTag2TagOpType(tag1, tag2);
}

uint32_t HybmEntityTagInfo::GetAllOpType()
{
    uint32_t opSet = 0;
    for (const auto &item: tagOpInfo_) {
        opSet |= item.second;
    }
    return opSet;
}

std::string HybmEntityTagInfo::GetOpTypeStr(hybm_data_op_type opType)
{
    static const std::map<hybm_data_op_type, std::string> opType2StrMap = {
        {HYBM_DOP_TYPE_SDMA, "DEVICE_SDMA"},    {HYBM_DOP_TYPE_DEVICE_RDMA, "DEVICE_RDMA"},
        {HYBM_DOP_TYPE_HOST_RDMA, "HOST_RDMA"}, {HYBM_DOP_TYPE_HOST_TCP, "HOST_TCP"},
        {HYBM_DOP_TYPE_HOST_URMA, "HOST_URMA"}, {HYBM_DOP_TYPE_MTE, "DEVICE_MTE"},
    };
    auto it = opType2StrMap.find(opType);
    if (it != opType2StrMap.end()) {
        return it->second;
    }
    return "OP_TYPE_BUTT";
}

} // namespace mf
} // namespace ock