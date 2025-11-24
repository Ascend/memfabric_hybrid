/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#include <mockcpp/mockcpp.hpp>
#include "smem.h"
#include "smem_bm.h"
#include "smem_bm_def.h"
#include "smem_shm.h"
#include "smem_shm_def.h"
#include "memcache_stubs.h"

// smem.h
int32_t smem_init_(uint32_t flags)
{
    return 0;
}

int32_t smem_set_extern_logger_(void (*func)(int level, const char *msg))
{
    return 0;
}

int32_t smem_set_log_level_(int level)
{
    if (level >= 0 && level <= 3) {
        return 0;
    }
    return -1;
}

void smem_uninit_()
{
    return;
}

const char *smem_get_last_err_msg_()
{
    return "this is a mock return of smem_get_last_err_msg";
}

const char *smem_get_and_clear_last_err_msg_()
{
    return "this is a mock return of smem_get_and_clear_last_err_msg";
}

void MockSmemH()
{
    MOCKER(smem_init).stubs().will(invoke(smem_init_));
    MOCKER(smem_set_extern_logger).stubs().will(invoke(smem_set_extern_logger_));
    MOCKER(smem_set_log_level).stubs().will(invoke(smem_set_log_level_));
    MOCKER(smem_uninit).stubs().will(invoke(smem_uninit_));
    MOCKER(smem_get_last_err_msg).stubs().will(invoke(smem_get_last_err_msg_));
    MOCKER(smem_get_and_clear_last_err_msg).stubs().will(invoke(smem_get_and_clear_last_err_msg_));
}

// smem_bm.h

int32_t smem_bm_config_init_(smem_bm_config_t *config)
{
    return 0;
}

int32_t smem_bm_init_(const char *storeURL, uint32_t worldSize, uint16_t deviceId, const smem_bm_config_t *config)
{
    return 0;
}

void smem_bm_uninit_(uint32_t flags)
{
    return;
}

uint32_t smem_bm_get_rank_id_(void)
{
    return 0;
}

smem_bm_t smem_bm_create_(uint32_t id, uint32_t memberSize, smem_bm_data_op_type dataOpType, uint64_t localDRAMSize,
                          uint64_t localHBMSize, uint32_t flags)
{
    return reinterpret_cast<uint8_t *>(1);
}

void smem_bm_destroy_(smem_bm_t handle)
{
    return;
}

int32_t smem_bm_join_(smem_bm_t handle, uint32_t flags)
{
    return 0;
}

int32_t smem_bm_leave_(smem_bm_t handle, uint32_t flags)
{
    return MAX_BUF_SIZE;
}

uint64_t smem_bm_get_local_mem_size_(smem_bm_t handle)
{
    return MAX_BUF_SIZE;
}

void *smem_bm_ptr_(smem_bm_t handle, uint16_t peerRankId)
{
    return reinterpret_cast<uint8_t *>(1);
}

uint64_t smem_bm_get_local_mem_size_by_mem_type_(smem_bm_t handle, smem_bm_mem_type memType)
{
    return MAX_BUF_SIZE;
}

void *smem_bm_ptr_by_mem_type_(smem_bm_t handle, smem_bm_mem_type memType, uint16_t peerRankId)
{
    return reinterpret_cast<uint8_t *>(1);
}

int32_t smem_bm_copy_(smem_bm_t handle, const void *src, void *dest, uint64_t size, smem_bm_copy_type t, uint32_t flags)
{
    return 0;
}

