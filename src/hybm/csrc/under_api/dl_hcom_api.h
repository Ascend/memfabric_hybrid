/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MF_HYBRID_DLHCOMAPI_H
#define MF_HYBRID_DLHCOMAPI_H

#include "hybm_common_include.h"
#include "hcom_service_c_define.h"

namespace ock {
namespace mf {

using serviceCreateFunc = int (*)(Service_Type, const char *, Service_Options, Hcom_Service *);
using serviceBindFunc = int (*)(Hcom_Service, const char *, Service_ChannelHandler);
using serviceStartFunc = int (*)(Hcom_Service);
using serviceDestroyFunc = int (*)(Hcom_Service, const char *);
using serviceConnectFunc = int (*)(Hcom_Service, const char *, Hcom_Channel *, Service_ConnectOptions);
using serviceDisConnectFunc = int (*)(Hcom_Service, Hcom_Channel);
using serviceRegisterMemoryRegionFunc = int (*)(Hcom_Service, uint64_t, Service_MemoryRegion *);
using serviceGetMemoryRegionInfoFunc = int (*)(Service_MemoryRegion, Service_MemoryRegionInfo *);
using serviceRegisterAssignMemoryRegionFunc = int (*)(Hcom_Service, uintptr_t,
                                                      uint64_t, Service_MemoryRegion *);
using serviceDestroyMemoryRegionFunc = int (*)(Hcom_Service, Service_MemoryRegion);
using serviceRegisterChannelBrokerHandlerFunc = void (*)(Hcom_Service, Service_ChannelHandler,
                                                         Service_ChannelPolicy, uint64_t);
using serviceRegisterIdleHandlerFunc = void (*)(Hcom_Service, Service_IdleHandler, uint64_t);
using serviceRegisterHandlerFunc = void (*)(Hcom_Service, Service_HandlerType, Service_RequestHandler,
                                            uint64_t);
using serviceAddWorkerGroupFunc = void (*)(Hcom_Service, int8_t, uint16_t, uint32_t, const char *);
using serviceAddListenerFunc = void (*)(Hcom_Service, const char *, uint16_t);
using serviceSetConnectLBPolicyFunc = void (*)(Hcom_Service, Service_LBPolicy);
using serviceSetSecureOptionsFunc = void (*)(Hcom_Service, Service_SecType, Hcom_SecInfoProvider,
                                             Hcom_SecInfoValidator, uint16_t, uint8_t);
using serviceSetTcpUserTimeOutSecFunc = void (*)(Hcom_Service, uint16_t);
using serviceSetTcpSendZCopyFunc = void (*)(Hcom_Service, bool);
using serviceSetDeviceIpMaskFunc = void (*)(Hcom_Service, const char *);
using serviceSetDeviceIpGroupFunc = void (*)(Hcom_Service, const char *);
using serviceSetCompletionQueueDepthFunc = void (*)(Hcom_Service, uint16_t);
using serviceSetSendQueueSizeFunc = void (*)(Hcom_Service, uint32_t);
using serviceSetRecvQueueSizeFunc = void (*)(Hcom_Service, uint32_t);
using serviceSetQueuePrePostSizeFunc = void (*)(Hcom_Service, uint32_t);
using serviceSetPollingBatchSizeFunc = void (*)(Hcom_Service, uint16_t);
using serviceSetEventPollingTimeOutUsFunc = void (*)(Hcom_Service, uint16_t);
using serviceSetTimeOutDetectionThreadNumFunc = void (*)(Hcom_Service, uint32_t);
using serviceSetMaxConnectionCountFunc = void (*)(Hcom_Service, uint32_t);
using serviceSetHeartBeatOptionsFunc = void (*)(Hcom_Service, uint16_t, uint16_t, uint16_t);
using serviceSetMultiRailOptionsFunc = void (*)(Hcom_Service, bool, uint32_t);
using channelSendFunc = int (*)(Hcom_Channel, Channel_Request, Channel_Callback *);
using channelCallFunc = int (*)(Hcom_Channel, Channel_Request, Channel_Response *, Channel_Callback *);
using channelReplyFunc = int (*)(Hcom_Channel, Channel_Request, Channel_ReplyContext, Channel_Callback *);
using channelPutFunc = int (*)(Hcom_Channel, Channel_OneSideRequest, Channel_Callback *);
using channelGetFunc = int (*)(Hcom_Channel, Channel_OneSideRequest, Channel_Callback *);
using channelSetFlowControlConfigFunc = int (*)(Hcom_Channel, Channel_FlowCtrlOptions);
using channelSetChannelTimeOutFunc = void (*)(Hcom_Channel, int16_t, int16_t);
using serviceGetRspCtxFunc = int (*)(Service_Context, Channel_ReplyContext *);
using serviceGetChannelFunc = int (*)(Service_Context, Hcom_Channel *);
using serviceGetContextTypeFunc = int (*)(Service_Context, Service_ContextType *);
using serviceGetResultFunc = int (*)(Service_Context, int *);
using serviceGetOpCodeFunc = uint64_t (*)(Service_Context);
using serviceGetMessageDataFunc = void *(*)(Service_Context);
using serviceGetMessageDataLenFunc = uint32_t (*)(Service_Context);
using serviceSetExternalLoggerFunc = void (*)(Service_LogHandler);

class DlHcomApi {
public:
    static Result LoadLibrary();
    static void CleanupLibrary();

