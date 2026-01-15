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
#ifndef MF_HYBM_CORE_DL_HCCP_API_H
#define MF_HYBM_CORE_DL_HCCP_API_H

#include "hybm_common_include.h"
#include "dl_hccp_def.h"

namespace ock {
namespace mf {

using raRdevGetHandleFunc = int (*)(uint32_t, void **);

using raGetInterfaceVersionFunc = int (*)(uint32_t, uint32_t, uint32_t *);
using raInitFunc = int (*)(const HccpRaInitConfig *);
using raSocketInitFunc = int (*)(HccpNetworkMode, HccpRdev, void **);
using raSocketDeinitFunc = int (*)(void *);
using raRdevInitV2Func = int (*)(HccpRdevInitInfo, HccpRdev, void **);
using raSocketBatchConnectFunc = int (*)(HccpSocketConnectInfo[], uint32_t);
using raSocketBatchCloseFunc = int (*)(HccpSocketCloseInfo[], uint32_t);
using raSocketBatchAbortFunc = int (*)(HccpSocketConnectInfo[], uint32_t);
using raSocketListenStartFunc = int (*)(HccpSocketListenInfo[], uint32_t);
using raSocketListenStopFunc = int (*)(HccpSocketListenInfo[], uint32_t);
using raGetSocketsFunc = int (*)(uint32_t, HccpSocketInfo[], uint32_t, uint32_t *);
using raSocketSendFunc = int (*)(const void *, const void *, uint64_t, uint64_t *);
using raSocketRecvFunc = int (*)(const void *, void *, uint64_t, uint64_t *);
using raGetIfNumFunc = int (*)(const HccpRaGetIfAttr *, uint32_t *);
using raGetIfAddrsFunc = int (*)(const HccpRaGetIfAttr *, HccpInterfaceInfo[], uint32_t *);
using raSocketWhiteListAddFunc = int (*)(void *, const HccpSocketWhiteListInfo[], uint32_t num);
using raSocketWhiteListDelFunc = int (*)(void *, const HccpSocketWhiteListInfo[], uint32_t num);
using raQpCreateFunc = int (*)(void *, int, int, void **);
using raQpAiCreateFunc = int (*)(void *, const HccpQpExtAttrs *, HccpAiQpInfo *, void **);
using raQpDestroyFunc = int (*)(void *);
using raGetQpStatusFunc = int (*)(void *, int *);
using raQpConnectAsyncFunc = int (*)(void *, const void *);
using raRegisterMrFunc = int (*)(const void *, HccpMrInfo *, void **);
using raDeregisterMrFunc = int (*)(const void *, void *);
using raMrRegFunc = int (*)(void *, HccpMrInfo *);
using raMrDeregFunc = int (*)(void *, HccpMrInfo *);
using raSendWrFunc = int (*)(void *, send_wr *, send_wr_rsp *);
using raSendWrV2Func = int (*)(void *, send_wr_v2 *, send_wr_rsp *);
using tsdOpenFunc = uint32_t (*)(uint32_t, uint32_t);
using raPollCqFunc = int (*)(void *, bool, uint32_t, void *);
using raGetNotifyBaseAddrFunc = int (*)(void *, uint64_t *, uint64_t *);
using raGetNotifyMrInfoFunc = int (*)(void *, HccpMrInfo *);

class DlHccpApi {
public:
    static Result LoadLibrary();
    static void CleanupLibrary();

