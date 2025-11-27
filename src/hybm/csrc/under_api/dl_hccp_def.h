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

constexpr int HOST_LITE_RESERVED = 4;
constexpr int RA_MR_MAX_NUM = 8;
constexpr uint32_t HCCL_ROOT_INFO_BYTES = 256; // 4108: root info length
constexpr uint32_t HCCP_SOCK_CONN_TAG_SIZE = 192;
constexpr uint32_t HCCP_MAX_INTERFACE_NAME_LEN = 256;

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

/**
 * @brief HCCL root info
 */
struct HcclRootInfo {
    char internal[HCCL_ROOT_INFO_BYTES];
};

struct HccpRaInitConfig {
    uint32_t phyId;        /**< physical device id */
    uint32_t nicPosition;  /**< reference to HccpNetworkMode */
    int hdcType;           /**< reference to drvHdcServiceType */
    bool enable_hdc_async; /**< true will init an extra HDC session for async APIs */
};

/**
 * @ingroup libinit
 * ip address
 */
union HccpIpAddr {
    struct in_addr addr;
    struct in6_addr addr6;
};

struct HccpRdevInitInfo {
    int mode;
    uint32_t notifyType;
    bool enabled910aLite;    /**< true will enable 910A lite, invalid if enabled_2mb_lite is false; default is false */
    bool disabledLiteThread; /**< true will not start lite thread, flag invalid if enabled_910a/2mb_lite is false */
    bool enabled2mbLite;     /**< true will enable 2MB lite(include 910A & 910B), default is false */
};

/**
 * @ingroup libinit
 * hccp operating environment
 */
enum HccpNetworkMode {
    NETWORK_PEER_ONLINE = 0, /**< Third-party online mode */
    NETWORK_OFFLINE,         /**< offline mode */
    NETWORK_ONLINE,          /**< online mode */
};

/**
 * @ingroup librdma
 * Flag of mr access
 */
enum HccpMrAccessFlags {
    RA_ACCESS_LOCAL_WRITE = 1,         /**< mr local write access */
    RA_ACCESS_REMOTE_WRITE = (1 << 1), /**< mr remote write access */
    RA_ACCESS_REMOTE_READ = (1 << 2),  /**< mr remote read access */
    RA_ACCESS_NORMAL = 7,              // RA_ACCESS_LOCAL_WRITE | RA_ACCESS_REMOTE_WRITE | RA_ACCESS_REMOTE_READ
    RA_ACCESS_REDUCE = (1 << 8),
};

enum HccpNotifyType {
    NO_USE = 0,
    NOTIFY = 1,
    EVENTID = 2,
};

/**
 * @ingroup libsocket
 * struct of the client socket
 */
struct HccpSocketConnectInfo {
    void *handle;                      /**< socket handle */
    HccpIpAddr remoteIp;               /**< IP address of remote socket, [0-7] is reserved for vnic */
    uint16_t port;                     /**< Socket listening port number */
    char tag[HCCP_SOCK_CONN_TAG_SIZE]; /**< tag must ended by '\0' */
};

/**
 * @ingroup libsocket
 * Details about socket after socket is linked
 */
struct HccpSocketCloseInfo {
    void *handle; /**< socket handle */
    void *fd;     /**< fd handle */
    int linger;   /**< 0:use(default l_linger is RS_CLOSE_TIMEOUT), others:disuse */
};

/**
 * @ingroup libsocket
 * struct of the listen info
 */
struct HccpSocketListenInfo {
    void *handle;       /**< socket handle */
    unsigned int port;  /**< Socket listening port number */
    unsigned int phase; /**< refer to enum listen_phase */
    unsigned int err;   /**< errno */
};

/**
 * @ingroup libsocket
 * Details about socket after socket is linked
 */
struct HccpSocketInfo {
    void *handle;                      /**< socket handle */
    void *fd;                          /**< fd handle */
    HccpIpAddr remoteIp;               /**< IP address of remote socket */
    int status;                        /**< socket status:0 not connected 1:connected 2:connect timeout 3:connecting */
    char tag[HCCP_SOCK_CONN_TAG_SIZE]; /**< tag must ended by '\0' */
};

/**
 * @ingroup libinit
 * hccp init info
 */
