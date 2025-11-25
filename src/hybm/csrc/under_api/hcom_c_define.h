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
#ifndef MF_HYBRID_HCOM_DEFINE_H
#define MF_HYBRID_HCOM_DEFINE_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define NET_SGE_MAX_IOV 4
#define MAX_IP_LENGTH 16
#define NET_C_FLAGS_BIT(i) (1UL << (i))

/*
 * @brief Driver, which include oob & rdma communication & callback etc
 */
typedef uintptr_t Hcom_Driver;

/*
 * @brief Endpoint, represent one RDMA connection to dual-direction communication
 *
 * two side operation, Hcom_EPPostSend
 * read operation from remote, Hcom_EPPostRead
 * write operation from remote, Hcom_EPPostWrite
 */
typedef uintptr_t Hcom_EndPoint;

/*
 * @brief RegMemoryRegion, which region memory in RDMA Nic for write/read operation
 */
typedef uintptr_t Hcom_MemoryRegion;

typedef enum {
    NET_C_EP_SELF_POLLING = NET_C_FLAGS_BIT(0),
    NET_C_EP_EVENT_POLLING = NET_C_FLAGS_BIT(1)
} Hcom_PollingMode;

/*
 * @brief Request type, part of Hcom_RequestContext
 */
typedef enum {
    C_SENT = 0,
    C_SENT_RAW = 1,
    C_SENT_RAW_SGL = 2,
    C_RECEIVED = 3,
    C_RECEIVED_RAW = 4,
    C_WRITTEN = 5,
    C_READ = 6,
    C_SGL_WRITTEN = 7,
    C_SGL_READ = 8,
} Hcom_RequestType;

/*
 * @brief Worker polling type
 * 1 For RDMA:
 * C_BUSY_POLLING, means cpu 100% polling no matter there is request done, better performance but cost dedicated CPU
 * C_EVENT_POLLING, waiting on OS kernel for request done
 * 2 For TCP/UDS
 * only event pooling is supported
 */
typedef enum {
    C_BUSY_POLLING = 0,
    C_EVENT_POLLING = 1,
} Hcom_DriverWorkingMode;

typedef enum {
    C_DRIVER_RDMA = 0,
    C_DRIVER_TCP = 1,
    C_DRIVER_UDS = 2,
    C_DRIVER_SHM = 3,
} Hcom_DriverType;

/*
 * @brief DriverOobType working mode
 */
typedef enum {
    C_NET_OOB_TCP = 0,
    C_NET_OOB_UDS = 1,
} Hcom_DriverOobType;

/*
 * @brief Enum for secure type
 */
typedef enum {
    C_NET_SEC_DISABLED = 0,
    C_NET_SEC_ONE_WAY = 1,
    C_NET_SEC_TWO_WAY = 2,
} Hcom_DriverSecType;

typedef enum {
    C_TLS_1_2 = 771,
    C_TLS_1_3 = 772,
} Hcom_DriverTlsVersion;

typedef enum {
    C_OPENSSL = 0,
    C_HITLS = 1,
} Hcom_DriverTlsMode;
/*
 * @brief DriverCipherSuite mode
 */
typedef enum {
    C_AES_GCM_128 = 0,
    C_AES_GCM_256 = 1,
    C_AES_CCM_128 = 2,
    C_CHACHA20_POLY1305 = 3,
} Hcom_DriverCipherSuite;

/*
 * @brief Memory allocator cache tier policy
 */
typedef enum {
    C_TIER_TIMES = 0, /* tier by times of min-block-size */
    C_TIER_POWER = 1, /* tier by power of min-block-size */
} Hcom_MemoryAllocatorCacheTierPolicy;

/*
 * @brief Enum for tls callback, set peer cert verify type
 */
typedef enum {
    C_VERIFY_BY_NONE = 0,
    C_VERIFY_BY_DEFAULT = 1,
    C_VERIFY_BY_CUSTOM_FUNC = 2,
} Hcom_PeerCertVerifyType;

/*
 * @brief Type of allocator
 */
typedef enum {
    C_DYNAMIC_SIZE = 0,            /* allocate dynamic memory size, there is alignment with X KB */
    C_DYNAMIC_SIZE_WITH_CACHE = 1, /* allocator with dynamic memory size, with pre-allocate cache for performance */
} Hcom_MemoryAllocatorType;

/*
 * @brief Enum for callback register [new endpoint connected or endpoint broken]
 */
