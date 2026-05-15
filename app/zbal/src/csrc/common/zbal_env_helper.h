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
#ifndef ZBAL_ENV_HELPER_H
#define ZBAL_ENV_HELPER_H

#include <cstdlib>
#include <cstdint>
#include <string>

/* env variable for perf */
#define ENV_NAME_PROF_SWITCH        "ZBAL_PROF_ENABLE"
#define ENV_NAME_PROF_TRACING_COUNT "ZBAL_PROF_MAX_TRACING_COUNT"
#define ENV_NAME_PROF_DIR           "ZBAL_PROF_DIR"
#define ENV_NAME_HCCL_OP            "ZBAL_HCCL_OP"
#define ENV_NAME_OP_DEFUALT_STREAM  "ZBAL_OP_DEFUALT_STREAM"

namespace zbal {
class EnvHelper {
public:
    static void Initialize() noexcept;
    static void DumpEnv() noexcept;

public:
    static bool PROF_ENABLED;               /* if the perf enabled, any str is ok */
    static bool OP_DEFAULT_STREAM;          /* aiv comm with default stream */
    static uint32_t PROF_TRACING_MAX_COUNT; /* max tracing count of perf */
    static std::string PROF_DIR;            /* the dir of zbal perf */
};
} // namespace zbal

#endif // ZBAL_ENV_HELPER_H