struct HccpRdev {
    uint32_t phyId; /**< physical device id */
    int family;     /**< AF_INET(ipv4) or AF_INET6(ipv6) */
    HccpIpAddr localIp;
};

struct HccpRaGetIfAttr {
    uint32_t phyId;       /**< physical device id */
    uint32_t nicPosition; /**< reference to network_mode */
    bool isAll; /**< valid when nic_position is NETWORK_OFFLINE. false: get specific rnic ip, true: get all rnic ip */
};

struct HccpIfaddrInfo {
    HccpIpAddr ip;       /* Address of interface */
    struct in_addr mask; /* Netmask of interface */
};

struct HccpInterfaceInfo {
    int family;
    int scopeId;
    HccpIfaddrInfo ifaddr;                    /* Address and netmask of interface */
    char ifname[HCCP_MAX_INTERFACE_NAME_LEN]; /* Name of interface */
};

struct HccpSocketWhiteListInfo {
    HccpIpAddr remoteIp;               /**< IP address of remote */
    uint32_t connLimit;                /**< limit of whilte list */
    char tag[HCCP_SOCK_CONN_TAG_SIZE]; /**< tag used for whitelist must ended by '\0' */
};

struct HccpMrInfo {
    void *addr;              /**< starting address of mr */
    unsigned long long size; /**< size of mr */
    int access;              /**< access of mr, reference to HccpMrAccessFlags */
    unsigned int lkey;       /**< local addr access key */
    unsigned int rkey;       /**< remote addr access key */
};

struct HccpCqExtAttr {
    int sendCqDepth;
    int recvDqDepth;
    int sendCqCompVector;
    int recvCqCompVector;
};

enum ibv_qp_type {
    IBV_QPT_RC = 2,
    IBV_QPT_UC,
    IBV_QPT_UD,
    IBV_QPT_RAW_PACKET = 8,
    IBV_QPT_XRC_SEND = 9,
    IBV_QPT_XRC_RECV,
    IBV_QPT_DRIVER = 0xff,
};

struct ibv_qp_cap {
    uint32_t max_send_wr;
    uint32_t max_recv_wr;
    uint32_t max_send_sge;
    uint32_t max_recv_sge;
    uint32_t max_inline_data;
};

struct ibv_qp_init_attr {
    void *qp_context;
    struct ibv_cq *send_cq;
    struct ibv_cq *recv_cq;
    struct ibv_srq *srq;
    struct ibv_qp_cap cap;
    enum ibv_qp_type qp_type;
    int sq_sig_all;
};

union ai_data_plane_cstm_flag {
    struct {
        uint32_t cq_cstm : 1; // 0: hccp poll cq; 1: caller poll cq
        uint32_t reserved : 31;
    } bs;
    uint32_t value;
};

struct HccpQpExtAttrs {
    int qpMode;
    // cq attr
    HccpCqExtAttr cqAttr;
    // qp attr
    struct ibv_qp_init_attr qp_attr;
    // version control and reserved
    int version;
    int mem_align; // 0,1:4KB, 2:2MB
    uint32_t udp_sport;
    union ai_data_plane_cstm_flag data_plane_flag; // only valid in ra_ai_qp_create
    uint32_t reserved[29];
};

struct ai_data_plane_wq {
    unsigned wqn;
    unsigned long long buf_addr;
    unsigned int wqebb_size;
    unsigned int depth;
    unsigned long long head_addr;
    unsigned long long tail_addr;
    unsigned long long swdb_addr;
    unsigned long long db_reg;
    unsigned int reserved[8U];
};

struct ai_data_plane_cq {
    unsigned int cqn;
    unsigned long long buf_addr;
    unsigned int cqe_size;
    unsigned int depth;
    unsigned long long head_addr;
    unsigned long long tail_addr;
    unsigned long long swdb_addr;
    unsigned long long db_reg;
    unsigned int reserved[2U];
};

struct ai_data_plane_info {
    struct ai_data_plane_wq sq;
    struct ai_data_plane_wq rq;
    struct ai_data_plane_cq scq;
    struct ai_data_plane_cq rcq;
    unsigned int reserved[8U];
};

struct HccpAiQpInfo {
    unsigned long long aiQpAddr; // refer to struct ibv_qp *
    unsigned int sqIndex;        // index of sq
    unsigned int dbIndex;        // index of db

