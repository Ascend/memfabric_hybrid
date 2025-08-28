/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_DL_HCCP_DEF_H
#define MF_HYBRID_DL_HCCP_DEF_H

#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <cstdint>
#include <string>
#include <ostream>
#include <iomanip>
#include <sstream>

namespace ock {
namespace mf {

#define HOST_LITE_RESERVED 4
#define RA_MR_MAX_NUM 8

struct ra_rdma_ops;
struct rdma_lite_cq;
struct rdma_lite_cq;
struct rdma_lite_qp;
struct ra_rdma_handle;
struct rdma_lite_wc;
struct lite_mr_info {
    uint32_t key;
    uint64_t addr;
    uint64_t len;
};

struct cqe_err_info {
    uint32_t status;
    uint32_t qpn;
    struct timeval time;
};

struct ra_cqe_err_info {
    pthread_mutex_t mutex;
    struct cqe_err_info info;
};

struct ra_list_head {
    struct ra_list_head *next, *prev;
};

struct ra_qp_handle {
    unsigned int qpn;
    int qp_mode;
    int flag;
    unsigned int phy_id;
    unsigned int rdev_index;
    struct ra_rdma_ops *rdma_ops;  // only ra use
    int support_lite;
    struct rdma_lite_cq *send_lite_cq;
    struct rdma_lite_cq *recv_lite_cq;
    struct rdma_lite_qp *lite_qp;
    struct lite_mr_info local_mr[RA_MR_MAX_NUM];
    struct lite_mr_info rem_mr[RA_MR_MAX_NUM];
    pthread_mutex_t qp_mutex;
    struct ra_cqe_err_info cqe_err_info;
    int db_index;
    unsigned int send_wr_num;
    unsigned int poll_cqe_num;
    unsigned int recv_wr_num;
    unsigned int poll_recv_cqe_num;
    struct ra_list_head list;
    struct ra_rdma_handle *rdma_handle;
    struct rdma_lite_wc *lite_wc;
    unsigned int mem_idx;
    int sq_sig_all;
    unsigned int udp_sport;
    unsigned int psn;
    unsigned int gid_idx;
};
}
}

#endif  // MF_HYBRID_DL_HCCP_DEF_H
