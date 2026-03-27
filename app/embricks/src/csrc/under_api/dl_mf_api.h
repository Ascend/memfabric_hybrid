/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Embricks is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MEMFABRIC_HYBRID_EMB_DL_MF_API_H
#define MEMFABRIC_HYBRID_EMB_DL_MF_API_H

#include "emb_common_includes.h"
#include "smem_bm_def.h"

namespace ock {
namespace emb {
namespace underapi {

using mfSmemInitFunc = int32_t (*)(uint32_t);
using mfSmemCreateConfigStoreFunc = int32_t (*)(const char *);
using mfSmemSetExternLoggerFunc = int32_t (*)(void (*func)(int level, const char *msg));
using mfSmemSetLogLevelFunc = int32_t (*)(int);
using mfSmemUnInitFunc = void (*)(void);
using mfSmemGetLastErrMsgFunc = const char *(*)(void);
using mfSmemGetAndClearErrMsgFunc = const char *(*)(void);

class DlMfApi {
public:
    static Result LoadLibrary(const std::string &libDirPath);
    static void CleanupLibrary();

    DlMfApi() = delete;
    ~DlMfApi() = delete;

    static Result SmemInit(uint32_t flags);
    static Result SmemSetExternLogger(void (*func)(int level, const char *msg));
    static Result SmemSetLoggerLevel(int level);
    static void SmemUnInit(void);
    static const char *SmemGetLastErrMsg(void);
    static const char *SmemGetAndClearLastErrMsg(void);

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *gMfSmemHandle;
    static const char *gMfLibName;

    static mfSmemInitFunc gMfSmemInit;
    static mfSmemCreateConfigStoreFunc gMfSmemCreateConfigStore;
    static mfSmemSetExternLoggerFunc gMfSmemSetExternLogger;
    static mfSmemSetLogLevelFunc gMfSmemSetLogLevel;
    static mfSmemUnInitFunc gMfSmemUnInit;
    static mfSmemGetLastErrMsgFunc gMfSmemGetLastErrMsg;
    static mfSmemGetAndClearErrMsgFunc gMfSmemGetAndClearErrMsg;
};

inline Result DlMfApi::SmemInit(uint32_t flags)
{
    EM_ASSERT_RETURN(gMfSmemInit != nullptr, EM_DL_FUNCTION_NOT_LOADED);
    return gMfSmemInit(flags);
}

inline Result DlMfApi::SmemSetExternLogger(void (*func)(int level, const char *msg))
{
    EM_ASSERT_RETURN(gMfSmemSetExternLogger != nullptr, EM_DL_FUNCTION_NOT_LOADED);
    return gMfSmemSetExternLogger(func);
}

inline Result DlMfApi::SmemSetLoggerLevel(int level)
{
    EM_ASSERT_RETURN(gMfSmemSetLogLevel != nullptr, EM_DL_FUNCTION_NOT_LOADED);
    return gMfSmemSetLogLevel(level);
}

inline void DlMfApi::SmemUnInit(void)
{
    EM_ASSERT_RET_VOID(gMfSmemUnInit != nullptr);
    gMfSmemUnInit();
}

inline const char *DlMfApi::SmemGetLastErrMsg(void)
{
    EM_ASSERT_RETURN(gMfSmemGetLastErrMsg != nullptr, "");
    return gMfSmemGetLastErrMsg();
}

inline const char *DlMfApi::SmemGetAndClearLastErrMsg(void)
{
    EM_ASSERT_RETURN(gMfSmemGetAndClearErrMsg != nullptr, "");
    return gMfSmemGetAndClearErrMsg();
}
} // namespace underapi
} // namespace emb
} // namespace ock

#endif // MEMFABRIC_HYBRID_EMB_DL_MF_API_H
