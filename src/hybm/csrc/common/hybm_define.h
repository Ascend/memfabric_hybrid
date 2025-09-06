/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_HYBRID_HYBM_DEFINE_H
#define MEM_FABRIC_HYBRID_HYBM_DEFINE_H

#include <netinet/in.h>
#include <cstdint>
#include <cstddef>
#include "mf_out_logger.h"

namespace ock {
namespace mf {

constexpr uint64_t DEVICE_LARGE_PAGE_SIZE = 2UL * 1024UL * 1024UL;  // 大页的size, 2M
constexpr uint64_t HYBM_DEVICE_VA_START = 0x100000000000UL;         // NPU上的地址空间起始: 16T
constexpr uint64_t HYBM_DEVICE_VA_SIZE = 0x80000000000UL;           // NPU上的地址空间范围: 8T
constexpr uint64_t SVM_END_ADDR = HYBM_DEVICE_VA_START + HYBM_DEVICE_VA_SIZE - (1UL << 30UL); // svm的结尾虚拟地址
constexpr uint64_t HYBM_DEVICE_PRE_META_SIZE = 128UL; // 128B
constexpr uint64_t HYBM_DEVICE_GLOBAL_META_SIZE = HYBM_DEVICE_PRE_META_SIZE; // 128B
constexpr uint64_t HYBM_ENTITY_NUM_MAX = 511UL; // entity最大数量
constexpr uint64_t HYBM_DEVICE_META_SIZE = HYBM_DEVICE_PRE_META_SIZE * HYBM_ENTITY_NUM_MAX
    + HYBM_DEVICE_GLOBAL_META_SIZE; // 64K

constexpr uint64_t HYBM_DEVICE_USER_CONTEXT_PRE_SIZE = 64UL * 1024UL; // 64K
constexpr uint64_t HYBM_DEVICE_INFO_SIZE = HYBM_DEVICE_USER_CONTEXT_PRE_SIZE * HYBM_ENTITY_NUM_MAX
    + HYBM_DEVICE_META_SIZE; // 元数据+用户context,总大小32M, 对齐DEVICE_LARGE_PAGE_SIZE
constexpr uint64_t HYBM_DEVICE_META_ADDR = SVM_END_ADDR - HYBM_DEVICE_INFO_SIZE;
constexpr uint64_t HYBM_DEVICE_USER_CONTEXT_ADDR = HYBM_DEVICE_META_ADDR + HYBM_DEVICE_META_SIZE;
constexpr uint32_t ACL_MEMCPY_HOST_TO_HOST = 0;
constexpr uint32_t ACL_MEMCPY_HOST_TO_DEVICE = 1;
constexpr uint32_t ACL_MEMCPY_DEVICE_TO_HOST = 2;
constexpr uint32_t ACL_MEMCPY_DEVICE_TO_DEVICE = 3;

constexpr uint32_t HCCL_ROOT_INFO_BYTES = 256;  // 4108: root info length
constexpr uint32_t HCCP_SOCK_CONN_TAG_SIZE = 192;
constexpr uint32_t HCCP_MAX_INTERFACE_NAME_LEN = 256;

constexpr uint64_t EXPORT_INFO_MAGIC = 0xAABB1234FFFFEEEEUL;
constexpr uint64_t EXPORT_SLICE_MAGIC = 0xAABB1234FFFFBBBBUL;
constexpr uint64_t EXPORT_INFO_VERSION = 0x1UL;

inline bool IsVirtualAddressNpu(uint64_t address)
{
    return (address >= HYBM_DEVICE_VA_START && address < (HYBM_DEVICE_VA_START + HYBM_DEVICE_VA_SIZE));
}

inline bool IsVirtualAddressNpu(const void *address)
{
    return IsVirtualAddressNpu(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address)));
}

inline uint64_t Valid48BitsAddress(uint64_t address)
{
    return address & 0xffffffffffffUL;
}

inline const void *Valid48BitsAddress(const void *address)
{
    uint64_t addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
    return reinterpret_cast<const void *>(static_cast<uintptr_t>(Valid48BitsAddress(addr)));
}

inline void *Valid48BitsAddress(void *address)
{
    uint64_t addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
    return reinterpret_cast<void *>(static_cast<uintptr_t>(Valid48BitsAddress(addr)));
}

enum DeviceSystemInfoType {
    INFO_TYPE_PHY_CHIP_ID = 18,
    INFO_TYPE_PHY_DIE_ID,
    INFO_TYPE_SDID = 26,
    INFO_TYPE_SERVER_ID,
    INFO_TYPE_SCALE_TYPE,
    INFO_TYPE_SUPER_POD_ID,
    INFO_TYPE_ADDR_MODE,
};

struct HybmDeviceGlobalMeta {
    uint64_t entityCount;
    uint64_t reserved[15]; // total 128B, equal HYBM_DEVICE_PRE_META_SIZE
};

struct HybmDeviceMeta {
    uint32_t entityId;
    uint32_t rankId;
    uint32_t rankSize;
    uint32_t extraContextSize;
    uint64_t symmetricSize;
    uint64_t qpInfoAddress;
    uint64_t reserved[12]; // total 128B, equal HYBM_DEVICE_PRE_META_SIZE
};


