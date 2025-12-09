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

#include <dlfcn.h>
#include "dl_hccp_api.h"

namespace ock {
namespace mf {
bool DlHccpApi::gLoaded = false;
std::mutex DlHccpApi::gMutex;
void *DlHccpApi::raHandle;
void *DlHccpApi::tsdHandle;

const char *DlHccpApi::gRaLibName = "libra.so";
const char *DlHccpApi::gTsdLibName = "libtsdclient.so";

raRdevGetHandleFunc DlHccpApi::gRaRdevGetHandle;

raInitFunc DlHccpApi::gRaInit;
raGetInterfaceVersionFunc DlHccpApi::gRaGetInterfaceVersion;
raSocketInitFunc DlHccpApi::gRaSocketInit;
raSocketDeinitFunc DlHccpApi::gRaSocketDeinit;
raRdevInitV2Func DlHccpApi::gRaRdevInitV2;
raSocketBatchConnectFunc DlHccpApi::gRaSocketBatchConnect;
raSocketBatchCloseFunc DlHccpApi::gRaSocketBatchClose;
raSocketBatchAbortFunc DlHccpApi::gRaSocketBatchAbort;
raSocketListenStartFunc DlHccpApi::gRaSocketListenStart;
raSocketListenStopFunc DlHccpApi::gRaSocketListenStop;
raGetSocketsFunc DlHccpApi::gRaGetSockets;
raSocketSendFunc DlHccpApi::gRaSocketSend;
raSocketRecvFunc DlHccpApi::gRaSocketRecv;
raGetIfNumFunc DlHccpApi::gRaGetIfNum;
raGetIfAddrsFunc DlHccpApi::gRaGetIfAddrs;
raSocketWhiteListAddFunc DlHccpApi::gRaSocketWhiteListAdd;
raSocketWhiteListDelFunc DlHccpApi::gRaSocketWhiteListDel;
raQpCreateFunc DlHccpApi::gRaQpCreate;
raQpAiCreateFunc DlHccpApi::gRaQpAiCreate;
raQpDestroyFunc DlHccpApi::gRaQpDestroy;
raGetQpStatusFunc DlHccpApi::gRaGetQpStatus;
raQpConnectAsyncFunc DlHccpApi::gRaQpConnectAsync;
raRegisterMrFunc DlHccpApi::gRaRegisterMR;
raDeregisterMrFunc DlHccpApi::gRaDeregisterMR;
raMrRegFunc DlHccpApi::gRaMrReg;
raMrDeregFunc DlHccpApi::gRaMrDereg;
raSendWrFunc DlHccpApi::gRaSendWr;
raSendWrV2Func DlHccpApi::gRaSendWrV2;
raPollCqFunc DlHccpApi::gRaPollCq;
raGetNotifyBaseAddrFunc DlHccpApi::gRaGetNotifyBaseAddr;
raGetNotifyMrInfoFunc DlHccpApi::gRaGetNotifyMrInfo;

tsdOpenFunc DlHccpApi::gTsdOpen;

Result DlHccpApi::LoadLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }

