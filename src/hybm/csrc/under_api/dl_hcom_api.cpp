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
#include "dl_hcom_api.h"

using namespace ock::mf;

bool DlHcomApi::gLoaded = false;
std::mutex DlHcomApi::gMutex;
void *DlHcomApi::hcomHandle = nullptr;
const char *DlHcomApi::hcomLibName = "libhcom.so";

serviceCreateFunc DlHcomApi::gServiceCreate = nullptr;
serviceBindFunc DlHcomApi::gServiceBind = nullptr;
serviceStartFunc DlHcomApi::gServiceStart = nullptr;
serviceDestroyFunc DlHcomApi::gServiceDestroy = nullptr;
serviceConnectFunc DlHcomApi::gServiceConnect = nullptr;
serviceDisConnectFunc DlHcomApi::gServiceDisConnectFunc = nullptr;
serviceRegisterMemoryRegionFunc DlHcomApi::gServiceRegisterMemoryRegion = nullptr;
serviceGetMemoryRegionInfoFunc DlHcomApi::gServiceGetMemoryRegionInfo = nullptr;
serviceRegisterAssignMemoryRegionFunc DlHcomApi::gServiceRegisterAssignMemoryRegion = nullptr;
serviceDestroyMemoryRegionFunc DlHcomApi::gServiceDestroyMemoryRegion = nullptr;
serviceRegisterChannelBrokerHandlerFunc DlHcomApi::gServiceRegisterChannelBrokerHandler = nullptr;
serviceRegisterIdleHandlerFunc DlHcomApi::gServiceRegisterIdleHandler = nullptr;
serviceRegisterHandlerFunc DlHcomApi::gServiceRegisterHandler = nullptr;
serviceAddWorkerGroupFunc DlHcomApi::gServiceAddWorkerGroup = nullptr;
serviceAddListenerFunc DlHcomApi::gServiceAddListener = nullptr;
serviceSetConnectLBPolicyFunc DlHcomApi::gServiceSetConnectLBPolicy = nullptr;
serviceSetTlsOptionsFunc DlHcomApi::gServiceSetTlsOptions = nullptr;
serviceSetSecureOptionsFunc DlHcomApi::gServiceSetSecureOptions = nullptr;
serviceSetTcpUserTimeOutSecFunc DlHcomApi::gServiceSetTcpUserTimeOutSec = nullptr;
serviceSetTcpSendZCopyFunc DlHcomApi::gServiceSetTcpSendZCopy = nullptr;
serviceSetDeviceIpMaskFunc DlHcomApi::gServiceSetDeviceIpMask = nullptr;
serviceSetDeviceIpGroupFunc DlHcomApi::gServiceSetDeviceIpGroup = nullptr;
serviceSetCompletionQueueDepthFunc DlHcomApi::gServiceSetCompletionQueueDepth = nullptr;
serviceSetSendQueueSizeFunc DlHcomApi::gServiceSetSendQueueSize = nullptr;
serviceSetRecvQueueSizeFunc DlHcomApi::gServiceSetRecvQueueSize = nullptr;
serviceSetQueuePrePostSizeFunc DlHcomApi::gServiceSetQueuePrePostSize = nullptr;
serviceSetPollingBatchSizeFunc DlHcomApi::gServiceSetPollingBatchSize = nullptr;
serviceSetEventPollingTimeOutUsFunc DlHcomApi::gServiceSetEventPollingTimeOutUs = nullptr;
serviceSetTimeOutDetectionThreadNumFunc DlHcomApi::gServiceSetTimeOutDetectionThreadNum = nullptr;
serviceSetMaxConnectionCountFunc DlHcomApi::gServiceSetMaxConnectionCount = nullptr;
serviceSetHeartBeatOptionsFunc DlHcomApi::gServiceSetHeartBeatOptions = nullptr;
serviceSetMultiRailOptionsFunc DlHcomApi::gServiceSetMultiRailOptions = nullptr;
channelSendFunc DlHcomApi::gChannelSend = nullptr;
channelCallFunc DlHcomApi::gChannelCall = nullptr;
channelReplyFunc DlHcomApi::gChannelReply = nullptr;
channelPutFunc DlHcomApi::gChannelPut = nullptr;
channelGetFunc DlHcomApi::gChannelGet = nullptr;
channelSetFlowControlConfigFunc DlHcomApi::gChannelSetFlowControlConfig = nullptr;
channelSetChannelTimeOutFunc DlHcomApi::gChannelSetChannelTimeOut = nullptr;
contextGetRspCtxFunc DlHcomApi::gContextGetRspCtx = nullptr;
contextGetChannelFunc DlHcomApi::gContextGetChannel = nullptr;
contextGetTypeFunc DlHcomApi::gContextGetType = nullptr;
contextGetResultFunc DlHcomApi::gContextGetResult = nullptr;
contextGetOpCodeFunc DlHcomApi::gContextGetOpCode = nullptr;
contextGetMessageDataFunc DlHcomApi::gContextGetMessageData = nullptr;
contextGetMessageDataLenFunc DlHcomApi::gContextGetMessageDataLen = nullptr;
setExternalLoggerFunc DlHcomApi::gSetExternalLogger = nullptr;
SetUbsMode DlHcomApi::gSetUbsMode = nullptr;
ImportUrmaSeg DlHcomApi::gImportUrmaSeg = nullptr;