/**
 * @brief HCCL root info
 */
struct HcclRootInfo {
    char internal[HCCL_ROOT_INFO_BYTES];
};

struct HccpRaInitConfig {
    uint32_t phyId;       /**< physical device id */
    uint32_t nicPosition; /**< reference to HccpNetworkMode */
    int hdcType;          /**< reference to drvHdcServiceType */
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

enum ibv_wc_status {
    IBV_WC_SUCCESS,
    IBV_WC_LOC_LEN_ERR,
    IBV_WC_LOC_QP_OP_ERR,
    IBV_WC_LOC_EEC_OP_ERR,
    IBV_WC_LOC_PROT_ERR,
    IBV_WC_WR_FLUSH_ERR,
    IBV_WC_MW_BIND_ERR,
    IBV_WC_BAD_RESP_ERR,
    IBV_WC_LOC_ACCESS_ERR,
    IBV_WC_REM_INV_REQ_ERR,
    IBV_WC_REM_ACCESS_ERR,
    IBV_WC_REM_OP_ERR,
    IBV_WC_RETRY_EXC_ERR,
    IBV_WC_RNR_RETRY_EXC_ERR,
    IBV_WC_LOC_RDD_VIOL_ERR,
    IBV_WC_REM_INV_RD_REQ_ERR,
    IBV_WC_REM_ABORT_ERR,
    IBV_WC_INV_EECN_ERR,
    IBV_WC_INV_EEC_STATE_ERR,
    IBV_WC_FATAL_ERR,
    IBV_WC_RESP_TIMEOUT_ERR,
    IBV_WC_GENERAL_ERR
};

enum ibv_wc_opcode {
    IBV_WC_SEND,
    IBV_WC_RDMA_WRITE,
    IBV_WC_RDMA_READ,
    IBV_WC_COMP_SWAP,
    IBV_WC_FETCH_ADD,
    IBV_WC_BIND_MW,
    /*
 * Set value of IBV_WC_RECV so consumers can test if a completion is a
 * receive by testing (opcode & IBV_WC_RECV).
 */
    IBV_WC_RECV = 1 << 7,
    IBV_WC_RECV_RDMA_WITH_IMM
};

struct ibv_wc {
    uint64_t wr_id;
    enum ibv_wc_status status;
    enum ibv_wc_opcode opcode;
    uint32_t vendor_err;
    uint32_t byte_len;
    uint32_t imm_data; /* in network byte order */
    uint32_t qp_num;
    uint32_t src_qp;
    int wc_flags;
    uint16_t pkey_index;
    uint16_t slid;
    uint8_t sl;
    uint8_t dlid_path_bits;
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
        uint32_t cq_cstm : 1;  // 0: hccp poll cq; 1: caller poll cq
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
    int mem_align;  // 0,1:4KB, 2:2MB
    uint32_t udp_sport;
    union ai_data_plane_cstm_flag data_plane_flag;  // only valid in ra_ai_qp_create
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
    unsigned long long aiQpAddr;  // refer to struct ibv_qp *
    unsigned int sqIndex;         // index of sq
    unsigned int dbIndex;         // index of db

    // below cq related info valid when data_plane_flag.bs.cq_cstm was 1
    unsigned long long ai_scq_addr;  // refer to struct ibv_cq *scq
    unsigned long long ai_rcq_addr;  // refer to struct ibv_cq *rcq
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
    DBMode dbMode{DBMode::INVALID_DB};  // 0-hw/1-sw
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
    DBMode dbMode{DBMode::INVALID_DB};  // 0-hw/1-sw
    uint64_t dbAddr{0};
};

struct RdmaMemRegionInfo {
    uint64_t size{0};  // size of the memory region
    uint64_t addr{0};  // start address of the memory region
    uint32_t lkey{0};
    uint32_t rkey{0};   // key of the memory region
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
typedef void *HcclComm;

// macro for gcc optimization for prediction of if/else
#ifndef LIKELY
#define LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

#define HYBM_API __attribute__((visibility("default")))

#define DL_LOAD_SYM(TARGET_FUNC_VAR, TARGET_FUNC_TYPE, FILE_HANDLE, SYMBOL_NAME)                      \
    do {                                                                                              \
        TARGET_FUNC_VAR = (TARGET_FUNC_TYPE)dlsym(FILE_HANDLE, SYMBOL_NAME);                          \
        if ((TARGET_FUNC_VAR) == nullptr) {                                                           \
            BM_LOG_ERROR("Failed to call dlsym to load " << (SYMBOL_NAME) << ", error" << dlerror()); \
            dlclose(FILE_HANDLE);                                                                     \
            return BM_DL_FUNCTION_FAILED;                                                             \
        }                                                                                             \
    } while (0)


enum HybmGvaVersion : uint32_t {
    HYBM_GVA_V1 = 0,
    HYBM_GVA_V2 = 1,
    HYBM_GVA_V3 = 2,
    HYBM_GVA_UNKNOWN
};

}  // namespace mf
}  // namespace ock

#endif  // MEM_FABRIC_HYBRID_HYBM_DEFINE_H