    raHandle = dlopen(gRaLibName, RTLD_NOW);
    if (raHandle == nullptr) {
        BM_LOG_ERROR(
            "Failed to open library ["
            << gRaLibName
            << "], please source ascend-toolkit set_env.sh, or add ascend driver lib path into LD_LIBRARY_PATH,"
            << " error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }

    tsdHandle = dlopen(gTsdLibName, RTLD_NOW);
    if (tsdHandle == nullptr) {
        BM_LOG_ERROR(
            "Failed to open library ["
            << gTsdLibName
            << "], please source ascend-toolkit set_env.sh, or add ascend driver lib path into LD_LIBRARY_PATH,"
            << " error: " << dlerror());
        dlclose(raHandle);
        raHandle = nullptr;
        return BM_DL_FUNCTION_FAILED;
    }

    /* load sym */
    DL_LOAD_SYM(gRaGetInterfaceVersion, raGetInterfaceVersionFunc, raHandle, "ra_get_interface_version");
    DL_LOAD_SYM(gRaSocketInit, raSocketInitFunc, raHandle, "ra_socket_init");
    DL_LOAD_SYM(gRaInit, raInitFunc, raHandle, "ra_init");
    DL_LOAD_SYM(gRaSocketDeinit, raSocketDeinitFunc, raHandle, "ra_socket_deinit");
    DL_LOAD_SYM(gRaRdevInitV2, raRdevInitV2Func, raHandle, "ra_rdev_init_v2");
    DL_LOAD_SYM(gRaRdevGetHandle, raRdevGetHandleFunc, raHandle, "ra_rdev_get_handle");
    DL_LOAD_SYM(gRaSocketBatchConnect, raSocketBatchConnectFunc, raHandle, "ra_socket_batch_connect");
    DL_LOAD_SYM(gRaSocketBatchClose, raSocketBatchCloseFunc, raHandle, "ra_socket_batch_close");
    DL_LOAD_SYM(gRaSocketBatchAbort, raSocketBatchAbortFunc, raHandle, "ra_socket_batch_abort");
    DL_LOAD_SYM(gRaSocketListenStart, raSocketListenStartFunc, raHandle, "ra_socket_listen_start");
    DL_LOAD_SYM(gRaSocketListenStop, raSocketListenStopFunc, raHandle, "ra_socket_listen_stop");
    DL_LOAD_SYM(gRaGetSockets, raGetSocketsFunc, raHandle, "ra_get_sockets");
    DL_LOAD_SYM(gRaSocketSend, raSocketSendFunc, raHandle, "ra_socket_send");
    DL_LOAD_SYM(gRaSocketRecv, raSocketRecvFunc, raHandle, "ra_socket_recv");
    DL_LOAD_SYM(gRaGetIfNum, raGetIfNumFunc, raHandle, "ra_get_ifnum");
    DL_LOAD_SYM(gRaGetIfAddrs, raGetIfAddrsFunc, raHandle, "ra_get_ifaddrs");
    DL_LOAD_SYM(gRaSocketWhiteListAdd, raSocketWhiteListAddFunc, raHandle, "ra_socket_white_list_add");
    DL_LOAD_SYM(gRaSocketWhiteListDel, raSocketWhiteListDelFunc, raHandle, "ra_socket_white_list_del");
    DL_LOAD_SYM(gRaQpCreate, raQpCreateFunc, raHandle, "ra_qp_create");
    DL_LOAD_SYM(gRaQpAiCreate, raQpAiCreateFunc, raHandle, "ra_ai_qp_create");
    DL_LOAD_SYM(gRaQpDestroy, raQpDestroyFunc, raHandle, "ra_qp_destroy");
    DL_LOAD_SYM(gRaGetQpStatus, raGetQpStatusFunc, raHandle, "ra_get_qp_status");
    DL_LOAD_SYM(gRaQpConnectAsync, raQpConnectAsyncFunc, raHandle, "ra_qp_connect_async");
    DL_LOAD_SYM(gRaRegisterMR, raRegisterMrFunc, raHandle, "ra_register_mr");
    DL_LOAD_SYM(gRaDeregisterMR, raDeregisterMrFunc, raHandle, "ra_deregister_mr");
    DL_LOAD_SYM(gRaMrReg, raMrRegFunc, raHandle, "ra_mr_reg");
    DL_LOAD_SYM(gRaMrDereg, raMrDeregFunc, raHandle, "ra_mr_dereg");
    DL_LOAD_SYM(gRaSendWr, raSendWrFunc, raHandle, "ra_send_wr");
    DL_LOAD_SYM(gRaSendWrV2, raSendWrV2Func, raHandle, "ra_send_wr_v2");
    DL_LOAD_SYM(gRaPollCq, raPollCqFunc, raHandle, "ra_poll_cq");
    DL_LOAD_SYM(gRaGetNotifyBaseAddr, raGetNotifyBaseAddrFunc, raHandle, "ra_get_notify_base_addr");
    DL_LOAD_SYM(gRaGetNotifyMrInfo, raGetNotifyMrInfoFunc, raHandle, "ra_get_notify_mr_info");

    DL_LOAD_SYM(gTsdOpen, tsdOpenFunc, tsdHandle, "TsdOpen");
    BM_LOG_INFO("LoadLibrary for DlHccpApi success");
    gLoaded = true;
    return BM_OK;
}

void DlHccpApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }

    gRaRdevGetHandle = nullptr;
    gRaInit = nullptr;
    gRaGetInterfaceVersion = nullptr;
    gRaSocketInit = nullptr;
    gRaSocketDeinit = nullptr;
    gRaRdevInitV2 = nullptr;
    gRaSocketBatchConnect = nullptr;
    gRaSocketBatchClose = nullptr;
    gRaSocketBatchAbort = nullptr;
    gRaSocketListenStart = nullptr;
    gRaSocketListenStop = nullptr;
    gRaGetSockets = nullptr;
    gRaSocketSend = nullptr;
    gRaSocketRecv = nullptr;
    gRaGetIfNum = nullptr;
    gRaGetIfAddrs = nullptr;
    gRaSocketWhiteListAdd = nullptr;
    gRaSocketWhiteListDel = nullptr;
    gRaQpCreate = nullptr;
    gRaQpAiCreate = nullptr;
    gRaQpDestroy = nullptr;
    gRaGetQpStatus = nullptr;
    gRaQpConnectAsync = nullptr;
    gRaRegisterMR = nullptr;
    gRaDeregisterMR = nullptr;
    gRaMrReg = nullptr;
    gRaMrDereg = nullptr;
    gTsdOpen = nullptr;
    gRaSendWr = nullptr;
    gRaPollCq = nullptr;

    // close raHandle may lead to core dump

    if (tsdHandle != nullptr) {
        dlclose(tsdHandle);
        tsdHandle = nullptr;
    }
    gLoaded = false;
}
}
}