Result DlHcomApi::LoadLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }
    hcomHandle = dlopen(hcomLibName, RTLD_NOW | RTLD_NODELETE);
    if (hcomHandle == nullptr) {
        BM_LOG_WARN("Failed to open library [" << hcomLibName << "], error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }
    DL_LOAD_SYM(gServiceCreate, serviceCreateFunc, hcomHandle, "ubs_hcom_service_create");
    DL_LOAD_SYM(gServiceBind, serviceBindFunc, hcomHandle, "ubs_hcom_service_bind");
    DL_LOAD_SYM(gServiceStart, serviceStartFunc, hcomHandle, "ubs_hcom_service_start");
    DL_LOAD_SYM(gServiceDestroy, serviceDestroyFunc, hcomHandle, "ubs_hcom_service_destroy");
    DL_LOAD_SYM(gServiceConnect, serviceConnectFunc, hcomHandle, "ubs_hcom_service_connect");
    DL_LOAD_SYM(gServiceDisConnectFunc, serviceDisConnectFunc, hcomHandle, "ubs_hcom_service_disconnect");
    DL_LOAD_SYM(gServiceRegisterMemoryRegion, serviceRegisterMemoryRegionFunc, hcomHandle,
        "ubs_hcom_service_register_memory_region");
    DL_LOAD_SYM(gServiceGetMemoryRegionInfo, serviceGetMemoryRegionInfoFunc, hcomHandle,
        "ubs_hcom_service_get_memory_region_info");
    DL_LOAD_SYM(gServiceRegisterAssignMemoryRegion, serviceRegisterAssignMemoryRegionFunc, hcomHandle,
        "ubs_hcom_service_register_assign_memory_region");
    DL_LOAD_SYM(gServiceDestroyMemoryRegion, serviceDestroyMemoryRegionFunc, hcomHandle,
        "ubs_hcom_service_destroy_memory_region");
    DL_LOAD_SYM(gServiceRegisterChannelBrokerHandler, serviceRegisterChannelBrokerHandlerFunc, hcomHandle,
        "ubs_hcom_service_register_broken_handler");
    DL_LOAD_SYM(gServiceRegisterIdleHandler, serviceRegisterIdleHandlerFunc, hcomHandle,
        "ubs_hcom_service_register_idle_handler");
    DL_LOAD_SYM(gServiceRegisterHandler, serviceRegisterHandlerFunc, hcomHandle, "ubs_hcom_service_register_handler");
    DL_LOAD_SYM(gServiceAddWorkerGroup, serviceAddWorkerGroupFunc, hcomHandle, "ubs_hcom_service_add_workergroup");
    DL_LOAD_SYM(gServiceAddListener, serviceAddListenerFunc, hcomHandle, "ubs_hcom_service_add_listener");
    DL_LOAD_SYM(gServiceSetConnectLBPolicy, serviceSetConnectLBPolicyFunc, hcomHandle, "ubs_hcom_service_set_lbpolicy");
    DL_LOAD_SYM(gServiceSetTlsOptions, serviceSetTlsOptionsFunc, hcomHandle, "ubs_hcom_service_set_tls_opt");
    DL_LOAD_SYM(gServiceSetSecureOptions, serviceSetSecureOptionsFunc, hcomHandle, "ubs_hcom_service_set_secure_opt");
    DL_LOAD_SYM(gServiceSetTcpUserTimeOutSec, serviceSetTcpUserTimeOutSecFunc, hcomHandle,
        "ubs_hcom_service_set_tcp_usr_timeout");
    DL_LOAD_SYM(gServiceSetTcpSendZCopy, serviceSetTcpSendZCopyFunc, hcomHandle, "ubs_hcom_service_set_tcp_send_zcopy");
    DL_LOAD_SYM(gServiceSetDeviceIpMask, serviceSetDeviceIpMaskFunc, hcomHandle, "ubs_hcom_service_set_ipmask");
    DL_LOAD_SYM(gServiceSetDeviceIpGroup, serviceSetDeviceIpGroupFunc, hcomHandle, "ubs_hcom_service_set_ipgroup");
    DL_LOAD_SYM(gServiceSetCompletionQueueDepth, serviceSetCompletionQueueDepthFunc, hcomHandle,
        "ubs_hcom_service_set_cq_depth");
    DL_LOAD_SYM(gServiceSetSendQueueSize, serviceSetSendQueueSizeFunc, hcomHandle, "ubs_hcom_service_set_sq_size");
    DL_LOAD_SYM(gServiceSetRecvQueueSize, serviceSetRecvQueueSizeFunc, hcomHandle, "ubs_hcom_service_set_rq_size");
    DL_LOAD_SYM(gServiceSetQueuePrePostSize, serviceSetQueuePrePostSizeFunc, hcomHandle,
        "ubs_hcom_service_set_prepost_size");
    DL_LOAD_SYM(gServiceSetPollingBatchSize, serviceSetPollingBatchSizeFunc, hcomHandle,
        "ubs_hcom_service_set_polling_batchsize");
    DL_LOAD_SYM(gServiceSetEventPollingTimeOutUs, serviceSetEventPollingTimeOutUsFunc, hcomHandle,
        "ubs_hcom_service_set_polling_timeoutus");
    DL_LOAD_SYM(gServiceSetTimeOutDetectionThreadNum, serviceSetTimeOutDetectionThreadNumFunc, hcomHandle,
        "ubs_hcom_service_set_timeout_threadnum");
    DL_LOAD_SYM(gServiceSetMaxConnectionCount, serviceSetMaxConnectionCountFunc, hcomHandle,
        "ubs_hcom_service_set_max_connection_cnt");
    DL_LOAD_SYM(gServiceSetHeartBeatOptions, serviceSetHeartBeatOptionsFunc, hcomHandle,
        "ubs_hcom_service_set_heartbeat_opt");
    DL_LOAD_SYM(gServiceSetMultiRailOptions, serviceSetMultiRailOptionsFunc, hcomHandle,
        "ubs_hcom_service_set_multirail_opt");
    DL_LOAD_SYM(gChannelSend, channelSendFunc, hcomHandle, "ubs_hcom_channel_send");
    DL_LOAD_SYM(gChannelCall, channelCallFunc, hcomHandle, "ubs_hcom_channel_call");
    DL_LOAD_SYM(gChannelReply, channelReplyFunc, hcomHandle, "ubs_hcom_channel_reply");
    DL_LOAD_SYM(gChannelPut, channelPutFunc, hcomHandle, "ubs_hcom_channel_put");
    DL_LOAD_SYM(gChannelGet, channelGetFunc, hcomHandle, "ubs_hcom_channel_get");
    DL_LOAD_SYM(gChannelSetFlowControlConfig, channelSetFlowControlConfigFunc, hcomHandle,
        "ubs_hcom_channel_set_flowctl_cfg");
    DL_LOAD_SYM(gChannelSetChannelTimeOut, channelSetChannelTimeOutFunc, hcomHandle, "ubs_hcom_channel_set_timeout");
    DL_LOAD_SYM(gContextGetRspCtx, contextGetRspCtxFunc, hcomHandle, "ubs_hcom_context_get_rspctx");
    DL_LOAD_SYM(gContextGetChannel, contextGetChannelFunc, hcomHandle, "ubs_hcom_context_get_channel");
    DL_LOAD_SYM(gContextGetType, contextGetTypeFunc, hcomHandle, "ubs_hcom_context_get_type");
    DL_LOAD_SYM(gContextGetResult, contextGetResultFunc, hcomHandle, "ubs_hcom_context_get_result");
    DL_LOAD_SYM(gContextGetOpCode, contextGetOpCodeFunc, hcomHandle, "ubs_hcom_context_get_opcode");
    DL_LOAD_SYM(gContextGetMessageData, contextGetMessageDataFunc, hcomHandle, "ubs_hcom_context_get_data");
    DL_LOAD_SYM(gContextGetMessageDataLen, contextGetMessageDataLenFunc, hcomHandle, "ubs_hcom_context_get_datalen");
    DL_LOAD_SYM(gSetExternalLogger, setExternalLoggerFunc, hcomHandle, "ubs_hcom_set_log_handler");
    DL_LOAD_SYM_OPTIONAL(gSetUbsMode, SetUbsMode, hcomHandle, "ubs_hcom_service_set_ubcmode");
    DL_LOAD_SYM_OPTIONAL(gImportUrmaSeg, ImportUrmaSeg, hcomHandle, "ubs_hcom_reg_seg");

    gLoaded = true;
    return BM_OK;
}

void DlHcomApi::CleanupLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (!gLoaded) {
        return;
    }
    gServiceCreate = nullptr;
    gServiceBind = nullptr;
    gServiceStart = nullptr;
    gServiceDestroy = nullptr;
    gServiceConnect = nullptr;
    gServiceDisConnectFunc = nullptr;
    gServiceRegisterMemoryRegion = nullptr;
    gServiceGetMemoryRegionInfo = nullptr;
    gServiceRegisterAssignMemoryRegion = nullptr;
    gServiceDestroyMemoryRegion = nullptr;
    gServiceRegisterChannelBrokerHandler = nullptr;
    gServiceRegisterIdleHandler = nullptr;
    gServiceRegisterHandler = nullptr;
    gServiceAddWorkerGroup = nullptr;
    gServiceAddListener = nullptr;
    gServiceSetConnectLBPolicy = nullptr;
    gServiceSetSecureOptions = nullptr;
    gServiceSetTcpUserTimeOutSec = nullptr;
    gServiceSetTcpSendZCopy = nullptr;
    gServiceSetDeviceIpMask = nullptr;
    gServiceSetDeviceIpGroup = nullptr;
    gServiceSetCompletionQueueDepth = nullptr;
    gServiceSetSendQueueSize = nullptr;
    gServiceSetRecvQueueSize = nullptr;
    gServiceSetQueuePrePostSize = nullptr;
    gServiceSetPollingBatchSize = nullptr;
    gServiceSetEventPollingTimeOutUs = nullptr;
    gServiceSetTimeOutDetectionThreadNum = nullptr;
    gServiceSetMaxConnectionCount = nullptr;
    gServiceSetHeartBeatOptions = nullptr;
    gServiceSetMultiRailOptions = nullptr;
    gChannelSend = nullptr;
    gChannelCall = nullptr;
    gChannelReply = nullptr;
    gChannelPut = nullptr;
    gChannelGet = nullptr;
    gChannelSetFlowControlConfig = nullptr;
    gChannelSetChannelTimeOut = nullptr;
    gContextGetRspCtx = nullptr;
    gContextGetChannel = nullptr;
    gContextGetType = nullptr;
    gContextGetResult = nullptr;
    gContextGetOpCode = nullptr;
    gContextGetMessageData = nullptr;
    gContextGetMessageDataLen = nullptr;
    gSetExternalLogger = nullptr;
    gSetUbsMode = nullptr;
    gImportUrmaSeg = nullptr;

    if (hcomHandle != nullptr) {
        dlclose(hcomHandle);
        hcomHandle = nullptr;
    }

    gLoaded = false;
}