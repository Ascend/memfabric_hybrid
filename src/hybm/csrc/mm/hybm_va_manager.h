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
#ifndef MEM_FABRIC_HYBRID_HYBM_VA_MANAGER_H
#define MEM_FABRIC_HYBRID_HYBM_VA_MANAGER_H

#include "hybm_common_include.h"
#include "hybm_mem_common.h"

namespace ock {
namespace mf {

class HybmVaManager {
public:
    // TO_DO: 单例
    Result AddOneVaInfo(uint64_t gva, uint64_t lva, uint64_t size, uint32_t rank, MemType memType = MEM_TYPE_BUTT);
    Result RemoveOneVaInfo(uint64_t gva);
    Result RemoveAllVaInfoByRank(uint32_t rank);

    uint64_t GetGvaByLva(uint64_t lva);
    uint64_t GetLvaByGva(uint64_t gva);

    uint64_t AllocReserveGva(uint64_t size, uint32_t memType); // TO_DO:可以把rank信息带入,接口再看看
    void FreeReserveGva(uint64_t gva);

    bool IsGva(uint64_t va);
    MemType GetMemType(uint64_t va); // support lva or gva
    uint32_t GetRank(uint64_t va);
    bool IsValidAddr(uint64_t va);

private:
    std::mutex lock_; // support multi threads
    std::map<uint64_t, uint32_t> vaInfo_;
};

}
}
#endif // MEM_FABRIC_HYBRID_HYBM_VA_MANAGER_H
