/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "dl_hcom_api.h"
#include <dlfcn.h>

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
serviceGetRspCtxFunc DlHcomApi::gServiceGetRspCtx = nullptr;
serviceGetChannelFunc DlHcomApi::gServiceGetChannel = nullptr;
serviceGetContextTypeFunc DlHcomApi::gServiceGetContextType = nullptr;
serviceGetResultFunc DlHcomApi::gServiceGetResult = nullptr;
serviceGetOpCodeFunc DlHcomApi::gServiceGetOpCode = nullptr;
serviceGetMessageDataFunc DlHcomApi::gServiceGetMessageData = nullptr;
serviceGetMessageDataLenFunc DlHcomApi::gServiceGetMessageDataLen = nullptr;
serviceSetExternalLoggerFunc DlHcomApi::gServiceSetExternalLogger = nullptr;

Result DlHcomApi::LoadLibrary()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gLoaded) {
        return BM_OK;
    }
    hcomHandle = dlopen(hcomLibName, RTLD_NOW);
    if (hcomHandle == nullptr) {
        BM_LOG_WARN("Failed to open library [" << hcomLibName << "], error: " << dlerror());
        return BM_DL_FUNCTION_FAILED;
    }
    DL_LOAD_SYM(gServiceCreate, serviceCreateFunc, hcomHandle, "Service_Create");
    DL_LOAD_SYM(gServiceDestroy, serviceDestroyFunc, hcomHandle, "Service_Destroy");
    DL_LOAD_SYM(gServiceConnect, serviceConnectFunc, hcomHandle, "Service_Connect");
    DL_LOAD_SYM(gServiceDisConnectFunc, serviceDisConnectFunc, hcomHandle, "Service_DisConnect");
    DL_LOAD_SYM(gServiceBind, serviceBindFunc, hcomHandle, "Service_Bind");
    DL_LOAD_SYM(gServiceStart, serviceStartFunc, hcomHandle, "Service_Start");
    DL_LOAD_SYM(gServiceRegisterMemoryRegion, serviceRegisterMemoryRegionFunc, hcomHandle,
        "Service_RegisterMemoryRegion");
    DL_LOAD_SYM(gServiceGetMemoryRegionInfo, serviceGetMemoryRegionInfoFunc, hcomHandle,
        "Service_GetMemoryRegionInfo");
    DL_LOAD_SYM(gServiceRegisterAssignMemoryRegion, serviceRegisterAssignMemoryRegionFunc, hcomHandle,
        "Service_RegisterAssignMemoryRegion");
    DL_LOAD_SYM(gServiceDestroyMemoryRegion, serviceDestroyMemoryRegionFunc, hcomHandle,
        "Service_DestroyMemoryRegion");
    DL_LOAD_SYM(gServiceRegisterChannelBrokerHandler, serviceRegisterChannelBrokerHandlerFunc, hcomHandle,
        "Service_RegisterChannelBrokerHandler");
    DL_LOAD_SYM(gServiceRegisterIdleHandler, serviceRegisterIdleHandlerFunc, hcomHandle,
        "Service_RegisterIdleHandler");
    DL_LOAD_SYM(gServiceRegisterHandler, serviceRegisterHandlerFunc, hcomHandle, "Service_RegisterHandler");
    DL_LOAD_SYM(gServiceAddWorkerGroup, serviceAddWorkerGroupFunc, hcomHandle, "Service_AddWorkerGroup");
    DL_LOAD_SYM(gServiceAddListener, serviceAddListenerFunc, hcomHandle, "Service_AddListener");
    DL_LOAD_SYM(gServiceSetConnectLBPolicy, serviceSetConnectLBPolicyFunc, hcomHandle, "Service_SetConnectLBPolicy");
    DL_LOAD_SYM(gServiceSetTlsOptions, serviceSetTlsOptionsFunc, hcomHandle, "Service_SetTlsOptions");
    DL_LOAD_SYM(gServiceSetSecureOptions, serviceSetSecureOptionsFunc, hcomHandle, "Service_SetSecureOptions");
    DL_LOAD_SYM(gServiceSetTcpUserTimeOutSec, serviceSetTcpUserTimeOutSecFunc, hcomHandle,
        "Service_SetTcpUserTimeOutSec");
    DL_LOAD_SYM(gServiceSetTcpSendZCopy, serviceSetTcpSendZCopyFunc, hcomHandle, "Service_SetTcpSendZCopy");
    DL_LOAD_SYM(gServiceSetDeviceIpMask, serviceSetDeviceIpMaskFunc, hcomHandle, "Service_SetDeviceIpMask");
    DL_LOAD_SYM(gServiceSetDeviceIpGroup, serviceSetDeviceIpGroupFunc, hcomHandle, "Service_SetDeviceIpGroup");
    DL_LOAD_SYM(gServiceSetCompletionQueueDepth, serviceSetCompletionQueueDepthFunc, hcomHandle,
        "Service_SetCompletionQueueDepth");
    DL_LOAD_SYM(gServiceSetSendQueueSize, serviceSetSendQueueSizeFunc, hcomHandle, "Service_SetSendQueueSize");
    DL_LOAD_SYM(gServiceSetRecvQueueSize, serviceSetRecvQueueSizeFunc, hcomHandle, "Service_SetRecvQueueSize");
    DL_LOAD_SYM(gServiceSetQueuePrePostSize, serviceSetQueuePrePostSizeFunc, hcomHandle,
        "Service_SetQueuePrePostSize");
    DL_LOAD_SYM(gServiceSetPollingBatchSize, serviceSetPollingBatchSizeFunc, hcomHandle,
        "Service_SetPollingBatchSize");
    DL_LOAD_SYM(gServiceSetEventPollingTimeOutUs, serviceSetEventPollingTimeOutUsFunc, hcomHandle,
        "Service_SetEventPollingTimeOutUs");
    DL_LOAD_SYM(gServiceSetTimeOutDetectionThreadNum, serviceSetTimeOutDetectionThreadNumFunc, hcomHandle,
        "Service_SetTimeOutDetectionThreadNum");
    DL_LOAD_SYM(gServiceSetMaxConnectionCount, serviceSetMaxConnectionCountFunc, hcomHandle,
        "Service_SetMaxConnectionCount");
    DL_LOAD_SYM(gServiceSetHeartBeatOptions, serviceSetHeartBeatOptionsFunc, hcomHandle, "Service_SetHeartBeatOptions");
    DL_LOAD_SYM(gServiceSetMultiRailOptions, serviceSetMultiRailOptionsFunc, hcomHandle, "Service_SetMultiRailOptions");
    DL_LOAD_SYM(gChannelSend, channelSendFunc, hcomHandle, "Channel_Send");
    DL_LOAD_SYM(gChannelCall, channelCallFunc, hcomHandle, "Channel_Call");
    DL_LOAD_SYM(gChannelReply, channelReplyFunc, hcomHandle, "Channel_Reply");
    DL_LOAD_SYM(gChannelPut, channelPutFunc, hcomHandle, "Channel_Put");
    DL_LOAD_SYM(gChannelGet, channelGetFunc, hcomHandle, "Channel_Get");
    DL_LOAD_SYM(gChannelSetFlowControlConfig, channelSetFlowControlConfigFunc, hcomHandle,
        "Channel_SetFlowControlConfig");
    DL_LOAD_SYM(gChannelSetChannelTimeOut, channelSetChannelTimeOutFunc, hcomHandle, "Channel_SetChannelTimeOut");
    DL_LOAD_SYM(gServiceGetRspCtx, serviceGetRspCtxFunc, hcomHandle, "Service_GetRspCtx");
    DL_LOAD_SYM(gServiceGetChannel, serviceGetChannelFunc, hcomHandle, "Service_GetChannel");
    DL_LOAD_SYM(gServiceGetContextType, serviceGetContextTypeFunc, hcomHandle, "Service_GetContextType");
    DL_LOAD_SYM(gServiceGetResult, serviceGetResultFunc, hcomHandle, "Service_GetResult");
    DL_LOAD_SYM(gServiceGetOpCode, serviceGetOpCodeFunc, hcomHandle, "Service_GetOpCode");
    DL_LOAD_SYM(gServiceGetMessageData, serviceGetMessageDataFunc, hcomHandle, "Service_GetMessageData");
    DL_LOAD_SYM(gServiceGetMessageDataLen, serviceGetMessageDataLenFunc, hcomHandle, "Service_GetMessageDataLen");
    DL_LOAD_SYM(gServiceSetExternalLogger, serviceSetExternalLoggerFunc, hcomHandle, "Service_SetExternalLogger");

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
    gServiceGetRspCtx = nullptr;
    gServiceGetChannel = nullptr;
    gServiceGetContextType = nullptr;
    gServiceGetResult = nullptr;
    gServiceGetOpCode = nullptr;
    gServiceGetMessageData = nullptr;
    gServiceGetMessageDataLen = nullptr;
    gServiceSetExternalLogger = nullptr;

    if (hcomHandle != nullptr) {
        dlclose(hcomHandle);
        hcomHandle = nullptr;
    }

    gLoaded = false;
}