    static inline int
    ServiceCreate(Service_Type t, const char *name, Service_Options options, Hcom_Service *service)
    {
        BM_ASSERT_RETURN(gServiceCreate != nullptr, BM_NOT_INITIALIZED);
        return gServiceCreate(t, name, options, service);
    }

    static inline int ServiceBind(Hcom_Service service, const char *listenerUrl, Service_ChannelHandler h)
    {
        BM_ASSERT_RETURN(gServiceBind != nullptr, BM_NOT_INITIALIZED);
        return gServiceBind(service, listenerUrl, h);
    }

    static inline int ServiceStart(Hcom_Service service)
    {
        BM_ASSERT_RETURN(gServiceStart != nullptr, BM_NOT_INITIALIZED);
        return gServiceStart(service);
    }

    static inline int ServiceDestroy(Hcom_Service service, const char *name)
    {
        BM_ASSERT_RETURN(gServiceDestroy != nullptr, BM_NOT_INITIALIZED);
        return gServiceDestroy(service, name);
    }

    static inline int ServiceConnect(Hcom_Service service, const char *serverUrl, Hcom_Channel *channel,
                                      Service_ConnectOptions options)
    {
        BM_ASSERT_RETURN(gServiceConnect != nullptr, BM_NOT_INITIALIZED);
        return gServiceConnect(service, serverUrl, channel, options);
    }

    static inline int ServiceDisConnect(Hcom_Service service, Hcom_Channel channel)
    {
        BM_ASSERT_RETURN(gServiceDisConnectFunc != nullptr, BM_NOT_INITIALIZED);
        return gServiceDisConnectFunc(service, channel);
    }

    static inline int
    ServiceRegisterMemoryRegion(Hcom_Service service, uint64_t size, Service_MemoryRegion *mr)
    {
        BM_ASSERT_RETURN(gServiceRegisterMemoryRegion != nullptr, BM_NOT_INITIALIZED);
        return gServiceRegisterMemoryRegion(service, size, mr);
    }

    static inline int ServiceGetMemoryRegionInfo(Service_MemoryRegion mr, Service_MemoryRegionInfo *info)
    {
        BM_ASSERT_RETURN(gServiceGetMemoryRegionInfo != nullptr, BM_NOT_INITIALIZED);
        return gServiceGetMemoryRegionInfo(mr, info);
    }

    static inline int ServiceRegisterAssignMemoryRegion(
            Hcom_Service service, uintptr_t address, uint64_t size, Service_MemoryRegion *mr)
    {
        BM_ASSERT_RETURN(gServiceRegisterAssignMemoryRegion != nullptr, BM_NOT_INITIALIZED);
        return gServiceRegisterAssignMemoryRegion(service, address, size, mr);
    }

    static inline int ServiceDestroyMemoryRegion(Hcom_Service service, Service_MemoryRegion mr)
    {
        BM_ASSERT_RETURN(gServiceDestroyMemoryRegion != nullptr, BM_NOT_INITIALIZED);
        return gServiceDestroyMemoryRegion(service, mr);
    }

    static inline void ServiceRegisterChannelBrokerHandler(Hcom_Service service, Service_ChannelHandler h,
                                                            Service_ChannelPolicy policy, uint64_t usrCtx)
    {
        BM_ASSERT_RET_VOID(gServiceRegisterChannelBrokerHandler != nullptr);
        gServiceRegisterChannelBrokerHandler(service, h, policy, usrCtx);
    }

    static inline void ServiceRegisterIdleHandler(Hcom_Service service, Service_IdleHandler h, uint64_t usrCtx)
    {
        BM_ASSERT_RET_VOID(gServiceRegisterIdleHandler != nullptr);
        gServiceRegisterIdleHandler(service, h, usrCtx);
    }