typedef enum {
    C_EP_NEW = 0,
    C_EP_BROKEN = 1,
} Hcom_EPHandlerType;

/*
 * @brief Enum for callback register [request received, request posted, read/write done]
 */
typedef enum {
    C_OP_REQUEST_RECEIVED = 0,
    C_OP_REQUEST_POSTED = 1,
    C_OP_READWRITE_DONE = 2,
} Hcom_OpHandlerType;

/*
 * @brief Two side RDMA operation (i.e. RDMA send/receive)
 *
 * @param data, pointer of data need to send to peer (the data will be copied to register RDMA memory region
 * the data must be less than mrSendReceiveSegSize of driver
 * @param size, size of data
 */
typedef struct {
    uintptr_t data;         // pointer of data to send to peer
    uint32_t size;          // size of data
    uint16_t upCtxSize;     // user context size
    char upCtxData[16];     // user context
} Hcom_SendRequest;

typedef struct {
    uint32_t seqNo;    // seq no
    int16_t timeout;  // timeout
    int16_t errorCode; // error code
    uint8_t flags;     // flags
} Hcom_OpInfo;

/*
 * @brief Device information for user
 */
typedef struct {
    int maxSge; // max iov count in NetTransSglRequest
} Hcom_DeviceInfo;

/*
 * @brief Read/write request for one side rdma operation
 */
typedef struct {
    uintptr_t lMRA;         // local memory region address
    uintptr_t rMRA;         // remote memory region address
    uint64_t lKey;          // local memory region key
    uint64_t rKey;          // remote memory region key
    uint32_t size;          // data size
    uint16_t upCtxSize;     // user context size
    char upCtxData[16]; // user context
} Hcom_ReadWriteRequest;

typedef struct {
    uintptr_t lAddress; // local memory region address
    uintptr_t rAddress; // remote memory region address
    uint64_t lKey;      // local memory region key
    uint64_t rKey;      // remote memory region key
    uint32_t size;      // data size
} __attribute__((packed)) Hcom_ReadWriteSge;

typedef struct {
    Hcom_ReadWriteSge *iov;  // sgl array
    uint16_t iovCount;      // max count:NUM_4
    uint16_t upCtxSize;     // user context size
    char upCtxData[16]; // user context
} __attribute__((packed)) Hcom_ReadWriteSglRequest;

/*
 * @brief Read/write mr info for one side rdma operation
 */
typedef struct {
    uintptr_t lAddress; // local memory region address
    uint64_t lKey;      // local memory region key
    uint32_t size;      // data size
} Hcom_MemoryRegionInfo;

/*
 * @brief Callback function context, for received, post done, read/write done
 */
typedef struct {
    Hcom_RequestType type;
    uint16_t opCode;   // for post send
    uint16_t flags;    // flags on the header
    int16_t timeout;  // timeout
    int16_t errorCode; // error code
    int result;        // return 0 successful
    void *msgData;     // for receive operation or C_OP_REQUEST_RECEIVED callback
    uint32_t msgSize;  // for receive operation or C_OP_REQUEST_RECEIVED callback
    uint32_t seqNo;    // for post send raw
    Hcom_EndPoint ep;
    Hcom_SendRequest originalSend;           // for C_OP_REQUEST_POSTED, copy struct information, not original
    // originalSend.data is self rdma address, not original input data address
    Hcom_ReadWriteRequest originalReq;       // for C_OP_READWRITE_DONE, copy struct information, not original
    Hcom_ReadWriteSglRequest originalSglReq; // for C_OP_READWRITE_DONE, copy struct information, not original
} Hcom_RequestContext;

typedef struct {
    uint16_t opCode;
    uint32_t seqNo;
    void *msgData;
    uint32_t msgSize;
} Hcom_ResponseContext;

typedef struct {
    uint32_t pid;
    uint32_t uid;
    uint32_t gid;
} Hcom_UdsIdInfo;

/*
 * @brief Options for driver initialization
 */
