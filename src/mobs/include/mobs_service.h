/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef __MEMFABRIC_MOBS_SERVICE_H__
#define __MEMFABRIC_MOBS_SERVICE_H__

#include "mobs_def.h"

#ifdef __cplusplus
extern "C" {
#endif

mobs_meta_service_t mobs_meta_service_start(mobs_meta_service_config_t *config);

void mobs_meta_service_stop(void);

mobs_local_service_t mobs_local_service_start(mobs_local_service_config_t *config);

void mobs_local_service_stop();

#ifdef __cplusplus
}
#endif

#endif //__MEMFABRIC_MOBS_SERVICE_H__
