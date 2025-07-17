/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_TRANSPORT_H
#define MF_HYBRID_HYBM_TRANSPORT_H

#include <vector>
#include <string>
#include "hybm_def.h"

typedef struct {
    uint8_t key[64];
} hybm_mr_key;

struct hybm_transport_mr_info {
    uint64_t size; // size of the memory region
    uint64_t addr; // start address of the memory region
    hybm_mr_key lkey; // key of the memory region
    hybm_mr_key rkey;
};

int hybm_trans_init(hybm_entity_t e, uint32_t rankId, const std::string &nic);
int hybm_trans_register_mr(hybm_entity_t e, uint64_t address, uint64_t size, hybm_mr_key *lkey, hybm_mr_key *rkey);
int hybm_trans_set_mrs(hybm_entity_t e, const std::vector<hybm_transport_mr_info> &mrs);
int hybm_trans_get_nic(hybm_entity_t e, std::string &nic);
int hybm_trans_set_nics(hybm_entity_t e, std::vector<std::string> nics);
int hybm_trans_make_connections(hybm_entity_t e);
int hybm_trans_ai_qp_info_address(hybm_entity_t e, uint32_t shmId, void **address);

#endif  // MF_HYBRID_HYBM_TRANSPORT_H
