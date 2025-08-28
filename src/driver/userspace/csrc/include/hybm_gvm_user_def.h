/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#ifndef MEMFABRIC_HYBM_GVM_USER_DEF_H
#define MEMFABRIC_HYBM_GVM_USER_DEF_H

#include <unistd.h>

#define HYBM_GVM_START_ADDR         0x700000000000ULL // 112T
#define HYBM_GVM_SDMA_START_ADDR    0x800000000000ULL // 128T
#define HYBM_GVM_RESERVE_SIZE       0x300000000000ULL  // 16T(RDMA)+32T(SDMA)

#define SIZE_1G (1ULL << 30)

#define DAVINIC_MODULE_NAME_MAX 256
#define DAVINCI_INTF_DEV_PATH   "/dev/davinci_manager"

/* Use 'Z' as magic number */
#define DAVINCI_INTF_IOC_MAGIC   'Z'
#define DAVINCI_INTF_IOCTL_OPEN  _IO(DAVINCI_INTF_IOC_MAGIC, 0)
#define DAVINCI_INTF_IOCTL_CLOSE _IO(DAVINCI_INTF_IOC_MAGIC, 1)

struct davinci_intf_open_arg {
    char module_name[DAVINIC_MODULE_NAME_MAX];
    int device_id;
};

struct davinci_intf_close_arg {
    char module_name[DAVINIC_MODULE_NAME_MAX];
    int device_id;
};

static inline const char *davinci_intf_get_dev_path(void)
{
    return DAVINCI_INTF_DEV_PATH;
}

#endif // MEMFABRIC_HYBM_GVM_USER_DEF_H
