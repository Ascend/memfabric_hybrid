/*
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <cstdint>

#include "hcom_c_define.h"
#include "hcom_service_c_define.h"

extern "C" {
uint32_t Service_Create(Service_Type t, const char *name, Service_Options options, Hcom_Service *service)
{
    *service = static_cast<Hcom_Service>(0x9);
    return 0;
}

uint32_t Service_Destroy(Hcom_Service service, const char *name)
{
    return 0;
}

uint32_t Service_Connect(Hcom_Service service, const char *serverUrl, Hcom_Channel *channel,
                         Service_ConnectOptions options)
{
    *channel = (Hcom_Channel)0x10000;
    return 0;
}

uint32_t Service_DisConnect(Hcom_Service service, Hcom_Channel channel)
{
    return 0;
}

uint32_t Service_Bind(Hcom_Service service, const char *listenerUrl, Service_ChannelHandler h)
{
    return 0;
}

uint32_t Service_Start(Hcom_Service service)
{
    return 0;
}

uint32_t Service_RegisterMemoryRegion(Hcom_Service service, uint64_t size, Service_MemoryRegion *mr)
{
    return 0;
}

int Service_GetMemoryRegionInfo(Service_MemoryRegion mr, Service_MemoryRegionInfo *info)
{
    return 0;
}

int Service_RegisterAssignMemoryRegion(Hcom_Service service, uintptr_t address, uint64_t size, Service_MemoryRegion *mr)
{
    return 0;
}

int Service_DestroyMemoryRegion(Hcom_Service service, Service_MemoryRegion mr)
{
    return 0;
}

// 返回 void 的函数，直接返回
void Service_RegisterChannelBrokerHandler(Hcom_Service service, Service_ChannelHandler h,
                                          Service_ChannelPolicy policy, uint64_t usrCtx)
{
    return;
}

void Service_RegisterIdleHandler(Hcom_Service service, Service_IdleHandler h, uint64_t usrCtx)
{
    return;
}

void Service_RegisterHandler(Hcom_Service service, Service_HandlerType t, Service_RequestHandler h, uint64_t usrCtx)
{
    return;
}

void Service_AddWorkerGroup(Hcom_Service service, int8_t priority, uint16_t workerGroupId, uint32_t threadCount,
                            const char *cpuIdsRange)
{
    return;
}

void Service_AddListener(Hcom_Service service, const char *url, uint16_t workerCount)
{
    return;
}

void Service_SetConnectLBPolicy(Hcom_Service service, Service_LBPolicy lbPolicy)
{
    return;
}

void Service_SetTlsOptions(Hcom_Service service, bool enableTls, Service_TlsMode mode,
                           Service_TlsVersion version, Service_CipherSuite cipherSuite,
                           Hcom_TlsGetCertCb certCb, Hcom_TlsGetPrivateKeyCb priKeyCb, Hcom_TlsGetCACb caCb)
{
    return;
}

void Service_SetSecureOptions(Hcom_Service service, Service_SecType secType, Hcom_SecInfoProvider provider,
                              Hcom_SecInfoValidator validator, uint16_t magic, uint8_t version)
{
    return;
}

void Service_SetTcpUserTimeOutSec(Hcom_Service service, uint16_t timeOutSec)
{
    return;
}

void Service_SetTcpSendZCopy(Hcom_Service service, bool tcpSendZCopy)
{
    return;
}

void Service_SetDeviceIpMask(Hcom_Service service, const char *ipMask)
{
    return;
}

void Service_SetDeviceIpGroup(Hcom_Service service, const char *ipGroup)
{
    return;
}

void Service_SetCompletionQueueDepth(Hcom_Service service, uint16_t depth)
{
    return;
}

void Service_SetSendQueueSize(Hcom_Service service, uint32_t sqSize)
{
    return;
}

void Service_SetRecvQueueSize(Hcom_Service service, uint32_t rqSize)
{
    return;
}

void Service_SetQueuePrePostSize(Hcom_Service service, uint32_t prePostSize)
{
    return;
}

void Service_SetPollingBatchSize(Hcom_Service service, uint16_t pollSize)
{
    return;
}

void Service_SetEventPollingTimeOutUs(Hcom_Service service, uint16_t pollTimeout)
{
    return;
}

void Service_SetTimeOutDetectionThreadNum(Hcom_Service service, uint32_t threadNum)
{
    return;
}

void Service_SetMaxConnectionCount(Hcom_Service service, uint32_t maxConnCount)
{
    return;
}

void Service_SetHeartBeatOptions(Hcom_Service service, uint16_t idleSec, uint16_t probeTimes, uint16_t intervalSec)
{
    return;
}

void Service_SetMultiRailOptions(Hcom_Service service, bool enable, uint32_t threshold)
{
    return;
}

void Service_SetExternalLogger(Service_LogHandler h)
{
    return;
}

int Channel_Send(Hcom_Channel channel, Channel_Request req, Channel_Callback *cb)
{
    return 0;
}

int Channel_Call(Hcom_Channel channel, Channel_Request req, Channel_Response *rsp, Channel_Callback *cb)
{
    return 0;
}

int Channel_Reply(Hcom_Channel channel, Channel_Request req, Channel_ReplyContext ctx, Channel_Callback *cb)
{
    return 0;
}

int Channel_Put(Hcom_Channel channel, Channel_OneSideRequest req, Channel_Callback *cb)
{
    return 0;
}

int Channel_Get(Hcom_Channel channel, Channel_OneSideRequest req, Channel_Callback *cb)
{
    return 0;
}

int Channel_SetFlowControlConfig(Hcom_Channel channel, Channel_FlowCtrlOptions opt)
{
    return 0;
}

void Channel_SetChannelTimeOut(Hcom_Channel channel, int16_t oneSideTimeout, int16_t twoSideTimeout)
{
    return;
}

// === Service 获取函数（返回 int）===
int Service_GetRspCtx(Service_Context context, Channel_ReplyContext *rspCtx)
{
    return 0;
}

int Service_GetChannel(Service_Context context, Hcom_Channel *channel)
{
    return 0;
}

int Service_GetContextType(Service_Context context, Service_ContextType *type)
{
    return 0;
}

int Service_GetResult(Service_Context context, int *result)
{
    return 0;
}

uint16_t Service_GetOpCode(Service_Context context)
{
    return 0;
}

void* Service_GetMessageData(Service_Context context)
{
    return nullptr;
}

uint32_t Service_GetMessageDataLen(Service_Context context)
{
    return 0;
}
}