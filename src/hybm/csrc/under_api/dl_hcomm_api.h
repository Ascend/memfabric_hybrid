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

#ifndef MF_HYBM_CORE_DL_HCOMM_API_H
#define MF_HYBM_CORE_DL_HCOMM_API_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include "hybm_common_include.h"

namespace ock {
namespace mf {

static constexpr uint32_t COMM_ADDR_EID_LEN = 16U;
static constexpr uint32_t HCOMM_CHANNEL_MAGIC_WORD = 0x0fcf0f0fU;
static constexpr uint32_t HCOMM_CHANNEL_VERSION_ONE = 1U;
static constexpr uint32_t HCOMM_CHANNEL_VERSION = HCOMM_CHANNEL_VERSION_ONE;
static constexpr uint32_t COMM_LINK_MAGIC_WORD = 0x0f0e0f0fU;
static constexpr uint32_t COMM_LINK_VERSION = 1U;

using HcommResult = int32_t;

/// 网络设备句柄
using EndpointHandle = void *;

/**
 * @brief 内存句柄类型（不透明结构）
 */
using HcommMemHandle = void *;

using HcommSocket = void *;

#ifndef CHANNEL_HANDLE_DEFINED
#define CHANNEL_HANDLE_DEFINED
/**
 * @brief 通道句柄类型
 */
using ChannelHandle = uint64_t;
#endif

#ifndef THREAD_HANDLE_DEFINED
#define THREAD_HANDLE_DEFINED
/**
 * @brief 线程句柄类型
 */
using ThreadHandle = uint64_t;
#endif

/**
 * @brief 通信引擎类型枚举
 */
enum CommEngine : int32_t {
    COMM_ENGINE_RESERVED = -1, ///< 保留的通信引擎
    COMM_ENGINE_CPU = 0,       ///< HOST CPU引擎
    COMM_ENGINE_CPU_TS = 1,    ///< HOST CPU TS引擎
    COMM_ENGINE_AICPU = 2,     ///< AICPU引擎
    COMM_ENGINE_AICPU_TS = 3,  ///< AICPU TS引擎
    COMM_ENGINE_AIV = 4,       ///< AIV引擎
    COMM_ENGINE_CCU = 5,       ///< CCU引擎
};

/**
 * @brief 通信协议类型枚举
 */
enum CommProtocol : int32_t {
    COMM_PROTOCOL_RESERVED = -1, ///< 保留协议类型
    COMM_PROTOCOL_HCCS = 0,      ///< HCCS协议
    COMM_PROTOCOL_ROCE = 1,      ///< RDMA over Converged Ethernet
    COMM_PROTOCOL_PCIE = 2,      ///< PCIe协议
    COMM_PROTOCOL_SIO = 3,       ///< SIO协议
    COMM_PROTOCOL_UBC_CTP = 4,   ///< 华为统一总线UBC_CTP
    COMM_PROTOCOL_UBC_TP = 5,    ///< 华为统一总线UBC_TP
    COMM_PROTOCOL_UB_MEM = 6,    ///< UB_MEM协议
    COMM_PROTOCOL_UBOE = 7,      ///< UBoE协议
};

/**
 * @brief 通信拓扑枚举
 */
enum CommTopo : int32_t {
    COMM_TOPO_RESERVED = -1,  ///< 保留拓扑
    COMM_TOPO_CLOS = 0,       ///< CLOS互联拓扑
    COMM_TOPO_1DMESH = 1,     ///< 1DMesh互联拓扑
    COMM_TOPO_910_93 = 2,     ///< 910_93互联拓扑(带SIO)
    COMM_TOPO_310P = 3,       ///< 310P互联拓扑
    COMM_TOPO_A2AXSERVER = 4, ///< A2_AX_SERVER
    COMM_TOPO_CUSTOM = 5      ///< 自定义
};

/**
 * @brief 异构组网模式枚举
 */
enum HcclHeterogMode : int32_t {
    HCCL_HETEROG_MODE_INVALID = -1,
    HCCL_HETEROG_MODE_HOMOGENEOUS = 0,
    HCCL_HETEROG_MODE_MIX_A2_A3,
};

/**
 * @brief 通信设备Endpoint属性
 */
enum EndpointAttr : int32_t {
    ENDPOINT_ATTR_INVALID = -1,
    ENDPOINT_ATTR_BW_COEFF = 0,
    ENDPOINT_ATTR_DIE_ID = 1,
    ENDPOINT_ATTR_LOCATION = 2,
};

using EndpointAttrBwCoeff = uint32_t;
using EndpointAttrDieId = uint32_t;
using EndpointAttrLocation = uint32_t;

/**
 * @brief 通信设备地址类别
 */
enum CommAddrType : int32_t {
    COMM_ADDR_TYPE_RESERVED = -1, ///< 保留地址类型
    COMM_ADDR_TYPE_IP_V4 = 0,     ///< IPv4地址类型
    COMM_ADDR_TYPE_IP_V6 = 1,     ///< IPv6地址类型
    COMM_ADDR_TYPE_ID = 2,        ///< ID地址类型
    COMM_ADDR_TYPE_EID = 3,       ///< EID地址类型
};

/**
 * @brief 通信设备地址描述结构体
 * @note 支持CommAddrType的扩展，地址最大长度36字节
 */
struct CommAddr {
    CommAddrType type;                   ///< 通信地址类别
    union {
        uint8_t raws[36];                ///< 通用数据
        struct in_addr addr;             ///< IPv4地址结构
        struct in6_addr addr6;           ///< IPv6地址结构
        uint32_t id;                     ///< 标识
        uint8_t eid[COMM_ADDR_EID_LEN];  ///< EID地址类型
    };
};

/**
 * @brief 通信设备Endpoint位置类型枚举
 */
enum EndpointLocType : int32_t {
    ENDPOINT_LOC_TYPE_RESERVED = -1, ///< 保留的Endpoint位置
    ENDPOINT_LOC_TYPE_DEVICE = 0,    ///< Endpoint在Device上
    ENDPOINT_LOC_TYPE_HOST = 1,      ///< Endpoint在Host上
};

/**
 * @brief Endpoint位置类型结构体
 * @note 支持EndpointLocType的扩展，最大60字节内容
 */
struct EndpointLoc {
    EndpointLocType locType;           ///< Endpoint的位置类别
    union {
        uint8_t raws[60];              ///< 通用数据
        struct {
            uint32_t devPhyId;         ///< 设备物理Id
            uint32_t superDevId;       ///< 超节点deviceId
            uint32_t serverIdx;        ///< Server的索引
            uint32_t superPodIdx;      ///< 超节点位置索引
        } device;                      ///< 当locType为DEVICE时使用
        struct {
            uint32_t id;               ///< 普通Id，当locType为HOST等时可能使用
        } host;
    };
};

/**
 * @brief 通信设备端点描述结构体
 */
struct EndpointDesc {
    CommProtocol protocol;             ///< 通信协议
    CommAddr commAddr;                 ///< 通信地址
    EndpointLoc loc;                   ///< Endpoint的位置信息
    union {
        uint8_t raws[52];              ///< 通用数据
    };
};

/**
 * @brief 内存类型枚举定义
 */
enum CommMemType : int32_t {
    COMM_MEM_TYPE_INVALID = -1, ///< 无效的内存类别
    COMM_MEM_TYPE_DEVICE = 0,   ///< 设备侧内存（如NPU等）
    COMM_MEM_TYPE_HOST = 1,     ///< 主机侧内存
};

inline std::ostream &operator<<(std::ostream &os, CommMemType obj)
{
    switch (obj) {
        case COMM_MEM_TYPE_INVALID:
            return os << "INVALID";
        case COMM_MEM_TYPE_DEVICE:
            return os << "DEVICE";
        case COMM_MEM_TYPE_HOST:
            return os << "HOST";
        default:
            return os << "UNKNOWN(" << static_cast<int32_t>(obj) << ")";
    }
}

/**
 * @brief 内存段元数据描述结构体
 */
struct HcommCommMem {
    CommMemType type{COMM_MEM_TYPE_INVALID}; ///< 内存物理位置类型，参见CommMemType
    void *addr{nullptr};                     ///< 内存地址
    uint64_t size{0};                        ///< 内存区域字节数
};

/**
 * @brief 兼容Abi字段结构体
 */
struct HcommAbiHeader {
    uint32_t version{0};     ///< ABI版本号
    uint32_t magicWord{0};   ///< 魔数
    uint32_t size{0};        ///< 结构体大小
    uint32_t reserved{0};    ///< 保留字段
};

using CommAbiHeader = HcommAbiHeader;

/**
 * @brief 通信Link信息
 */
struct CommLink {
    CommAbiHeader header;
    EndpointDesc srcEndpointDesc;
    EndpointDesc dstEndpointDesc;
    union {
        uint8_t raws[128];
        struct {
            CommProtocol linkProtocol;
            uint8_t hop;
        };
    } linkAttr;
};

/**
 * @brief 套接字角色
 */
enum HcommSocketRole : int32_t {
    HCOMM_SOCKET_ROLE_RESERVED = -1, ///< 保留的套接字角色
    HCOMM_SOCKET_ROLE_CLIENT = 0,    ///< 客户端角色，用于发起连接
    HCOMM_SOCKET_ROLE_SERVER = 1,    ///< 服务器角色，用于监听连接
};

/**
 * @brief 通道描述参数
 * @note 结构体末尾扩展需要自增版本号，并补充兼容处理逻辑。
 */
struct HcommChannelDesc {
    HcommAbiHeader header;             ///< ABI头部，包含版本等信息
    EndpointDesc remoteEndpoint;       ///< 远端网络设备端侧描述
    uint32_t notifyNum;                ///< channel上使用的通知消息数量
    bool exchangeAllMems;              ///< true表示无需显式传入memHandles
    HcommMemHandle *memHandles;        ///< 注册到通信域的待交换内存句柄
    uint32_t memHandleNum;             ///< 待交换内存句柄数量
    HcommSocket socket;                ///< 预创建socket句柄
    HcommSocketRole role;              ///< 本端角色(SERVER或CLIENT)
    uint16_t port;                     ///< 监听端口或目标端口
    union {
        uint8_t raws[128];             ///< 通用缓存
        struct {
            uint32_t queueNum;         ///< QP数量
            uint32_t retryCnt;         ///< 最大重传次数
            uint32_t retryInterval;    ///< 重传间隔（ms）
            uint8_t tc;                ///< 流量类别（QoS)
            uint8_t sl;                ///< 服务等级（QoS)
        } roceAttr;
        struct {
            uint32_t qos;              ///< HCCS QoS
        } hccsAttr;
    };
};

enum HcommTransferType : int32_t {
    HCOMM_TRANSFER_TYPE_INVALID = -1,
    HCOMM_TRANSFER_TYPE_WRITE = 0,
    HCOMM_TRANSFER_TYPE_WRITE_REDUCE = 1,
    HCOMM_TRANSFER_TYPE_WRITE_WITH_NOTIFY = 2,
    HCOMM_TRANSFER_TYPE_WRITE_REDUCE_WITH_NOTIFY = 3,
    HCOMM_TRANSFER_TYPE_READ = 4,
    HCOMM_TRANSFER_TYPE_READ_REDUCE = 5,
    HCOMM_TRANSFER_TYPE_NOTIFY_RECORD = 6,
    HCOMM_TRANSFER_TYPE_NOTIFY_WAIT = 7,
};

enum HcommReduceOp : int32_t {
    HCOMM_REDUCE_SUM = 0,
    HCOMM_REDUCE_PROD = 1,
    HCOMM_REDUCE_MAX = 2,
    HCOMM_REDUCE_MIN = 3,
    HCOMM_REDUCE_RESERVED = 255,
};

enum HcommDataType : int32_t {
    HCOMM_DATA_TYPE_INT8 = 0,
    HCOMM_DATA_TYPE_INT16 = 1,
    HCOMM_DATA_TYPE_INT32 = 2,
    HCOMM_DATA_TYPE_FP16 = 3,
    HCOMM_DATA_TYPE_FP32 = 4,
    HCOMM_DATA_TYPE_INT64 = 5,
    HCOMM_DATA_TYPE_UINT64 = 6,
    HCOMM_DATA_TYPE_UINT8 = 7,
    HCOMM_DATA_TYPE_UINT16 = 8,
    HCOMM_DATA_TYPE_UINT32 = 9,
    HCOMM_DATA_TYPE_FP64 = 10,
    HCOMM_DATA_TYPE_BFP16 = 11,
    HCOMM_DATA_TYPE_INT128 = 12,
    HCOMM_DATA_TYPE_HIF8 = 14,
    HCOMM_DATA_TYPE_FP8E4M3 = 15,
    HCOMM_DATA_TYPE_FP8E5M2 = 16,
    HCOMM_DATA_TYPE_FP8E8M0 = 17,
    HCOMM_DATA_TYPE_RESERVED = 255,
};

struct HcommBatchTransferDesc {
    HcommTransferType transType{HCOMM_TRANSFER_TYPE_INVALID};
    uint8_t reserved[4]{};
    union {
        uint8_t raws[56];
        struct {
            uint64_t len;
            void *dst;
            void *src;
        } write;
        struct {
            uint64_t len;
            void *dst;
            void *src;
        } read;
        struct {
            uint64_t count;
            void *dst;
            void *src;
            HcommReduceOp reduceOp;
            HcommDataType dataType;
        } reduce;
        struct {
            uint32_t notifyIdx;
        } notifyRecord;
        struct {
            uint32_t notifyIdx;
            uint32_t timeOut;
        } notifyWait;
        struct {
            uint64_t len;
            void *dst;
            void *src;
            uint32_t notifyIdx;
        } writeWithNotify;
        struct {
            uint64_t count;
            void *dst;
            void *src;
            HcommReduceOp reduceOp;
            HcommDataType dataType;
            uint32_t notifyIdx;
        } writeReduceWithNotify;
    } transferInfo{};
};

// =============================================================================
// HCOMM 函数指针类型定义
// =============================================================================

using hcommEndpointCreateFunc = HcommResult (*)(const EndpointDesc *, EndpointHandle *);
using hcommEndpointDestroyFunc = HcommResult (*)(EndpointHandle);
using hcommMemRegFunc = HcommResult (*)(EndpointHandle, const char *, const HcommCommMem *, HcommMemHandle *);
using hcommMemUnregFunc = HcommResult (*)(EndpointHandle, HcommMemHandle);
using hcommMemExportFunc = HcommResult (*)(EndpointHandle, HcommMemHandle, void **, uint32_t *);
using hcommMemImportFunc = HcommResult (*)(EndpointHandle, const void *, uint32_t, HcommCommMem *);
using hcommMemUnimportFunc = HcommResult (*)(EndpointHandle, const void *, uint32_t);
using hcommChannelCreateFunc = HcommResult (*)(EndpointHandle, CommEngine, HcommChannelDesc *, uint32_t,
                                               ChannelHandle *);
using hcommChannelDestroyFunc = HcommResult (*)(const ChannelHandle *, uint32_t);
using hcommThreadAllocFunc = HcommResult (*)(CommEngine, uint32_t, const uint32_t *, ThreadHandle *);
using hcommThreadFreeFunc = HcommResult (*)(const ThreadHandle *, uint32_t);
using hcommReadOnThreadFunc = int32_t (*)(ThreadHandle, ChannelHandle, void *, const void *, uint64_t);
using hcommWriteOnThreadFunc = int32_t (*)(ThreadHandle, ChannelHandle, void *, const void *, uint64_t);
using hcommChannelFenceOnThreadFunc = int32_t (*)(ThreadHandle, ChannelHandle);
using hcommBatchModeStartFunc = int32_t (*)(const char *);
using hcommBatchModeEndFunc = int32_t (*)(const char *);
using hcommBatchTransferOnThreadFunc = int32_t (*)(ThreadHandle, ChannelHandle, const HcommBatchTransferDesc *,
                                                   uint32_t);

// =============================================================================
// 初始化工具函数（对应 hcomm_res_defs.h 中的 EndpointDescInit / HcommChannelDescInit）
// =============================================================================

/**
 * @brief 初始化EndpointDesc结构体
 *
 * @param[inout] endpoint 返回的端点描述参数
 * @param[in] num 描述数量
 * @return HcommResult 执行结果状态码
 */
static inline HcommResult EndpointDescInit(EndpointDesc *endpoint, uint32_t num)
{
    const HcommResult hcommEPointer = static_cast<HcommResult>(2); // 对齐HCCL_E_PTR

    for (uint32_t idx = 0; idx < num; ++idx) {
        if (endpoint == nullptr) {
            return hcommEPointer;
        }

        // 用0xFF填充整个结构体
        std::memset(endpoint, 0xFF, sizeof(EndpointDesc));

        // 显式设置关键字段为无效值
        endpoint->protocol = COMM_PROTOCOL_RESERVED;
        endpoint->commAddr.type = COMM_ADDR_TYPE_RESERVED;
        endpoint->loc.locType = ENDPOINT_LOC_TYPE_RESERVED;
        ++endpoint; // 移动到下一个描述符
    }

    return 0;
}

/**
 * @brief 初始化CommLink结构体
 *
 * @param[inout] commLink 通信link信息列表
 * @param[in] linkNum link数量
 * @return HcommResult 执行结果状态码
 */
static inline HcommResult CommLinkInit(CommLink *commLink, uint32_t linkNum)
{
    const HcommResult hcommEPointer = static_cast<HcommResult>(2);   // 对齐HCCL_E_PTR
    const HcommResult hcommEInternal = static_cast<HcommResult>(4);   // 对齐HCCL_E_INTERNAL

    for (uint32_t idx = 0; idx < linkNum; ++idx) {
        if (commLink == nullptr) {
            return hcommEPointer;
        }

        std::memset(commLink, 0xFF, sizeof(CommLink));
        commLink->header.version = COMM_LINK_VERSION;
        commLink->header.magicWord = COMM_LINK_MAGIC_WORD;
        commLink->header.size = sizeof(CommLink);
        commLink->header.reserved = 0;
        if (EndpointDescInit(&commLink->srcEndpointDesc, 1) != 0 ||
            EndpointDescInit(&commLink->dstEndpointDesc, 1) != 0) {
            return hcommEInternal;
        }
        commLink->linkAttr.linkProtocol = COMM_PROTOCOL_RESERVED;
        commLink->linkAttr.hop = 1;
        ++commLink;
    }

    return 0;
}

/**
 * @brief 初始化HcommChannelDesc结构体
 *
 * @param[inout] channelDesc 返回的通道描述参数
 * @param[in] descNum 描述数量
 * @return HcommResult 执行结果状态码
 */
static inline HcommResult HcommChannelDescInit(HcommChannelDesc *channelDesc, uint32_t descNum)
{
    const HcommResult hcommEPointer = static_cast<HcommResult>(2);   // 对齐HCCL_E_PTR
    const HcommResult hcommEInternal = static_cast<HcommResult>(4);   // 对齐HCCL_E_INTERNAL

    for (uint32_t idx = 0; idx < descNum; ++idx) {
        if (channelDesc == nullptr) {
            return hcommEPointer;
        }

        std::memset(channelDesc, 0xFF, sizeof(HcommChannelDesc));
        channelDesc->header.version = HCOMM_CHANNEL_VERSION;
        channelDesc->header.magicWord = HCOMM_CHANNEL_MAGIC_WORD;
        channelDesc->header.size = sizeof(HcommChannelDesc);
        channelDesc->header.reserved = 0;
        channelDesc->notifyNum = 0;
        channelDesc->exchangeAllMems = false;
        channelDesc->memHandles = nullptr;
        channelDesc->memHandleNum = 0;
        channelDesc->socket = nullptr;
        channelDesc->role = HCOMM_SOCKET_ROLE_RESERVED;
        channelDesc->port = 0;
        if (EndpointDescInit(&channelDesc->remoteEndpoint, 1) != 0) {
            return hcommEInternal;
        }
        ++channelDesc;
    }

    return 0;
}

// =============================================================================
// DlHcommApi - HCOMM 动态加载封装类
// =============================================================================

class DlHcommApi {
public:
    static Result LoadLibrary();
    static void CleanupLibrary();

