/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "hybm_trans.h"
#include "hybm_define.h"
#include "hybm_types.h"
#include "hybm_logger.h"
#include "hybm_entity.h"

using namespace ock::mf;

HYBM_API int hybm_trans_init(hybm_entity_t e, uint32_t rankId, const std::string &nic)
{
    BM_ASSERT_RETURN(e != nullptr, BM_ERROR);
    auto entity = (MemEntity *) e;
    entity->TransportInit(rankId, nic);
    return BM_OK;
}

HYBM_API int hybm_trans_register_mr(hybm_entity_t e, uint64_t address, uint64_t size,
                                    hybm_mr_key *lkey, hybm_mr_key *rkey)
{
    BM_ASSERT_RETURN(e != nullptr, BM_ERROR);
    BM_ASSERT_RETURN(lkey != nullptr, BM_ERROR);
    BM_ASSERT_RETURN(rkey != nullptr, BM_ERROR);
    auto entity = (MemEntity *) e;
    entity->TransportRegisterMr(address, size, lkey, rkey);
    return BM_OK;
}

HYBM_API int hybm_trans_set_mrs(hybm_entity_t e, const std::vector<hybm_transport_mr_info> &mrs)
{
    BM_ASSERT_RETURN(e != nullptr, BM_ERROR);
    auto entity = (MemEntity *) e;
    entity->TransportSetMr(mrs);
    return BM_OK;
}

HYBM_API int hybm_trans_get_nic(hybm_entity_t e, std::string &nic)
{
    BM_ASSERT_RETURN(e != nullptr, BM_ERROR);
    auto entity = (MemEntity *) e;
    entity->TransportGetAddress(nic);
    return BM_OK;
}

HYBM_API int hybm_trans_set_nics(hybm_entity_t e, std::vector<std::string> nics)
{
    BM_ASSERT_RETURN(e != nullptr, BM_ERROR);
    auto entity = (MemEntity *) e;
    entity->TransportSetAddress(nics);
    return BM_OK;
}

HYBM_API int hybm_trans_make_connections(hybm_entity_t e)
{
    BM_ASSERT_RETURN(e != nullptr, BM_ERROR);
    auto entity = (MemEntity *) e;
    entity->TransportMakeConnect();
    return BM_OK;
}

HYBM_API int hybm_trans_ai_qp_info_address(hybm_entity_t e, uint32_t shmId, void **address)
{
    BM_ASSERT_RETURN(e != nullptr, BM_ERROR);
    auto entity = (MemEntity *) e;
    entity->TransportAiQPInfoAddress(shmId, address);
    return BM_OK;
}