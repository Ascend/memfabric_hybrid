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
#include "zbal_env_helper.h"
#include "zbal_functions.h"
#include "zbal_logger.h"

namespace zbal {
bool EnvHelper::PROF_ENABLED = false;
bool EnvHelper::OP_DEFAULT_STREAM = false;
uint32_t EnvHelper::PROF_TRACING_MAX_COUNT = 0;
std::string EnvHelper::PROF_DIR = "/home/";

void EnvHelper::Initialize() noexcept
{
    PROF_ENABLED = (Func::GetEnv<uint32_t>(ENV_NAME_PROF_SWITCH, 0) == 1);
    PROF_TRACING_MAX_COUNT = Func::GetEnv(ENV_NAME_PROF_TRACING_COUNT, 0);
    PROF_DIR = Func::GetEnv<std::string>(ENV_NAME_PROF_DIR, "/home/");
    OP_DEFAULT_STREAM = (Func::GetEnv<uint32_t>(ENV_NAME_OP_DEFUALT_STREAM, 0) == 1);
}

void EnvHelper::DumpEnv() noexcept
{
    ZBAL_LOG_DEBUG(ENV_NAME_PROF_SWITCH << " = " << PROF_ENABLED);
    ZBAL_LOG_DEBUG(ENV_NAME_PROF_TRACING_COUNT << " = " << PROF_TRACING_MAX_COUNT);
    ZBAL_LOG_DEBUG(ENV_NAME_PROF_DIR << " = " << PROF_DIR);
    ZBAL_LOG_DEBUG(ENV_NAME_OP_DEFUALT_STREAM << " = " << OP_DEFAULT_STREAM);
}
} // namespace zbal