    static inline void
    ServiceRegisterHandler(Hcom_Service service, Service_HandlerType t, Service_RequestHandler h,
                            uint64_t usrCtx)
    {
        BM_ASSERT_RET_VOID(gServiceRegisterHandler != nullptr);
        gServiceRegisterHandler(service, t, h, usrCtx);
    }

    static inline void
    ServiceAddWorkerGroup(Hcom_Service service, int8_t priority, uint16_t workerGroupId, uint32_t threadCount,
                           const char *cpuIdsRange)
    {
        BM_ASSERT_RET_VOID(gServiceAddWorkerGroup != nullptr);
        gServiceAddWorkerGroup(service, priority, workerGroupId, threadCount, cpuIdsRange);
    }

    static inline void ServiceAddListener(Hcom_Service service, const char *url, uint16_t workerCount)
    {
        BM_ASSERT_RET_VOID(gServiceAddListener != nullptr);
        gServiceAddListener(service, url, workerCount);
    }

    static inline void ServiceSetConnectLBPolicy(Hcom_Service service, Service_LBPolicy lbPolicy)
    {
        BM_ASSERT_RET_VOID(gServiceSetConnectLBPolicy != nullptr);
        gServiceSetConnectLBPolicy(service, lbPolicy);
    }

    static inline void
    ServiceSetSecureOptions(Hcom_Service service, Service_SecType secType, Hcom_SecInfoProvider provider,
                             Hcom_SecInfoValidator validator, uint16_t magic, uint8_t version)
    {
        BM_ASSERT_RET_VOID(gServiceSetSecureOptions != nullptr);
        gServiceSetSecureOptions(service, secType, provider, validator, magic, version);
    }

    static inline void ServiceSetTcpUserTimeOutSec(Hcom_Service service, uint16_t timeOutSec)
    {
        BM_ASSERT_RET_VOID(gServiceSetTcpUserTimeOutSec != nullptr);
        gServiceSetTcpUserTimeOutSec(service, timeOutSec);
    }

    static inline void ServiceSetTcpSendZCopy(Hcom_Service service, bool tcpSendZCopy)
    {
        BM_ASSERT_RET_VOID(gServiceSetTcpSendZCopy != nullptr);
        gServiceSetTcpSendZCopy(service, tcpSendZCopy);
    }

    static inline void ServiceSetDeviceIpMask(Hcom_Service service, const char *ipMask)
    {
        BM_ASSERT_RET_VOID(gServiceSetDeviceIpMask != nullptr);
        gServiceSetDeviceIpMask(service, ipMask);
    }

    static inline void ServiceSetDeviceIpGroup(Hcom_Service service, const char *ipGroup)
    {
        BM_ASSERT_RET_VOID(gServiceSetDeviceIpGroup != nullptr);
        gServiceSetDeviceIpGroup(service, ipGroup);
    }

    static inline void ServiceSetCompletionQueueDepth(Hcom_Service service, uint16_t depth)
    {
        BM_ASSERT_RET_VOID(gServiceSetCompletionQueueDepth != nullptr);
        gServiceSetCompletionQueueDepth(service, depth);
    }

    static inline void ServiceSetSendQueueSize(Hcom_Service service, uint32_t sqSize)
    {
        BM_ASSERT_RET_VOID(gServiceSetSendQueueSize != nullptr);
        gServiceSetSendQueueSize(service, sqSize);
    }

    static inline void ServiceSetRecvQueueSize(Hcom_Service service, uint32_t rqSize)
    {
        BM_ASSERT_RET_VOID(gServiceSetRecvQueueSize != nullptr);
        gServiceSetRecvQueueSize(service, rqSize);
    }

    static inline void ServiceSetQueuePrePostSize(Hcom_Service service, uint32_t prePostSize)
    {
        BM_ASSERT_RET_VOID(gServiceSetQueuePrePostSize != nullptr);
        gServiceSetQueuePrePostSize(service, prePostSize);
    }

    static inline void ServiceSetPollingBatchSize(Hcom_Service service, uint16_t pollSize)
    {
        BM_ASSERT_RET_VOID(gServiceSetPollingBatchSize != nullptr);
        gServiceSetPollingBatchSize(service, pollSize);
    }

    static inline void ServiceSetEventPollingTimeOutUs(Hcom_Service service, uint16_t pollTimeout)
    {
        BM_ASSERT_RET_VOID(gServiceSetEventPollingTimeOutUs != nullptr);
        gServiceSetEventPollingTimeOutUs(service, pollTimeout);
    }

