#ifndef SHM_RDMA_TEST_DEV_H
#define SHM_RDMA_TEST_DEV_H

void shm_rdma_pollcq_test_do(void* stream, uint8_t* gva, uint64_t heap_size);
void shm_rdma_read_test_do(void* stream, uint8_t* gva, uint64_t heap_size);
void shm_rdma_write_test_do(void* stream, uint8_t* gva, uint64_t heap_size);
void shm_rdma_get_qpinfo_test_do(void* stream, uint8_t* gva, uint32_t rankId);

#endif // SHM_RDMA_TEST_DEV_H