typedef struct {
    Hcom_DriverWorkingMode mode;        // polling mode
    uint32_t mrSendReceiveSegCount;    // segment count of segment for send/receive
    uint32_t mrSendReceiveSegSize;     // single segment size of send/receive memory region
    char netDeviceIpMask[256];     // device ip mask, for multiple net device cases
    char netDeviceIpGroup[1024];   // ip group for devices
    uint16_t completionQueueDepth;     // rdma completion queue size
    uint16_t maxPostSendCountPerQP;    // max post send count
    uint16_t prePostReceiveSizePerQP;  // pre post receive size for one qp
    uint16_t pollingBatchSize;         // polling wc size on at one time
    uint32_t qpSendQueueSize;          // qp send queue size, by default is 256
    uint32_t qpReceiveQueueSize;       // qp receive queue size, by default is 256
    uint16_t dontStartWorkers;         // start worker or not, 1 means don't start, 0 means start
    char workerGroups[64];         // worker groups, for example 1,3,3
    char workerGroupsCpuSet[128];  // worker groups cpu set, for example 1-16
    // worker thread priority [-20,20], 20 is the lowest, -20 is the highest, 0 (default) means do not set priority
    int workerThreadPriority;
    uint16_t heartBeatIdleTime;        // heart beat idle time, in seconds
    uint16_t heartBeatProbeTimes;      // heart beat probe times, in seconds
    uint16_t heartBeatProbeInterval;   // heart beat probe interval, in seconds
    // timeout during io (s), it should be [-1, 1024], -1 means do not set, 0 means never timeout during io
    int16_t tcpUserTimeout;
    bool tcpEnableNoDelay;             // tcp TCP_NODELAY option, true in default
    bool tcpSendZCopy;                 // tcp whether copy request to inner memory, false in default
    /* The buffer sizes will be adjusted automatically when these two variables are 0, and the performance would be
     * better */
    uint16_t tcpSendBufSize;           // tcp connection send buffer size in kernel, in KB
    uint16_t tcpReceiveBufSize;        // tcp connection send receive buf size in kernel, in KB
    uint16_t enableTls;                // value only in 0 and 1, value 1 means enable ssl and encrypt, 0 on the contrary
    Hcom_DriverTlsMode tlsMode;         // tls mode, default is openssl
    Hcom_DriverSecType secType;         // security type
    Hcom_DriverTlsVersion tlsVersion;   // tls version, default TLS1.3 (772)
    Hcom_DriverCipherSuite cipherSuite; // if tls enabled can set cipher suite, client and server should same
    Hcom_DriverOobType oobType;         // oob type, tcp or UDS, UDS cannot accept remote connection
    uint8_t version;                   // program version used by connect validation
    uint32_t maxConnectionNum;         // max connection number
    char oobPortRange[16];         // port range when enable port auto selection
} Hcom_DriverOptions;

/*
 * @brief Options for multiple listeners
 */
typedef struct {
    char ip[16];            // ip to be listened
    uint16_t port;              // port to be listened
    uint16_t targetWorkerCount; // the count of workers can be dispatched to, for connections from this listener
} Hcom_DriverOobListenOptions;

/*
 * @brief Oob uds listening information
 */
typedef struct {
    char name[96];              // UDS name for listen or file path
    uint16_t perm;              // if 0 means not use file, otherwise use file and this perm as file perm
    uint16_t targetWorkerCount; // the count of target workers, if >= 1,
    // the accepted socket will be attached to sub set to workers, 0 means all
} Hcom_OobUDSListenerOptions;

/*
 * @brief Callback function definition
 * 1) new endpoint connected from client, only need to register this at sever side
 * 2) endpoint is broken, called when RDMA qp detection error or broken
 */
typedef int (*Hcom_EPHandler)(Hcom_EndPoint ep, uint64_t usrCtx, const char *payLoad);

/*
 * @brief Callback function definition
 *
 * it is called when the following cases happen
 * 1) post send done
 * 2) read done
 * 3) write done
 *
 * Important notes:
 * 1) ctx is a thread local static variable, cannot transform to another thread directly
 * 2) msgData need to copy to another space properly
 * 3) ep can be transferred to another thread for further reply or other stuff
 * in this case, need to call Hcom_EPRefer() to increase reference count
 * and call Hcom_EPDestroy() after to decrease the reference count
 */
typedef int (*Hcom_RequestHandler)(Hcom_RequestContext *ctx, uint64_t usrCtx);

/*
 * @brief Idle callback function, when worker thread idle, this function will be called
 *
 * @param wkrGrpIdx        [in] worker group index in on net driver
 * @param idxInGrp         [in] worker index in the group
 * @param usrCtx           [in] user context
 */
typedef void (*Hcom_IdleHandler)(uint8_t wkrGrpIdx, uint16_t idxInGrp, uint64_t usrCtx);

