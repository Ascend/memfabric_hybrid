/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __MMC_LEADER_API_H__
#define __MMC_LEADER_API_H__

namespace ock {
namespace mmc {

using MmcLeaderInitFunc = int32_t (*)();
using MmcLeaderUnInitFunc = void (*)();
using ExternalLog = void (*)(int, const char *);
using MmcLeaderSetLoggerFunc = void (*)(ExternalLog func);

class MmcLeaderApi {
public:
    static Result LoadLibrary(const std::string &libDirPath);

    static int32_t MmcLeaderInit()
    {
        return gLeaderInit();
    }

    static void MmcLeaderUninit()
    {
        gLeaderUnInit();
    };

    static void MmcLeaderSetLogger(ExternalLog func)
    {
        gLeaderSetLogger(func);
    };

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *gLeaderLibHandle;
    static const char *gLeaderLibName;

    static MmcLeaderInitFunc gLeaderInit;
    static MmcLeaderUnInitFunc gLeaderUnInit;
    static MmcLeaderSetLoggerFunc gLeaderSetLogger;
};
}
}

#endif  // __MMC_LEADER_API_H__