    static inline int32_t HcommEndpointCreate(const EndpointDesc *endpoint, EndpointHandle *endpointHandle)
    {
        if (gHcommEndpointCreate == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommEndpointCreate(endpoint, endpointHandle);
    }

    static inline int32_t HcommEndpointDestroy(EndpointHandle endpointHandle)
    {
        if (gHcommEndpointDestroy == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommEndpointDestroy(endpointHandle);
    }

    static inline int32_t HcommMemReg(EndpointHandle endpoint, const char *memTag, const HcommCommMem *mem,
                                      HcommMemHandle *memHandle)
    {
        if (gHcommMemReg == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommMemReg(endpoint, memTag, mem, memHandle);
    }

    static inline int32_t HcommMemUnreg(EndpointHandle endpoint, HcommMemHandle memHandle)
    {
        if (gHcommMemUnreg == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommMemUnreg(endpoint, memHandle);
    }

    static inline int32_t HcommMemExport(EndpointHandle endpoint, HcommMemHandle memHandle, void **memDesc,
                                         uint32_t *memDescLen)
    {
        if (gHcommMemExport == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommMemExport(endpoint, memHandle, memDesc, memDescLen);
    }

    static inline int32_t HcommMemImport(EndpointHandle endpoint, const void *memDesc, uint32_t descLen,
                                         HcommCommMem *commMem)
    {
        if (gHcommMemImport == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommMemImport(endpoint, memDesc, descLen, commMem);
    }

    static inline int32_t HcommMemUnimport(EndpointHandle endpoint, const void *memDesc, uint32_t descLen)
    {
        if (gHcommMemUnimport == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommMemUnimport(endpoint, memDesc, descLen);
    }

    static inline int32_t HcommChannelCreate(EndpointHandle endpoint, CommEngine engine, HcommChannelDesc *channelDescs,
                                             uint32_t channelNum, ChannelHandle *channels)
    {
        if (gHcommChannelCreate == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommChannelCreate(endpoint, engine, channelDescs, channelNum, channels);
    }

    static inline int32_t HcommChannelDestroy(const ChannelHandle *channels, uint32_t channelNum)
    {
        if (gHcommChannelDestroy == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommChannelDestroy(channels, channelNum);
    }

    static inline int32_t HcommThreadAlloc(CommEngine engine, uint32_t threadNum, const uint32_t *notifyNumPerThread,
                                           ThreadHandle *threads)
    {
        if (gHcommThreadAlloc == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommThreadAlloc(engine, threadNum, notifyNumPerThread, threads);
    }

    static inline int32_t HcommThreadFree(const ThreadHandle *threads, uint32_t threadNum)
    {
        if (gHcommThreadFree == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommThreadFree(threads, threadNum);
    }

    static inline int32_t HcommReadOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
                                            uint64_t len)
    {
        if (gHcommReadOnThread == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommReadOnThread(thread, channel, dst, src, len);
    }

    static inline int32_t HcommWriteOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
                                             uint64_t len)
    {
        if (gHcommWriteOnThread == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommWriteOnThread(thread, channel, dst, src, len);
    }

    static inline int32_t HcommChannelFenceOnThread(ThreadHandle thread, ChannelHandle channel)
    {
        if (gHcommChannelFenceOnThread == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gHcommChannelFenceOnThread(thread, channel);
    }

    static inline int32_t HcommBatchModeStart(const char *batchTag)
    {
        if (gHcommBatchModeStart == nullptr) {
            return BM_NOT_SUPPORTED;
        }
        return gHcommBatchModeStart(batchTag);
    }

    static inline int32_t HcommBatchModeEnd(const char *batchTag)
    {
        if (gHcommBatchModeEnd == nullptr) {
            return BM_NOT_SUPPORTED;
        }
        return gHcommBatchModeEnd(batchTag);
    }

    static inline int32_t HcommBatchTransferOnThread(ThreadHandle thread, ChannelHandle channel,
                                                     const HcommBatchTransferDesc *transferDescs,
                                                     uint32_t transferDescNum)
    {
        if (gHcommBatchTransferOnThread == nullptr) {
            return BM_NOT_SUPPORTED;
        }
        return gHcommBatchTransferOnThread(thread, channel, transferDescs, transferDescNum);
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *hcommHandle;
    static const char *hcommLibName;

    static hcommEndpointCreateFunc gHcommEndpointCreate;
    static hcommEndpointDestroyFunc gHcommEndpointDestroy;
    static hcommMemRegFunc gHcommMemReg;
    static hcommMemUnregFunc gHcommMemUnreg;
    static hcommMemExportFunc gHcommMemExport;
    static hcommMemImportFunc gHcommMemImport;
    static hcommMemUnimportFunc gHcommMemUnimport;
    static hcommChannelCreateFunc gHcommChannelCreate;
    static hcommChannelDestroyFunc gHcommChannelDestroy;
    static hcommThreadAllocFunc gHcommThreadAlloc;
    static hcommThreadFreeFunc gHcommThreadFree;
    static hcommReadOnThreadFunc gHcommReadOnThread;
    static hcommWriteOnThreadFunc gHcommWriteOnThread;
    static hcommChannelFenceOnThreadFunc gHcommChannelFenceOnThread;
    static hcommBatchModeStartFunc gHcommBatchModeStart;
    static hcommBatchModeEndFunc gHcommBatchModeEnd;
    static hcommBatchTransferOnThreadFunc gHcommBatchTransferOnThread;
};

} // namespace mf
} // namespace ock

#endif // MF_HYBM_CORE_DL_HCOMM_API_H
