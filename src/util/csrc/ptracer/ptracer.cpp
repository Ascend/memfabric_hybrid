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
#include "ptracer.h"
#include "ptracer_default.h"

#define PTRACER_API __attribute__((visibility("default")))
#define PTRACER_VALIDATE_RETURN(CONDITION, MSG, RETURN_VALUE) \
    do {                                                      \
        if (!(CONDITION)) {                                   \
            LastError::Set(MSG);                              \
            return RETURN_VALUE;                              \
        }                                                     \
    } while (0)

static std::mutex g_mutex;
static bool g_inited = false;
static int32_t g_tracerType = 0;
ptracer g_tracer = {};

namespace ock {
namespace mf {
namespace tracer {

int32_t InitDefaultTracer(const std::string &dumpDir, ptracer &tracer)
{
#ifdef ENABLE_PTRACER
    auto &service = DefaultTracer::GetInstance();
    if (service.StartUp(dumpDir) != 0) {
        return -1;
    }

    tracer.trace_begin = DefaultTracer::TraceBegin;
    tracer.trace_end = DefaultTracer::TraceEnd;
    tracer.current_time_ns = DefaultTracer::TimeNs;
    tracer.enabled = true;
    return 0;
#else
    tracer.enabled = false;
    return 0;
#endif
}

void UnInitDefaultTracer(ptracer &tracer)
{
#ifdef ENABLE_PTRACER
    DefaultTracer::GetInstance().ShutDown();
    tracer.enabled = false;
    tracer.trace_begin = nullptr;
    tracer.trace_end = nullptr;
    tracer.current_time_ns = nullptr;
#endif
}

}  // namespace tracer
}  // namespace mf
}  // namespace ock

PTRACER_API int32_t ptracer_init(ptracer_config_t *config)
{
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_inited) {
        return 0;
    }

    using namespace ock::mf::tracer;
    PTRACER_VALIDATE_RETURN(config != nullptr, "invalid config, which is null", -1);
    PTRACER_VALIDATE_RETURN(config->tracerType == 1, "invalid config.dumpType, only 1 is supported", -1);
    PTRACER_VALIDATE_RETURN(config->dumpFilePath != nullptr, "invalid param config.dumpFilePath is null", -1);

    int32_t result = 0;
    if (config->tracerType == 1) {
        result = InitDefaultTracer(config->dumpFilePath, g_tracer);
    }

    if (result == 0) {
        g_tracerType = config->tracerType;
        g_inited = true;
    }

    return result;
}

PTRACER_API void ptracer_uninit(void)
{
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_inited) {
        return;
    }

    if (g_tracerType) {
        ock::mf::tracer::UnInitDefaultTracer(g_tracer);
    }
    g_inited = false;
}

PTRACER_API const char *ptracer_get_last_err_msg(void)
{
    return ock::mf::tracer::LastError::Get();
}