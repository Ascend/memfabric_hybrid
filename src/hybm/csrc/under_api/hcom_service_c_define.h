/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MF_HYBRID_HCOM_SERVICE_C_DEFINE_H
#define MF_HYBRID_HCOM_SERVICE_C_DEFINE_H

#include <cstdint>
#include "hcom_c_define.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t Hcom_Channel;
/*
 * @brief Service context, which used for callback as param
 */
typedef uintptr_t Service_Context;

/*
 * @brief service, which include oob & multi protocols(TCP/RDMA/SHM) workers & callback etc
 */
typedef uintptr_t Hcom_Service;

/*
 * @brief Channel, represent multi connections(EPs) of one protocol
 *
 * two side operation, Hcom_ChannelSend
 * read operation from remote, Hcom_ChannelRead
 * write operation from remote, Hcom_ChannelWrite
 */
typedef uintptr_t Hcom_Channel;

typedef uintptr_t Service_MemoryRegion;

/*
 * @brief Service context, which used for callback as param
 */
typedef uintptr_t Service_Context;

/*
 * Callback function which will be invoked by async use mode
 */
typedef void (*Channel_CallbackFunc)(void *arg, Service_Context context);
/*
 * @brief Callback function definition
 * 1) new endpoint connected from client, only need to register this at sever side
 * 2) endpoint is broken, called when RDMA qp detection error or broken
 */
typedef int (*Service_ChannelHandler)(Hcom_Channel channel, uint64_t usrCtx, const char *payLoad);
typedef void (*Service_IdleHandler)(uint8_t wkrGrpIdx, uint16_t idxInGrp, uint64_t usrCtx);
typedef int (*Service_RequestHandler)(Service_Context ctx, uint64_t usrCtx);

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
typedef int (*Hcom_TlsGetPrivateKeyCb)(const char *name, char **priKeyPath, char **keyPass,
                                       Hcom_TlsKeyPassErase *erase);

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
 * @brief External log callback function
 *
 * @param level            [in] level, 0/1/2/3 represent debug/info/warn/error
 * @param msg              [in] message, log message with name:code-line-number
 */
typedef void (*Service_LogHandler)(int level, const char *msg);

/*
 * @brief Worker polling type
 * 1 For RDMA:
 * C_BUSY_POLLING, means cpu 100% polling no matter there is request cb, better performance but cost dedicated CPU
 * C_EVENT_POLLING, waiting on OS kernel for request cb
 * 2 For TCP/UDS
 * only event pooling is supported
 */
typedef enum {
    C_SERVICE_BUSY_POLLING = 0,
    C_SERVICE_EVENT_POLLING = 1,
} Service_WorkerMode;

typedef enum {
    C_CLIENT_WORKER_POLL = 0,
    C_CLIENT_SELF_POLL = 1,
} Service_ClientPollingMode;

typedef enum {
    C_CHANNEL_FUNC_CB = 0,   // use channel function param (const NetCallback *cb)
    C_CHANNEL_GLOBAL_CB = 1, // use service RegisterOpHandler
} Channel_CBType;

typedef enum {
    HIGH_LEVEL_BLOCK,   /* spin-wait by busy loop */
    LOW_LEVEL_BLOCK,    /* full sleep */
} Channel_FlowCtrlLevel;

typedef enum {
    C_SERVICE_RDMA = 0,
    C_SERVICE_TCP = 1,
    C_SERVICE_UDS = 2,
    C_SERVICE_SHM = 3,
} Service_Type;

typedef enum {
    C_CHANNEL_BROKEN_ALL = 0, /* when one ep broken, all eps broken */
    C_CHANNEL_RECONNECT = 1,  /* when one ep broken, try re-connect first. If re-connect fail, broken all eps */
    C_CHANNEL_KEEP_ALIVE = 2, /* when one ep broken, keep left eps alive until all eps broken */
} Service_ChannelPolicy;

/*
 * @brief Enum for callback register [new endpoint connected or endpoint broken]
 */
typedef enum {
    C_CHANNEL_NEW = 0,
    C_CHANNEL_BROKEN = 1,
} Service_ChannelHandlerType;

