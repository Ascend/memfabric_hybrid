/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBM_CORE_DL_HCCL_API_H
#define MF_HYBM_CORE_DL_HCCL_API_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {

using hcclGetRootInfoFunc = int (*)(HcclRootInfo *);
using hcclCommInitRootInfoFunc = int (*)(uint32_t, const HcclRootInfo *, uint32_t, HcclComm *);

class RuntimeHcclApi {
public:
    static Result LoadLibrary();

    static inline int HcclGetRootInfo(HcclRootInfo *rootInfo)
    {
        return gHcclGetRootInfo(rootInfo);
    }

    static inline int HcclCommInitRootInfo(uint32_t nRanks, const HcclRootInfo *rootInfo, uint32_t rank, HcclComm *comm)
    {
        return gHcclCommInitRootInfo(nRanks, rootInfo, rank, comm);
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *hcclHandle;
    static const char *gHcclLibName;

    static hcclGetRootInfoFunc gHcclGetRootInfo;
    static hcclCommInitRootInfoFunc gHcclCommInitRootInfo;
};
}
}

#endif  // MF_HYBM_CORE_DL_HCCL_API_H