/*
 * @brief Sec callback function, when oob connect build, this function will be called to generate auth info.
 * if this function not set secure type is C_NET_SEC_NO_VALID and oob will not send secure info
 *
 * @param ctx              [in] ctx from connect param ctx, and will send in auth process
 * @param flag             [out] flag to sent in auth process
 * @param type             [out] secure type, value should set in oob client, and should in [C_NET_SEC_ONE_WAY,
 * C_NET_SEC_TWO_WAY]
 * @param output           [out] secure info created
 * @param outLen           [out] secure info length
 * @param needAutoFree     [out] secure info need to auto free in hcom or not
 */
typedef int (*Hcom_SecInfoProvider)(uint64_t ctx, int64_t *flag, Hcom_DriverSecType *type, char **output,
                                    uint32_t *outLen, int *needAutoFree);

/*
 * @brief ValidateSecInfo callback function, when oob connect build, this function will be called to validate auth info
 * if this function not set oob will not validate secure info
 *
 * @param flag             [in] flag received in auth process
 * @param ctx              [in] ctx received in auth process
 * @param input            [in] secure info received
 * @param inputLen         [in] secure info length
 */
typedef int (*Hcom_SecInfoValidator)(uint64_t ctx, int64_t flag, const char *input, uint32_t inputLen);

/*
 * @brief keyPass          [in] erase function
 * @param keyPass          [in] the memory address of keyPass
 */
typedef void (*Hcom_TlsKeyPassErase)(char *keyPass, int len);

/*
 * @brief The cert verify function
 *
 * @param x509             [in] the x509 object of CA
 * @param crlPath          [in] the crl file path
 *
 * @return -1 for failed, and 1 for success
 */
typedef int (*Hcom_TlsCertVerify)(void *x509, const char *crlPath);

/*
 * @brief Get the certificate file of public key
 *
 * @param name             [out] the name
 * @param certPath         [out] the path of certificate
 */
typedef int (*Hcom_TlsGetCertCb)(const char *name, char **certPath);

/*
 * @brief Get private key file's path and length, and get the keyPass
 * @param name             [out] the name
 * @param priKeyPath       [out] the path of private key
 * @param keyPass          [out] the keyPass
 * @param erase            [out] the erase function
 */
typedef int (*Hcom_TlsGetPrivateKeyCb)
        (const char *name, char **priKeyPath, char **keyPass, Hcom_TlsKeyPassErase *erase);

/*
 * @brief Get the CA and verify
 * @param name             [out] the name
 * @param caPath           [out] the path of CA file
 * @param crlPath          [out] the crl file path
 * @param verifyType       [out] the type of verify in[VERIFY_BY_NONE,VERIFY_BY_DEFAULT, VERIFY_BY_CUSTOM_FUNC]
 * @param verify           [out] the verify function, only effect in VERIFY_BY_CUSTOM_FUNC mode
 */
typedef int (*Hcom_TlsGetCACb)(const char *name, char **caPath, char **crlPath, Hcom_PeerCertVerifyType *verifyType,
                               Hcom_TlsCertVerify *verify);

/*
 * @brief External log callback function
 *
 * @param level            [in] level, 0/1/2/3 represent debug/info/warn/error
 * @param msg              [in] message, log message with name:code-line-number
 */
typedef void (*Hcom_LogHandler)(int level, const char *msg);

/*
 * @brief Options for Memory Allocator
 */
typedef struct {
    uintptr_t address;                                  /* base address of large range of memory for allocator */
    uint64_t size;                                      /* size of large memory chuck */
    uint32_t minBlockSize;                              /* min size of block, more than 4 KB is required */
    uint32_t bucketCount;                               /* default size of hash bucket */
    uint16_t alignedAddress;                            /* force to align the memory block allocated, 0 means not align
                                                          1 means align */
    uint16_t cacheTierCount;                            /* for DYNAMIC_SIZE_WITH_CACHE only */
    uint16_t cacheBlockCountPerTier;                    /* for DYNAMIC_SIZE_WITH_CACHE only */
    Hcom_MemoryAllocatorCacheTierPolicy cacheTierPolicy; /* tier policy */
} Hcom_MemoryAllocatorOptions;

/*
 * @brief memory allocator ptr
 */
typedef uintptr_t Hcom_MemoryAllocator;

#ifdef __cplusplus
}
#endif
#endif // MF_HYBRID_HCOM_DEFINE_H
