/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#ifndef MF_HYBRID_HYBM_GVM_USER_H
#define MF_HYBRID_HYBM_GVM_USER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYBM_GVM_SUCCESS (0)
#define HYBM_GVM_FAILURE (-1)

int32_t hybm_gvm_init(uint64_t device_id);
void hybm_gvm_deinit(void);
int32_t hybm_gvm_get_device_info(uint32_t *ssid);
int32_t hybm_gvm_reserve_memory(uint64_t *addr, uint64_t size, bool shared);
int32_t hybm_gvm_unreserve_memory(void);
int32_t hybm_gvm_mem_fetch(uint64_t addr, uint64_t size, uint32_t sdid);
int32_t hybm_gvm_mem_register(uint64_t addr, uint64_t size, uint64_t new_addr);
uint64_t hybm_gvm_mem_has_registered(uint64_t addr, uint64_t size);
int32_t hybm_gvm_mem_alloc(uint64_t addr, uint64_t size);
int32_t hybm_gvm_mem_free(uint64_t addr);
int32_t hybm_gvm_get_key(uint64_t addr, uint64_t *key);
int32_t hybm_gvm_set_whitelist(uint64_t key, uint32_t sdid);
int32_t hybm_gvm_mem_open(uint64_t addr, uint64_t key);
int32_t hybm_gvm_mem_close(uint64_t addr);

#ifdef __cplusplus
}
#endif

#endif // MF_HYBRID_HYBM_GVM_USER_H