    // below cq related info valid when data_plane_flag.bs.cq_cstm was 1
    unsigned long long ai_scq_addr; // refer to struct ibv_cq *scq
    unsigned long long ai_rcq_addr; // refer to struct ibv_cq *rcq
    struct ai_data_plane_info data_plane_info;
};

enum class DBMode : int32_t { INVALID_DB = -1, HW_DB = 0, SW_DB };

struct AiQpRMAWQ {
    uint32_t wqn{0};
    uint64_t bufAddr{0};
    uint32_t wqeSize{0};
    uint32_t depth{0};
    uint64_t headAddr{0};
    uint64_t tailAddr{0};
    DBMode dbMode{DBMode::INVALID_DB}; // 0-hw/1-sw
    uint64_t dbAddr{0};
    uint32_t sl{0};
};

struct AiQpRMACQ {
    uint32_t cqn{0};
    uint64_t bufAddr{0};
    uint32_t cqeSize{0};
    uint32_t depth{0};
    uint64_t headAddr{0};
    uint64_t tailAddr{0};
    DBMode dbMode{DBMode::INVALID_DB}; // 0-hw/1-sw
    uint64_t dbAddr{0};
};

struct RdmaMemRegionInfo {
    uint64_t size{0}; // size of the memory region
    uint64_t addr{0}; // start address of the memory region
    uint32_t lkey{0};
    uint32_t rkey{0}; // key of the memory region
};

struct AiQpRMAQueueInfo {
    uint32_t count;
    struct AiQpRMAWQ *sq;
    struct AiQpRMAWQ *rq;
    struct AiQpRMACQ *scq;
    struct AiQpRMACQ *rcq;
    RdmaMemRegionInfo *mr;
};

/**
 * @ingroup librdma
 * Scatter and gather element
 */
struct sg_list {
    uint64_t addr; /**< address of buf */
    uint32_t len;  /**< len of buf */
    uint32_t lkey; /**< local addr access key */
};

/**
 * @ingroup librdma
 * RDMA work request
 */
struct send_wr {
    struct sg_list *buf_list; /**< list of sg */
    uint16_t buf_num;         /**< num of buf_list */
    uint64_t dst_addr;        /**< destination address */
    uint32_t rkey;            /**< remote address access key */
    uint32_t op;              /**< operations of RDMA supported:RDMA_WRITE:0 */
    int send_flag;            /**< reference to ra_send_flags */
};

struct wr_aux_info {
    uint8_t data_type;
    uint8_t reduce_type;
    uint32_t notify_offset;
};

struct wr_ext_info {
    uint32_t imm_data;
    uint16_t resv;
};

struct send_wr_v2 {
    uint64_t wr_id;           /**< user assigned work request ID */
    struct sg_list *buf_list; /**< list of sg */
    uint16_t buf_num;         /**< num of buf_list */
    uint64_t dst_addr;        /**< destination address */
    uint32_t rkey;            /**< remote address access key */
    uint32_t op;              /**< operations of RDMA supported:RDMA_WRITE:0 */
    int send_flag;            /**< reference to ra_send_flags */
    union {
        struct wr_aux_info aux; /**< aux info */
        struct wr_ext_info ext; /**< ext info */
    };
};

/**
 * @ingroup librdma
 * wqe template info
 */
struct wqe_info {
    unsigned int sq_index;  /**< index of sq */
    unsigned int wqe_index; /**< index of wqe */
};

enum ra_send_flags {
    RA_SEND_FENCE = 1 << 0,     /**< RDMA operation with fence */
    RA_SEND_SIGNALED = 1 << 1,  /**< RDMA operation with signaled */
    RA_SEND_SOLICITED = 1 << 2, /**< RDMA operation with solicited */
    RA_SEND_INLINE = 1 << 3,    /**< RDMA operation with inline */
};
/**
 * @ingroup librdma
 * doorbell info
 */
struct db_info {
    unsigned int db_index; /**< index of db */
    unsigned long db_info; /**< db content */
};

/**
 * @ingroup librdma
 * respond of sending work request
 */
struct send_wr_rsp {
    union {
        struct wqe_info wqe_tmp; /**< wqe template info */
        struct db_info db;       /**< doorbell info */
    };
};
/**
 * @brief handle to HCCL communicator
 */
using HcclComm = void *;

}
}

#endif  // MF_HYBRID_DL_HCCP_DEF_H
