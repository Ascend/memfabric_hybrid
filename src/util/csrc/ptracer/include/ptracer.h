/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_PTRACER_H
#define MEM_FABRIC_PTRACER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief ptracer (i.e. performance tracer) interface for extension
 */
typedef struct {
    bool enabled;                                                                /* if the tracer enabled  */
    void (*trace_begin)(uint32_t traceId, const char *traceName);                /* function hook to start recording */
    void (*trace_end)(uint32_t traceId, const uint64_t diff, int32_t goodOrBad); /* function hook to stop recording */
    uint64_t (*current_time_ns)();                                               /* function hook to get time in ns */
} ptracer;

extern ptracer g_tracer;

typedef struct {
    int32_t tracerType;       /* 1 default tracer with file dumper */
    const char *dumpFilePath; /* dir path of dump file */
} ptracer_config_t;

int32_t ptracer_init(ptracer_config_t *config);
void ptracer_uninit(void);
const char *ptracer_get_last_err_msg(void);
const char *ptracer_get_all_tp_string(void);

/**
 * @brief Start to trace
 * @param TP_ID            [in] trace id defined with macro PTRACER_ID
 */
#define TP_TRACE_BEGIN(TP_ID)                        \
    uint64_t tpBegin##TP_ID = 0;                     \
    if (g_tracer.enabled) {                          \
        g_tracer.trace_begin(TP_ID, #TP_ID);         \
        tpBegin##TP_ID = g_tracer.current_time_ns(); \
    }

/**
 * @brief End to trace, this should be in the same thread with PTRACER_ID
 * @param TP_ID            [in] trace id defined with macro PTRACER_ID
 * @param GOOD_BAD         [in] good or bad result to be record, used to counting function
 *                              execution result beside the time cost, 0 means good, other
 *                              value means bad, bad execution will be not record into
 *                              performance counting
 */
#define TP_TRACE_END(TP_ID, GOOD_BAD)                                                     \
    if (g_tracer.enabled) {                                                               \
        g_tracer.trace_end(TP_ID, g_tracer.current_time_ns() - tpBegin##TP_ID, GOOD_BAD); \
    }

#define TP_TRACE_TRACE_BEGIN(TP_ID, P_U64_TIME_NS)       \
    if (g_tracer.enabled) {                              \
        g_tracer.trace_begin(TP_ID, #TP_ID);             \
        (*(P_U64_TIME_NS)) = g_tracer.current_time_ns(); \
    }

#define TP_TRACE_TRACE_END(TP_ID, U64_TIME_NS, GOOD_BAD)                                   \
    if (g_tracer.enabled) {                                                                \
        g_tracer.trace_end(TP_ID, (g_tracer.current_time_ns() - (U64_TIME_NS)), GOOD_BAD); \
    }

#define TP_TRACE_RECORD(TP_ID, U64_DIFF_TIME_NS, GOOD_BAD)       \
    if (g_tracer.enabled) {                                      \
        g_tracer.trace_begin(TP_ID, #TP_ID);                     \
        g_tracer.trace_end(TP_ID, (U64_DIFF_TIME_NS), GOOD_BAD); \
    }

#define TP_CURRENT_TIME_NS (g_tracer.current_time_ns ? g_tracer.current_time_ns() : 0)

#define PTRACER_ID(MODULE_ID_, traceId_) ((MODULE_ID_) << 16 | ((traceId_) & 0xFFFF))

#ifdef __cplusplus
}
#endif
#endif // MEM_FABRIC_PTRACER_H