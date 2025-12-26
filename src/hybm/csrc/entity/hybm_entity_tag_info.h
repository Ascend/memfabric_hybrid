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

#include <unordered_map>
#include "hybm_common_include.h"

namespace ock {
namespace mf {

class HybmEntityTagInfo {
public:
    Result TagInfoInit(hybm_options option);
    Result AddTagOpInfo(std::string info);

    Result AddRankTag(uint32_t rankId, std::string tag);
    Result RemoveRankTag(uint32_t rankId, std::string tag);
    std::string GetTagByRank(uint32_t rankId);
    hybm_data_op_type GetTag2TagOpType(std::string tag1, std::string tag2);
private:
    std::unordered_map<uint32_t, std::string> rankTagInfo_;
    std::unordered_map<std::string, hybm_data_op_type> tagOpInfo_;
};

}
}
#endif // MEM_FABRIC_HYBRID_HYBM_ENTITY_TAG_INFO_H
