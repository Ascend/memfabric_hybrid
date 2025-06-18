/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_TRANSPORT_H
#define MF_HYBRID_HYBM_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

int hybm_transport_init(uint32_t rankId, uint32_t rankCount);
int hybm_transport_get_address(uint64_t *address);
int hybm_transport_set_addresses(uint64_t addresses[], uint32_t count);
int hybm_transport_make_connections(void);
int hybm_transport_ai_qp_info_address(void **address);

#ifdef __cplusplus
}
#endif

#endif  // MF_HYBRID_HYBM_TRANSPORT_H
