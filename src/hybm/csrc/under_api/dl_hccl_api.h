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
#ifndef MF_HYBM_CORE_DL_HCCL_API_H
#define MF_HYBM_CORE_DL_HCCL_API_H

#include "hybm_common_include.h"

namespace ock {
namespace mf {

const uint32_t COMM_NAME_MAX_LENGTH = 128; // group name max length
const uint32_t UDI_MAX_LENGTH = 128; // UDI max length
const uint32_t HCCL_COMM_CONFIG_INFO_BYTES = 24;
const uint32_t HCCL_COMM_CONFIG_MAGIC_WORD = 0xf0f0f0f0;
const uint32_t HCCL_COMM_CONFIG_VERSION = 10;

const uint32_t HCCL_COMM_CONFIG_PAD_LEN = 4096;
typedef struct HcclCommConfigDef {
    char reserved[HCCL_COMM_CONFIG_INFO_BYTES];
    uint32_t hcclBufferSize;
    uint32_t hcclDeterministic;
    char hcclCommName[COMM_NAME_MAX_LENGTH];
    char hcclUdi[UDI_MAX_LENGTH];
    uint32_t hcclOpExpansionMode;   // 0:默认值  1:host  2:aicpu  3:aiv
    uint32_t hcclRdmaTrafficClass;
    uint32_t hcclRdmaServiceLevel;

    char pad[HCCL_COMM_CONFIG_PAD_LEN];
} HcclCommConfig;

typedef struct {
    size_t size;
    uint32_t magicWord;
    uint32_t version;
    uint64_t reserved;
} configInfo_t;

using HcclComm = void*;

using hcclCommInitClusterInfoFunc = int32_t (*)(const char *, uint32_t , HcclCommConfig *, HcclComm *);
using hcclCommDestroyFunc = int32_t (*)(HcclComm);

class DlHcclApi {
public:
    static Result LoadLibrary();
    static void CleanupLibrary();

    static inline void HcclCommConfigInit(HcclCommConfig *config)
    {
        if (config == nullptr) {
            return;
        }

        auto info = reinterpret_cast<configInfo_t *>(config);
        info->size = sizeof(HcclCommConfig);
        info->magicWord = HCCL_COMM_CONFIG_MAGIC_WORD;
        info->version = HCCL_COMM_CONFIG_VERSION;
        info->reserved = 0;

        config->hcclBufferSize = UINT32_MAX;
        config->hcclDeterministic = UINT32_MAX;
        config->hcclCommName[0] = '\0';
        config->hcclUdi[0] = '\0';
        config->hcclOpExpansionMode = 0;
        config->hcclRdmaTrafficClass = UINT32_MAX;
        config->hcclRdmaServiceLevel = UINT32_MAX;
    }

    static inline Result HcclCommInitClusterInfoMemConfig(const char *rankTable, uint32_t rank,
                                                          HcclCommConfig *config, HcclComm *comm)
    {
        if (gHcclCommInitClusterInfo == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcclCommInitClusterInfo(rankTable, rank, config, comm);
    }

    static inline Result HcclCommDestroy(HcclComm comm)
    {
        if (gHcclCommDestroy == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcclCommDestroy(comm);
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *hcclHandle;

    static hcclCommInitClusterInfoFunc gHcclCommInitClusterInfo;
    static hcclCommDestroyFunc gHcclCommDestroy;
};

} // namespace mf
} // namespace ock

#endif // MF_HYBM_CORE_DL_HCCL_API_H