    static inline int RaSocketInit(HccpNetworkMode mode, const HccpRdev &rdev, void *&socketHandle)
    {
        if (gRaSocketInit == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaSocketInit(mode, rdev, &socketHandle);
    }

    static inline int RaInit(const HccpRaInitConfig &config)
    {
        if (gRaInit == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaInit(&config);
    }

    static inline int RaSocketDeinit(void *socketHandle)
    {
        if (gRaSocketDeinit == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaSocketDeinit(socketHandle);
    }

    static inline int RaRdevInitV2(const HccpRdevInitInfo &info, const HccpRdev &rdev, void *&rdmaHandle)
    {
        if (gRaRdevInitV2 == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaRdevInitV2(info, rdev, &rdmaHandle);
    }

    static inline int RaRdevGetHandle(uint32_t deviceId, void *&rdmaHandle)
    {
        if (gRaRdevGetHandle == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaRdevGetHandle(deviceId, &rdmaHandle);
    }

    static inline int RaSocketBatchConnect(HccpSocketConnectInfo conn[], uint32_t num)
    {
        if (gRaSocketBatchConnect == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaSocketBatchConnect(conn, num);
    }

    static inline int RaSocketBatchClose(HccpSocketCloseInfo conn[], uint32_t num)
    {
        if (gRaSocketBatchClose == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaSocketBatchClose(conn, num);
    }

    static inline int RaSocketListenStart(HccpSocketListenInfo conn[], uint32_t num)
    {
        if (gRaSocketListenStart == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaSocketListenStart(conn, num);
    }

    static inline int RaSocketListenStop(HccpSocketListenInfo conn[], uint32_t num)
    {
        if (gRaSocketListenStop == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaSocketListenStop(conn, num);
    }

    static inline int RaGetSockets(uint32_t role, HccpSocketInfo conn[], uint32_t num, uint32_t &connectedNum)
    {
        if (gRaGetSockets == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaGetSockets(role, conn, num, &connectedNum);
    }

    static inline int RaGetIfNum(const HccpRaGetIfAttr &config, uint32_t &num)
    {
        if (gRaGetIfNum == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaGetIfNum(&config, &num);
    }

    static inline int RaGetIfAddrs(const HccpRaGetIfAttr &config, HccpInterfaceInfo infos[], uint32_t &num)
    {
        if (gRaGetIfAddrs == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaGetIfAddrs(&config, infos, &num);
    }

    static inline int RaSocketWhiteListAdd(void *socket, const HccpSocketWhiteListInfo list[], uint32_t num)
    {
        if (gRaSocketWhiteListAdd == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaSocketWhiteListAdd(socket, list, num);
    }

    static inline int RaSocketWhiteListDel(void *socket, const HccpSocketWhiteListInfo list[], uint32_t num)
    {
        if (gRaSocketWhiteListDel == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaSocketWhiteListDel(socket, list, num);
    }

    static inline int RaQpCreate(void *rdmaHandle, int flag, int qpMode, void *&qpHandle)
    {
        if (gRaQpCreate == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaQpCreate(rdmaHandle, flag, qpMode, &qpHandle);
    }

    static inline int RaQpAiCreate(void *rdmaHandle, const HccpQpExtAttrs &attrs, HccpAiQpInfo &info, void *&qpHandle)
    {
        if (gRaQpAiCreate == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaQpAiCreate(rdmaHandle, &attrs, &info, &qpHandle);
    }

    static inline int RaQpDestroy(void *qpHandle)
    {
        if (gRaQpDestroy == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaQpDestroy(qpHandle);
    }

    static inline int RaGetQpStatus(void *qpHandle, int &status)
    {
        if (gRaGetQpStatus == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaGetQpStatus(qpHandle, &status);
    }

    static inline int RaQpConnectAsync(void *qp, const void *socketFd)
    {
        if (gRaQpConnectAsync == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaQpConnectAsync(qp, socketFd);
    }

    static inline int RaRegisterMR(const void *rdmaHandle, HccpMrInfo *info, void *&mrHandle)
    {
        if (gRaRegisterMR == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaRegisterMR(rdmaHandle, info, &mrHandle);
    }

    static inline int RaDeregisterMR(const void *rdmaHandle, void *mrHandle)
    {
        if (gRaDeregisterMR == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaDeregisterMR(rdmaHandle, mrHandle);
    }

    static inline int RaSendWr(void *qp_handle, struct send_wr *wr, struct send_wr_rsp *op_rsp)
    {
        if (gRaSendWr == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaSendWr(qp_handle, wr, op_rsp);
    }

    static inline int RaSendWrV2(void *qp_handle, struct send_wr_v2 *wr, struct send_wr_rsp *op_rsp)
    {
        if (gRaSendWrV2 == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaSendWrV2(qp_handle, wr, op_rsp);
    }

    static inline uint32_t TsdOpen(uint32_t deviceId, uint32_t rankSize)
    {
        if (gTsdOpen == nullptr) {
            return UINT32_MAX;
        }
        return gTsdOpen(deviceId, rankSize);
    }

    static inline int RaGetNotifyBaseAddr(void *rdmaHandle, uint64_t *va, uint64_t *size)
    {
        if (gRaGetNotifyBaseAddr == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaGetNotifyBaseAddr(rdmaHandle, va, size);
    }

    static inline int RaGetNotifyMrInfo(void *rdmaHandle, HccpMrInfo *info)
    {
        if (gRaGetNotifyMrInfo == nullptr) {
            return BM_UNDER_API_UNLOAD;
        }
        return gRaGetNotifyMrInfo(rdmaHandle, info);
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *raHandle;
    static void *tsdHandle;
    static const char *gRaLibName;
    static const char *gTsdLibName;

    static raRdevGetHandleFunc gRaRdevGetHandle;
    static raInitFunc gRaInit;
    static raSocketInitFunc gRaSocketInit;
    static raSocketDeinitFunc gRaSocketDeinit;
    static raRdevInitV2Func gRaRdevInitV2;
    static raSocketBatchConnectFunc gRaSocketBatchConnect;
    static raSocketBatchCloseFunc gRaSocketBatchClose;
    static raSocketListenStartFunc gRaSocketListenStart;
    static raSocketListenStopFunc gRaSocketListenStop;
    static raGetSocketsFunc gRaGetSockets;
    static raGetIfNumFunc gRaGetIfNum;
    static raGetIfAddrsFunc gRaGetIfAddrs;
    static raSocketWhiteListAddFunc gRaSocketWhiteListAdd;
    static raSocketWhiteListDelFunc gRaSocketWhiteListDel;
    static raQpCreateFunc gRaQpCreate;
    static raQpAiCreateFunc gRaQpAiCreate;
    static raQpDestroyFunc gRaQpDestroy;
    static raGetQpStatusFunc gRaGetQpStatus;
    static raQpConnectAsyncFunc gRaQpConnectAsync;
    static raRegisterMrFunc gRaRegisterMR;
    static raDeregisterMrFunc gRaDeregisterMR;
    static raSendWrFunc gRaSendWr;
    static raSendWrV2Func gRaSendWrV2;
    static raGetNotifyBaseAddrFunc gRaGetNotifyBaseAddr;
    static raGetNotifyMrInfoFunc gRaGetNotifyMrInfo;

    static tsdOpenFunc gTsdOpen;
};

} // namespace mf
} // namespace ock

#endif // MF_HYBM_CORE_DL_HCCP_API_H
