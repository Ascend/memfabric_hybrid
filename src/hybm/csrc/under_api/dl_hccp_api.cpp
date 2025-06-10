/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include <dlfcn.h>
#include "dl_hccp_api.h"

namespace ock {
namespace mf {
bool RuntimeHccpApi::gLoaded = false;
std::mutex RuntimeHccpApi::gMutex;
void *RuntimeHccpApi::raHandle;
void *RuntimeHccpApi::tsdHandle;

const char *RuntimeHccpApi::gRaLibName = "libra.so";
const char *RuntimeHccpApi::gTsdLibName = "libtsdclient.so";

raRdevGetHandleFunc RuntimeHccpApi::gRaRdevGetHandle;

raInitFunc RuntimeHccpApi::gRaInit;
raSocketInitFunc RuntimeHccpApi::gRaSocketInit;
raSocketDeinitFunc RuntimeHccpApi::gRaSocketDeinit;
raRdevInitV2Func RuntimeHccpApi::gRaRdevInitV2;
raSocketBatchConnectFunc RuntimeHccpApi::gRaSocketBatchConnect;
raSocketBatchCloseFunc RuntimeHccpApi::gRaSocketBatchClose;
raSocketBatchAbortFunc RuntimeHccpApi::gRaSocketBatchAbort;
raSocketListenStartFunc RuntimeHccpApi::gRaSocketListenStart;
raSocketListenStopFunc RuntimeHccpApi::gRaSocketListenStop;
raGetSocketsFunc RuntimeHccpApi::gRaGetSockets;
raSocketSendFunc RuntimeHccpApi::gRaSocketSend;
raSocketRecvFunc RuntimeHccpApi::gRaSocketRecv;
raGetIfNumFunc RuntimeHccpApi::gRaGetIfNum;
raGetIfAddrsFunc RuntimeHccpApi::gRaGetIfAddrs;
raSocketWhiteListAddFunc RuntimeHccpApi::gRaSocketWhiteListAdd;
raSocketWhiteListDelFunc RuntimeHccpApi::gRaSocketWhiteListDel;
raQpCreateFunc RuntimeHccpApi::gRaQpCreate;
raQpAiCreateFunc RuntimeHccpApi::gRaQpAiCreate;
raQpDestroyFunc RuntimeHccpApi::gRaQpDestroy;
raGetQpStatusFunc RuntimeHccpApi::gRaGetQpStatus;
raQpConnectAsyncFunc RuntimeHccpApi::gRaQpConnectAsync;
raRegisterMrFunc RuntimeHccpApi::gRaRegisterMR;
raDeregisterMrFunc RuntimeHccpApi::gRaDeregisterMR;
raMrRegFunc RuntimeHccpApi::gRaMrReg;
raMrDeregFunc RuntimeHccpApi::gRaMrDereg;

tsdOpenFunc RuntimeHccpApi::gTsdOpen;

Result RuntimeHccpApi::LoadLibrary()
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
    DL_LOAD_SYM(gRaRegisterMR, raRegisterMrFunc, raHandle, "ra_register_mr");
    DL_LOAD_SYM(gRaDeregisterMR, raDeregisterMrFunc, raHandle, "ra_deregister_mr");
    DL_LOAD_SYM(gRaMrReg, raMrRegFunc, raHandle, "ra_mr_reg");
    DL_LOAD_SYM(gRaMrDereg, raMrDeregFunc, raHandle, "ra_mr_dereg");

    DL_LOAD_SYM(gTsdOpen, tsdOpenFunc, tsdHandle, "TsdOpen");

    gLoaded = true;
    return BM_OK;
}
}
}