    static inline void ServiceSetTimeOutDetectionThreadNum(Hcom_Service service, uint32_t threadNum)
    {
        BM_ASSERT_RET_VOID(gServiceSetTimeOutDetectionThreadNum != nullptr);
        gServiceSetTimeOutDetectionThreadNum(service, threadNum);
    }

    static inline void ServiceSetMaxConnectionCount(Hcom_Service service, uint32_t maxConnCount)
    {
        BM_ASSERT_RET_VOID(gServiceSetMaxConnectionCount != nullptr);
        gServiceSetMaxConnectionCount(service, maxConnCount);
    }

    static inline void ServiceSetHeartBeatOptions(Hcom_Service service, uint16_t idleSec, uint16_t probeTimes,
                                                   uint16_t intervalSec)
    {
        BM_ASSERT_RET_VOID(gServiceSetHeartBeatOptions != nullptr);
        gServiceSetHeartBeatOptions(service, idleSec, probeTimes, intervalSec);
    }

    static inline void ServiceSetMultiRailOptions(Hcom_Service service, bool enable, uint32_t threshold)
    {
        BM_ASSERT_RET_VOID(gServiceSetMultiRailOptions != nullptr);
        gServiceSetMultiRailOptions(service, enable, threshold);
    }

    static inline int ChannelSend(Hcom_Channel channel, Channel_Request req, Channel_Callback *cb)
    {
        BM_ASSERT_RETURN(gChannelSend != nullptr, BM_NOT_INITIALIZED);
        return gChannelSend(channel, req, cb);
    }

    static inline int
    ChannelCall(Hcom_Channel channel, Channel_Request req, Channel_Response *rsp, Channel_Callback *cb)
    {
        BM_ASSERT_RETURN(gChannelCall != nullptr, BM_NOT_INITIALIZED);
        return gChannelCall(channel, req, rsp, cb);
    }

    static inline int
    ChannelReply(Hcom_Channel channel, Channel_Request req, Channel_ReplyContext ctx, Channel_Callback *cb)
    {
        BM_ASSERT_RETURN(gChannelReply != nullptr, BM_NOT_INITIALIZED);
        return gChannelReply(channel, req, ctx, cb);
    }

    static inline int ChannelPut(Hcom_Channel channel, Channel_OneSideRequest req, Channel_Callback *cb)
    {
        BM_ASSERT_RETURN(gChannelPut != nullptr, BM_NOT_INITIALIZED);
        return gChannelPut(channel, req, cb);
    }

    static inline int ChannelGet(Hcom_Channel channel, Channel_OneSideRequest req, Channel_Callback *cb)
    {
        BM_ASSERT_RETURN(gChannelGet != nullptr, BM_NOT_INITIALIZED);
        return gChannelGet(channel, req, cb);
    }

    static inline int ChannelSetFlowControlConfig(Hcom_Channel channel, Channel_FlowCtrlOptions opt)
    {
        BM_ASSERT_RETURN(gChannelSetFlowControlConfig != nullptr, BM_NOT_INITIALIZED);
        return gChannelSetFlowControlConfig(channel, opt);
    }

    static inline void
    ChannelSetChannelTimeOut(Hcom_Channel channel, int16_t oneSideTimeout, int16_t twoSideTimeout)
    {
        BM_ASSERT_RET_VOID(gChannelSetChannelTimeOut != nullptr);
        gChannelSetChannelTimeOut(channel, oneSideTimeout, twoSideTimeout);
    }

    static inline int ServiceGetRspCtx(Service_Context context, Channel_ReplyContext *rspCtx)
    {
        BM_ASSERT_RETURN(gServiceGetRspCtx != nullptr, BM_NOT_INITIALIZED);
        return gServiceGetRspCtx(context, rspCtx);
    }

    static inline int ServiceGetChannel(Service_Context context, Hcom_Channel *channel)
    {
        BM_ASSERT_RETURN(gServiceGetChannel != nullptr, BM_NOT_INITIALIZED);
        return gServiceGetChannel(context, channel);
    }

    static inline int ServiceGetContextType(Service_Context context, Service_ContextType *type)
    {
        BM_ASSERT_RETURN(gServiceGetContextType != nullptr, BM_NOT_INITIALIZED);
        return gServiceGetContextType(context, type);
    }

    static inline int ServiceGetResult(Service_Context context, int *result)
    {
        BM_ASSERT_RETURN(gServiceGetResult != nullptr, BM_NOT_INITIALIZED);
        return gServiceGetResult(context, result);
    }