typedef enum {
    C_SERVICE_REQUEST_RECEIVED = 0,
    C_SERVICE_REQUEST_POSTED = 1,
    C_SERVICE_READWRITE_DONE = 2,
} Service_HandlerType;

typedef enum {
    SERVICE_ROUND_ROBIN = 0,
    SERVICE_HASH_IP_PORT = 1,
} Service_LBPolicy;

typedef enum {
    C_SERVICE_OPENSSL = 0,
    C_SERVICE_HITLS = 1,
} Service_TlsMode;

typedef enum {
    C_SERVICE_TLS_1_2 = 771,
    C_SERVICE_TLS_1_3 = 772,
} Service_TlsVersion;

typedef enum {
    C_SERVICE_AES_GCM_128 = 0,
    C_SERVICE_AES_GCM_256 = 1,
    C_SERVICE_AES_CCM_128 = 2,
    C_SERVICE_CHACHA20_POLY1305 = 3,
} Service_CipherSuite;

typedef enum {
    C_SERVICE_NET_SEC_DISABLED = 0,
    C_SERVICE_NET_SEC_ONE_WAY = 1,
    C_SERVICE_NET_SEC_TWO_WAY = 2,
} Service_SecType;

/*
 * @brief Context type, part of Service_Context, sync mode is not aware most of them
 */
typedef enum {
    SERVICE_RECEIVED = 0,     /* support invoke all functions */
    SERVICE_RECEIVED_RAW = 1, /* support invoke most functions except Service_GetOpInfo() */
    SERVICE_SENT = 2,         /* support invoke basic functions except
                                 Service_GetMessage() * 3、Service_GetRspCtx()、 */
    SERVICE_SENT_RAW = 3,     /* support invoke basic functions except
                                 Service_GetMessage() * 3、、Service_GetRspCtx()、Service_GetOpInfo() */
    SERVICE_ONE_SIDE = 4,     /* support invoke basic functions except
                                 Service_GetMessage() * 3、、Service_GetRspCtx()、Service_GetOpInfo() */

    SERVICE_INVALID_OP_TYPE = 255,
} Service_ContextType;

typedef struct {
    uint32_t maxSendRecvDataSize;
    uint16_t workerGroupId;
    uint16_t workerGroupThreadCount;
    Service_WorkerMode workerGroupMode;
    int8_t workerThreadPriority;
    char workerGroupCpuRange[64];   // worker group cpu range, for example 6-10
} Service_Options;

typedef struct {
    Channel_CallbackFunc cb; // User callback function
    void *arg;               // Argument of callback
} Channel_Callback;

typedef struct {
    uint16_t clientGroupId;     // worker group id of client
    uint16_t serverGroupId;     // worker group id of server
    uint8_t linkCount;     // actual link count of the channel
    Service_ClientPollingMode mode;
    Channel_CBType cbType;
    char payLoad[512];
} Service_ConnectOptions;

typedef struct {
    void *address;    /* pointer of data */
    uint32_t size;              /* size of data */
    uint16_t opcode;
} Channel_Request;

typedef struct {
    void *address;              /* pointer of data */
    uint32_t size;              /* size of data */
    int16_t errorCode;          /* error code of response */
} Channel_Response;

typedef struct {
    void *rspCtx;
    int16_t errorCode;
} Channel_ReplyContext;

typedef struct {
    uint32_t keys[4];
} OneSideKey;

/*
 * @brief Read/write mr info for one side rdma operation
 */
typedef struct {
    uintptr_t lAddress; // local memory region address
    OneSideKey lKey;      // local memory region key
    uint32_t size;      // data size
} Service_MemoryRegionInfo;

typedef struct {
    void *lAddress;
    void *rAddress;
    OneSideKey lKey;
    OneSideKey rKey;
    uint32_t size;
} Channel_OneSideRequest;

typedef struct {
    uint64_t intervalTimeMs;
    uint64_t thresholdByte;
    Channel_FlowCtrlLevel flowCtrlLevel;
} Channel_FlowCtrlOptions;

#ifdef __cplusplus
}
#endif

#endif // MF_HYBRID_HCOM_SERVICE_C_DEFINE_H