void MockSmemBmH()
{
    MOCKER(smem_bm_config_init).stubs().will(invoke(smem_bm_config_init_));
    MOCKER(smem_bm_init).stubs().will(invoke(smem_bm_init_));
    MOCKER(smem_bm_uninit).stubs().will(invoke(smem_bm_uninit_));
    MOCKER(smem_bm_get_rank_id).stubs().will(invoke(smem_bm_get_rank_id_));
    MOCKER(smem_bm_create).stubs().will(invoke(smem_bm_create_));
    MOCKER(smem_bm_destroy).stubs().will(invoke(smem_bm_destroy_));
    MOCKER(smem_bm_join).stubs().will(invoke(smem_bm_join_));
    MOCKER(smem_bm_leave).stubs().will(invoke(smem_bm_leave_));
    MOCKER(smem_bm_get_local_mem_size).stubs().will(invoke(smem_bm_get_local_mem_size_));
    MOCKER(smem_bm_ptr).stubs().will(invoke(smem_bm_ptr_));
    MOCKER(smem_bm_get_local_mem_size_by_mem_type).stubs().will(invoke(smem_bm_get_local_mem_size_by_mem_type_));
    MOCKER(smem_bm_ptr_by_mem_type).stubs().will(invoke(smem_bm_ptr_by_mem_type_));
    MOCKER(smem_bm_copy).stubs().will(invoke(smem_bm_copy_));
}

// smem_shm.h

int32_t smem_shm_config_init_(smem_shm_config_t *config)
{
    return 0;
}

int32_t smem_shm_init_(const char *configStoreIpPort, uint32_t worldSize, uint32_t rankId, uint16_t deviceId,
                       smem_shm_config_t *config)
{
    return 0;
}

void smem_shm_uninit_(uint32_t flags)
{
    return;
}

uint32_t smem_shm_query_support_data_operation_(void)
{
    return 0;
}

smem_shm_t smem_shm_create_(uint32_t id, uint32_t rankSize, uint32_t rankId, uint64_t symmetricSize,
                            smem_shm_data_op_type dataOpType, uint32_t flags, void **gva)
{
    return {};
}

int32_t smem_shm_destroy_(smem_shm_t handle, uint32_t flags)
{
    return 0;
}

int32_t smem_shm_set_extra_context_(smem_shm_t handle, const void *context, uint32_t size)
{
    return 0;
}

uint32_t smem_shm_get_global_rank_(smem_shm_t handle)
{
    return 0;
}

uint32_t smem_shm_get_global_rank_size_(smem_shm_t handle)
{
    return 1;
}

int32_t smem_shm_control_barrier_(smem_shm_t handle)
{
    return 0;
}

int32_t smem_shm_control_allgather_(smem_shm_t handle, const char *sendBuf, uint32_t sendSize, char *recvBuf,
                                    uint32_t recvSize)
{
    return 0;
}

int32_t smem_shm_topology_can_reach_(smem_shm_t handle, uint32_t remoteRank, uint32_t *reachInfo)
{
    return 0;
}

void MockSmemShmH()
{
    MOCKER(smem_shm_config_init).stubs().will(invoke(smem_shm_config_init_));
    MOCKER(smem_shm_init).stubs().will(invoke(smem_shm_init_));
    MOCKER(smem_shm_uninit).stubs().will(invoke(smem_shm_uninit_));
    MOCKER(smem_shm_query_support_data_operation).stubs().will(invoke(smem_shm_query_support_data_operation_));
    MOCKER(smem_shm_create).stubs().will(invoke(smem_shm_create_));
    MOCKER(smem_shm_destroy).stubs().will(invoke(smem_shm_destroy_));
    MOCKER(smem_shm_set_extra_context).stubs().will(invoke(smem_shm_set_extra_context_));
    MOCKER(smem_shm_get_global_rank).stubs().will(invoke(smem_shm_get_global_rank_));
    MOCKER(smem_shm_get_global_rank_size).stubs().will(invoke(smem_shm_get_global_rank_size_));
    MOCKER(smem_shm_control_barrier).stubs().will(invoke(smem_shm_control_barrier_));
    MOCKER(smem_shm_control_allgather).stubs().will(invoke(smem_shm_control_allgather_));
    MOCKER(smem_shm_topology_can_reach).stubs().will(invoke(smem_shm_topology_can_reach_));
}

// summary

void MockSmemAll()
{
    MockSmemH();
    MockSmemBmH();
    MockSmemShmH();
}
