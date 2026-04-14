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
#ifndef SHM_RDMA_TEST_DEV_H
#define SHM_RDMA_TEST_DEV_H

void shm_rdma_pollcq_test_do(void *stream, uint8_t *gva, uint64_t heap_size);
void shm_rdma_read_test_do(void *stream, uint8_t *gva, uint64_t heap_size);
void shm_rdma_write_test_do(void *stream, uint8_t *gva, uint64_t heap_size);
void shm_rdma_get_qpinfo_test_do(void *stream, uint8_t *gva, uint32_t rankId);

#endif // SHM_RDMA_TEST_DEV_H