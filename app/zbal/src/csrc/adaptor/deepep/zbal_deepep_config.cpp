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

#include "zbal_deepep_config.h"

namespace zbal {
namespace adaptor {
namespace deep_ep {
size_t get_low_latency_rdma_size_hint(int num_max_dispatch_tokens_per_rank, int hidden, int num_ranks, int num_experts)
{
    (void)hidden;
    (void)num_ranks;
    (void)num_experts;
    return num_max_dispatch_tokens_per_rank;
}

int get_value_from_env(const std::string &name, int defaultValue)
{
    int retValue = defaultValue;
    if (const char *rank_str = std::getenv(name.c_str())) {
        char *end;
        errno = 0;
        long val = std::strtol(rank_str, &end, 10);
        if (errno == ERANGE || *end != '\0' || !std::isdigit(*rank_str)) {
            return retValue;
        }
        retValue = static_cast<int>(val);
        return retValue;
    } else {
        return retValue;
    }
}
} // namespace deep_ep
} // namespace adaptor
} // namespace zbal
