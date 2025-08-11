/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MEM_FABRIC_HYBRID_DEVMM_IOCTL_H
#define MEM_FABRIC_HYBRID_DEVMM_IOCTL_H

#include <cstdint>

constexpr auto DEVMM_MAX_NAME_SIZE = 65U;
constexpr auto DAVINIC_MODULE_NAME_MAX = 256U;

struct DevmmCommandHead {
    uint32_t logicDevId;
    uint32_t devId;
    uint32_t vFid;
};

struct DAVINCI_INTF_OPEN_ARAG { // davinci_intf_open_arg
    char module[DAVINIC_MODULE_NAME_MAX];
    int device_id;
};

struct davinci_intf_close_arg {
    char module_name[DAVINIC_MODULE_NAME_MAX];
    int device_id;
};


struct DevmmCommandOpenParam {
    uint64_t vptr;
    char name[DEVMM_MAX_NAME_SIZE];
};

struct DevmmMemTranslateParam {
    uint64_t vptr;
    uint64_t pptr;
    uint32_t addrInDevice;
};

struct DevmmMemAdvisePara {
    uint64_t ptr;
    size_t count;
    uint32_t advise;
};

struct DevmmMemQuerySizePara {
    char name[DEVMM_MAX_NAME_SIZE];
    int32_t isHuge;
    size_t len;
};

struct DevmmMemAllocPara {
    uint64_t p;
    size_t size;
};

struct DevmmFreePagesPara {
    uint64_t va;
};

struct DevmmCommandMessage {
    DevmmCommandHead head;
    union {
        DevmmCommandOpenParam openParam;
        DevmmMemTranslateParam translateParam;
        DevmmMemAdvisePara prefetchParam;
        DevmmMemQuerySizePara queryParam;
        DevmmMemAllocPara allocSvmPara;
        DevmmMemAdvisePara advisePara;
        DevmmFreePagesPara freePagesPara;
    } data;
};

#endif // MEM_FABRIC_HYBRID_DEVMM_IOCTL_H