    static inline uint16_t ServiceGetOpCode(Service_Context context)
    {
        BM_ASSERT_RETURN(gServiceGetOpCode != nullptr, BM_NOT_INITIALIZED);
        return gServiceGetOpCode(context);
    }

    static inline void *ServiceGetMessageData(Service_Context context)
    {
        BM_ASSERT_RETURN(gServiceGetMessageData != nullptr, nullptr);
        return gServiceGetMessageData(context);
    }

    static inline uint32_t ServiceGetMessageDataLen(Service_Context context)
    {
        BM_ASSERT_RETURN(gServiceGetMessageDataLen != nullptr, BM_NOT_INITIALIZED);
        return gServiceGetMessageDataLen(context);
    }

    static inline void ServiceSetExternalLogger(Service_LogHandler h)
    {
        BM_ASSERT_RET_VOID(gServiceSetExternalLogger != nullptr);
        gServiceSetExternalLogger(h);
    }

private:
    static std::mutex gMutex;
    static bool gLoaded;
    static void *hcomHandle;
    static const char *hcomLibName;

    static serviceCreateFunc gServiceCreate;
    static serviceBindFunc gServiceBind;
    static serviceStartFunc gServiceStart;
    static serviceDestroyFunc gServiceDestroy;
    static serviceConnectFunc gServiceConnect;
    static serviceDisConnectFunc gServiceDisConnectFunc;
    static serviceRegisterMemoryRegionFunc gServiceRegisterMemoryRegion;
    static serviceGetMemoryRegionInfoFunc gServiceGetMemoryRegionInfo;
    static serviceRegisterAssignMemoryRegionFunc gServiceRegisterAssignMemoryRegion;
    static serviceDestroyMemoryRegionFunc gServiceDestroyMemoryRegion;
    static serviceRegisterChannelBrokerHandlerFunc gServiceRegisterChannelBrokerHandler;
    static serviceRegisterIdleHandlerFunc gServiceRegisterIdleHandler;
    static serviceRegisterHandlerFunc gServiceRegisterHandler;
    static serviceAddWorkerGroupFunc gServiceAddWorkerGroup;
    static serviceAddListenerFunc gServiceAddListener;
    static serviceSetConnectLBPolicyFunc gServiceSetConnectLBPolicy;
    static serviceSetSecureOptionsFunc gServiceSetSecureOptions;
    static serviceSetTcpUserTimeOutSecFunc gServiceSetTcpUserTimeOutSec;
    static serviceSetTcpSendZCopyFunc gServiceSetTcpSendZCopy;
    static serviceSetDeviceIpMaskFunc gServiceSetDeviceIpMask;
    static serviceSetDeviceIpGroupFunc gServiceSetDeviceIpGroup;
    static serviceSetCompletionQueueDepthFunc gServiceSetCompletionQueueDepth;
    static serviceSetSendQueueSizeFunc gServiceSetSendQueueSize;
    static serviceSetRecvQueueSizeFunc gServiceSetRecvQueueSize;
    static serviceSetQueuePrePostSizeFunc gServiceSetQueuePrePostSize;
    static serviceSetPollingBatchSizeFunc gServiceSetPollingBatchSize;
    static serviceSetEventPollingTimeOutUsFunc gServiceSetEventPollingTimeOutUs;
    static serviceSetTimeOutDetectionThreadNumFunc gServiceSetTimeOutDetectionThreadNum;
    static serviceSetMaxConnectionCountFunc gServiceSetMaxConnectionCount;
    static serviceSetHeartBeatOptionsFunc gServiceSetHeartBeatOptions;
    static serviceSetMultiRailOptionsFunc gServiceSetMultiRailOptions;
    static channelSendFunc gChannelSend;
    static channelCallFunc gChannelCall;
    static channelReplyFunc gChannelReply;
    static channelPutFunc gChannelPut;
    static channelGetFunc gChannelGet;
    static channelSetFlowControlConfigFunc gChannelSetFlowControlConfig;
    static channelSetChannelTimeOutFunc gChannelSetChannelTimeOut;
    static serviceGetRspCtxFunc gServiceGetRspCtx;
    static serviceGetChannelFunc gServiceGetChannel;
    static serviceGetContextTypeFunc gServiceGetContextType;
    static serviceGetResultFunc gServiceGetResult;
    static serviceGetOpCodeFunc gServiceGetOpCode;
    static serviceGetMessageDataFunc gServiceGetMessageData;
    static serviceGetMessageDataLenFunc gServiceGetMessageDataLen;
    static serviceSetExternalLoggerFunc gServiceSetExternalLogger;
};
}
}
#endif // MF_HYBRID_DLHCOMAPI_H
