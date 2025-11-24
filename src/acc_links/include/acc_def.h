/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ACC_LINKS_ACC_DEF_H
#define ACC_LINKS_ACC_DEF_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <sstream>
#include <thread>

#include "acc_ref.h"
#include "acc_log.h"

namespace ock {
namespace acc {
constexpr uint32_t MAX_RECV_BODY_LEN = 10 * 1024 * 1024; /* max receive body len limit */
constexpr uint32_t UNO_1024 = 1024;
constexpr uint32_t UNO_500 = 500;
constexpr uint32_t UNO_256 = 256;
constexpr uint32_t UNO_48 = 48;
constexpr uint32_t UNO_32 = 32;
constexpr uint32_t UNO_16 = 16;
constexpr uint32_t UNO_7 = 7;
constexpr uint32_t UNO_2 = 2;
constexpr uint32_t UNO_1 = 1;

/**
 * @brief Header of connecting to server
 */
struct AccConnReq {
    int16_t magic = 0;
    int16_t version = 0;
    uint64_t rankId = 0;
};

/**
 * @brief Response of connecting
 */
struct AccConnResp {
    int16_t result = 0;
};

/**
 * @brief Result of message sending
 */
enum AccMsgSentResult {
    MSG_SENT = 0,
    MSG_TIMEOUT = 1,
    MSG_LINK_BROKEN = 2,
    /* add error code ahead of this */
    MSG_BUTT,
};

/**
 * @brief Header of message
 */
struct AccMsgHeader {
    int16_t type = 0;     /* data type or opCode */
    int16_t result = 0;   /* result for response */
    uint32_t bodyLen = 0; /* length of data */
    uint32_t seqNo = 0;   /* seqNo */
    uint32_t crc = 0;     /* reserved crc */

    AccMsgHeader() = default;

    AccMsgHeader(int16_t t, uint32_t bLen, uint32_t sno) : type(t), bodyLen(bLen), seqNo(sno)
    {
    }

    AccMsgHeader(int16_t t, int16_t r, uint32_t bLen, uint32_t sno) : type(t), result(r), bodyLen(bLen), seqNo(sno)
    {
    }

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "type: " << type << ", result: " << result << ", bodyLen: " << bodyLen << ", seqNo: " << seqNo
            << ", crc: " << crc;
        return oss.str();
    }
};

/**
 * @brief Options of Tcp Server, required when start a tcp server
 */
struct AccTcpServerOptions {
    std::string listenIp;                    /* listen ip */
    uint16_t listenPort = 9966L;             /* listen port */
    uint16_t workerCount = UNO_2;            /* number of worker threads */
    int16_t workerThreadPriority = 0;        /* priority of worker threads */
    int16_t workerPollTimeoutMs = UNO_500;   /* epoll timeout */
    int16_t workerStartCpuId = -1;           /* start cpu id of workers */
    uint16_t linkSendQueueSize = UNO_1024;   /* send queue size */
    uint16_t keepaliveIdleTime = UNO_32;     /* tcp keepalive idle time */
    uint16_t keepaliveProbeTimes = UNO_7;    /* tcp keepalive probe times */
    uint16_t keepaliveProbeInterval = UNO_2; /* tcp keepalive probe interval */
    bool reusePort = true;                   /* reuse listen port */
    bool enableListener = false;             /* start listener or not */
    int16_t magic = 0;                       /* magic number of  */
    int16_t version = 0;                     /* version */
    uint32_t maxWorldSize = UNO_1024;        /* max client number */
};

/**
 * @brief Callback function of private key password decryptor, see @RegisterDecryptHandler
 *
 * @param cipherText       [in] the encrypted text(private key password)
 * @param cipherTextLen    [in] the length of cipherText
 * @param plainText        [out] the decrypted text(private key password)
 * @param plaintextLen     [out] the length of plainText
 */
using AccDecryptHandler = std::function<int(const char *cipherText, size_t cipherTextLen, char *plainText,
                                            size_t plainTextLen)>;

/**
 * @brief Tls related option, required if TLS enabled
 */
struct AccTlsOption {
    bool enableTls = false;
    std::string tlsTopPath;           /* root path of certifications */
    std::string tlsCert;              /* certification of server */
    std::string tlsCrlPath;           /* optional, crl file path */
    std::string tlsCaPath;            /* ca file path */
    std::set<std::string> tlsCaFile;  /* paths of ca */
    std::set<std::string> tlsCrlFile; /* path of crl file */
    std::string tlsPk;                /* private key */
    std::string tlsPkPwd;             /* private key password, required, encrypt or plain both allowed */

    AccTlsOption() : enableTls(false)
    {
    }
};

/**
 * @brief Result codes
 */
enum AccResult {
    ACC_OK = 0,
    ACC_ERROR = -1,
    ACC_NEW_OBJECT_FAIL = -2,
    ACC_MALLOC_FAIL = -3,
    ACC_INVALID_PARAM = -4,
    ACC_NOT_INITIALIZED = -5,
    ACC_TIMEOUT = -6,
    ACC_CONNECTION_NOT_READY = -7,
    ACC_EPOLL_ERROR = -8,
    ACC_LINK_OPTION_ERROR = -9,
    ACC_QUEUE_IS_FULL = -10,
    ACC_LINK_ERROR = -11,
    ACC_LINK_EAGAIN = -12,
    ACC_LINK_MSG_READY = -13,
    ACC_LINK_MSG_SENT = -14,
    ACC_LINK_MSG_INVALID = -15,
    ACC_LINK_NEED_RECONN = -16,
    ACC_LINK_ADDRESS_IN_USE = -17,
    ACC_RESULT_BUTT = -18,
};

class AccDataBuffer;
class AccTcpServer;
class AccTcpLink;
class AccTcpRequestContext;
class AccTcpLinkComplex;
using AccDataBufferPtr = AccRef<AccDataBuffer>;
using AccTcpServerPtr = AccRef<AccTcpServer>;
using AccTcpLinkPtr = AccRef<AccTcpLink>;
using AccTcpLinkComplexPtr = AccRef<AccTcpLinkComplex>;

#define ACC_API __attribute__((visibility("default")))
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_DEF_H
