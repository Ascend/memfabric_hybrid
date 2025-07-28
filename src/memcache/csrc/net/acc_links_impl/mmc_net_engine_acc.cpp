/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "acc_def.h"
#include "acc_tcp_server.h"

#include "mmc_net_link_acc.h"
#include "mmc_net_engine_acc.h"
#include "mmc_net_ctx_store.h"
#include "mmc_msg_base.h"
#include "mmc_net_wait_handle.h"
#include "mmc_net_common_acc.h"
#include "mmc_net_ctx_acc.h"

namespace ock {
namespace mmc {
constexpr const int16_t NET_SERVER_MAGIC = 3867;

Result NetEngineAcc::Start(const NetEngineOptions &options)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (started_) {
        MMC_LOG_INFO("NetEngineAcc " << options.name << " already started");
        return MMC_OK;
    }

    /* verify */
    MMC_RETURN_ERROR(VerifyOptions(options), "NetEngineAcc " << options.name << " option set error");

    /* initialize */
    MMC_RETURN_ERROR(Initialize(options), "NetEngineAcc " << options.name << " initialize error");

    /* check handler */
    if (options_.startListener && newLinkHandler_ == nullptr) {
        MMC_LOG_ERROR("No new link handler function registered, call 'RegisterNewLinkHandler' to register");
        return MMC_INVALID_PARAM;
    } else if (!options_.startListener && newLinkHandler_ != nullptr) {
        MMC_LOG_ERROR("No need to register new link handler function for the engine without listener");
        return MMC_INVALID_PARAM;
    }

    /* call start inner */
    MMC_RETURN_ERROR(StartInner(), "NetEngineAcc " << options.name << " start error");

    started_ = true;
    return MMC_OK;
}

void NetEngineAcc::Stop()
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!started_) {
        MMC_LOG_WARN("NetEngineAcc has not been started");
        return;
    }

    MMC_ASSERT(StopInner() == MMC_OK);

    UnInitialize();

    started_ = false;
}

Result NetEngineAcc::VerifyOptions(const NetEngineOptions &options)
{
    if (options.rankId == UINT16_MAX) {
        MMC_LOG_ERROR("verify NetEngineOptions failed, invalid rank id " << options.rankId);
        return MMC_INVALID_PARAM;
    } else if (options.name.empty()) {
        MMC_LOG_ERROR("verify NetEngineOptions failed, empty name");
        return MMC_INVALID_PARAM;
    }

    if (options.startListener) {
        if (options.ip.empty()) {
            MMC_LOG_ERROR("verify NetEngineOptions failed, ip is empty");
            return MMC_INVALID_PARAM;
        } else if (options.port <= N1024) {
            MMC_LOG_ERROR("verify NetEngineOptions failed, invalid port " << options.port);
            return MMC_INVALID_PARAM;
        }
    }

    if (options.threadCount > N1024 || options.threadCount == 0) {
        MMC_LOG_ERROR("verify NetEngineOptions failed, threadCount is too large, which should between 1~" << N1024);
        return MMC_INVALID_PARAM;
    }

    return MMC_OK;
}

Result NetEngineAcc::StartInner()
{
    /* construct acc tcp server */
    TcpServerOptions serverOptions;
    serverOptions.version = static_cast<int16_t>(NetProtoVersion::VERSION_1);
    serverOptions.magic = NET_SERVER_MAGIC;
    if (options_.startListener) {
        serverOptions.enableListener = true;
        serverOptions.listenIp = options_.ip;
        serverOptions.listenPort = options_.port;
    }
    serverOptions.workerCount = options_.threadCount;
    serverOptions.linkSendQueueSize = UN32;

    TcpTlsOption tlsOpt;
    tlsOpt.enableTls = options_.tlsOption.tlsEnable;
    tlsOpt.tlsTopPath = options_.tlsOption.tlsTopPath;
    tlsOpt.tlsCaPath = "/";
    tlsOpt.tlsCaFile.insert(options_.tlsOption.tlsCaPath);
    tlsOpt.tlsCert = options_.tlsOption.tlsCertPath;
    tlsOpt.tlsPk = options_.tlsOption.tlsKeyPath;

    if (tlsOpt.enableTls) {
        MMC_RETURN_ERROR(server_->LoadDynamicLib(options_.tlsOption.packagePath),
            "Failed to load openssl dynamic library");
    }

    /* start server, listen and thread will be started */
    MMC_RETURN_ERROR(server_->Start(serverOptions, tlsOpt),
                     "Failed to start tcp server for NetEngine " << options_.name);

    return MMC_OK;
}

Result NetEngineAcc::StopInner()
{
    /* stop server */
    if (server_ != nullptr) {
        server_->Stop();
        server_ = nullptr;
    }

    return MMC_OK;
}

Result NetEngineAcc::Call(uint32_t targetId, int16_t opCode, const char *reqData, uint32_t reqDataLen, char **respData,
                          uint32_t &respDataLen, int32_t timeoutInSecond)
{
    MMC_ASSERT_RETURN(started_, MMC_NOT_STARTED);
    MMC_ASSERT_RETURN(reqData != nullptr, MMC_INVALID_PARAM);
    MMC_ASSERT_RETURN(reqDataLen != 0, MMC_INVALID_PARAM);
    MMC_ASSERT_RETURN(respData != nullptr, MMC_INVALID_PARAM);

    // TODO
    MMC_ASSERT_RETURN(opCode != -1, MMC_INVALID_PARAM);

    /* step1: do serialization */
//    auto result = request->Serialize();
//    KVS_ASSERT_RETURN(result == K_OK, result);

    /* step2: get the link to send */
    NetLinkAccPtr link;
    auto result = peerLinkMap_->Find(targetId, link);
    if (!result) {
        return MMC_LINK_NOT_FOUND; /* need to connect */
    }
    /* step3: copy data */
    auto dataBuf = MmcMakeRef<ock::acc::AccDataBuffer>(reqDataLen);
    MMC_ASSERT_RETURN(dataBuf.Get() != nullptr, MMC_NEW_OBJECT_FAILED);
    MMC_ASSERT_RETURN(dataBuf->AllocIfNeed(), MMC_NEW_OBJECT_FAILED);
    memcpy(dataBuf->DataPtrVoid(), static_cast<void*>(const_cast<char*>(reqData)),
           reqDataLen);
    dataBuf->SetDataSize(reqDataLen);

    /* step4: create wait handler and initialize */
    auto waiter = MmcMakeRef<NetWaitHandler>(ctxStore_);
    MMC_ASSERT_RETURN(waiter.Get() != nullptr, MMC_NEW_OBJECT_FAILED);
    MMC_ASSERT_RETURN(waiter->Initialize() == MMC_OK, MMC_ERROR);

    /* step5: put into ctx store before sent the data to peer in case of the peer responses very fast  */
    uint32_t seqNo = 0;
    result = ctxStore_->PutAndGetSeqNo<NetWaitHandler>(waiter.Get(), seqNo);
    MMC_ASSERT_RETURN(result == MMC_OK, result);
    /* step6: send message to peer */

    result = link->RealLink()->NonBlockSend(MSG_TYPE_DATA, opCode, seqNo, dataBuf.Get(), nullptr);
    if (result != MMC_OK) {
        /* remove wait handler from context store */
        ctxStore_->RemoveSeqNo<NetWaitHandler>(seqNo);
        MMC_LOG_ERROR("Failed to send data to service " << targetId);
        return result;
    }

    /* step7: wait for response */
    result = waiter->TimedWait(timeoutInSecond);
    /* if timeout */
    if (result == MMC_TIMEOUT) {
        /* remove waiter in context store */
        ctxStore_->RemoveSeqNo<NetWaitHandler>(seqNo);
        MMC_LOG_WARN("Peer " << targetId << " doesn't response within " << timeoutInSecond << " seconds");
        return result;
    }

    /* got response data and deserialize */
    auto& data = waiter->Data();
    MMC_ASSERT_RETURN(data.Get() != nullptr, MMC_ERROR);
    /* set response code */
    // userResult = waiter->GetResult();
    /* deserialize */
//    result = response->Deserialize(data->DataIntPtr(), data->DataLen());
//    if (result != K_OK) {
//        KVS_LOG_ERROR("Failed to deserialize response data for " << request->OpCode() << " from service " <<
//                                                                 targetId.ToString());
//        return result;
//    }
    if (*respData == nullptr) {
        *respData = (char*) malloc(data->DataLen());
    }
    memcpy(*respData, (void*) data->DataIntPtr(), data->DataLen());
    respDataLen = data->DataLen();
    return result;
}

Result NetEngineAcc::Send(uint32_t peerId, const char *reqData, uint32_t reqDataLen, int32_t timeoutInSecond) {
    MMC_ASSERT_RETURN(started_, MMC_NOT_STARTED);
    MMC_ASSERT_RETURN(reqData != nullptr, MMC_INVALID_PARAM);
    MMC_ASSERT_RETURN(reqDataLen != 0, MMC_INVALID_PARAM);

    return MMC_OK;
}

NetEngineAcc::~NetEngineAcc()
{
    Stop();
}

Result NetEngineAcc::Initialize(const NetEngineOptions &options)
{
    if (inited_) {
        MMC_LOG_INFO("NetEngine [" << options.name << "] already initialized");
        return MMC_OK;
    }

    MMC_LOG_DEBUG("Start to init NetEngine " << options.name);

    /* create concurrent link map */
    NetLinkMapAccPtr tmpLinkMap = MmcMakeRef<NetLinkMapAcc>();
    MMC_ASSERT_RETURN(tmpLinkMap != nullptr, MMC_NEW_OBJECT_FAILED);

    /* create ctx store */
    NetContextStorePtr tmpCtxStore = MmcMakeRef<NetContextStore>(UN65536);
    MMC_ASSERT_RETURN(tmpCtxStore != nullptr, MMC_NEW_OBJECT_FAILED);
    auto result = tmpCtxStore->Initialize();
    MMC_RETURN_ERROR(result, "Failed to initialize ctx store for communication seq number");

    /* create tcp server */
    auto tmpServer = TcpServer::Create();
    MMC_ASSERT_RETURN(tmpServer != nullptr, MMC_NEW_OBJECT_FAILED);
    server_ = tmpServer.Get();

    /* register callbacks */
    options_ = options;
    MMC_RETURN_ERROR(RegisterTcpServerHandler(), "NetEngineAcc " << options_.name << " register handler error");
    /* assign to member variables */
    peerLinkMap_ = tmpLinkMap;
    ctxStore_ = tmpCtxStore;

    inited_ = true;

    return MMC_OK;
}

void NetEngineAcc::UnInitialize()
{
    if (!inited_) {
        MMC_LOG_DEBUG("NetEngine [" << options_.name << "] has not been initialized");
        return;
    }

    /* un-initialize ctx store */
    if (ctxStore_ != nullptr) {
        ctxStore_->UnInitialize();
    }

    /* clear link map */
    if (peerLinkMap_ != nullptr) {
        peerLinkMap_->Clear();
    }

    inited_ = false;
}

Result NetEngineAcc::RegisterTcpServerHandler()
{
    MMC_ASSERT_RETURN(server_ != nullptr, MMC_NOT_INITIALIZED);

    using namespace std::placeholders;
    if (options_.startListener) {
        server_->RegisterNewLinkHandler(std::bind(&NetEngineAcc::HandleNewLink, this, _1, _2));
    }

    server_->RegisterNewRequestHandler(MSG_TYPE_DATA, std::bind(&NetEngineAcc::HandleNeqRequest, this, _1));
    server_->RegisterRequestSentHandler(MSG_TYPE_DATA, std::bind(&NetEngineAcc::HandleMsgSent, this, _1, _2, _3));
    server_->RegisterLinkBrokenHandler(std::bind(&NetEngineAcc::HandleLinkBroken, this, _1));

    return MMC_OK;
}

Result NetEngineAcc::HandleNewLink(const TcpConnReq &req, const TcpLinkPtr &link) const
{
    MMC_ASSERT_RETURN(link.Get() != nullptr, MMC_INVALID_PARAM);

    auto peerId = static_cast<uint32_t>(req.rankId);
    link->UpCtx(peerId);

    auto newLinkAcc = MmcMakeRef<NetLinkAcc>(peerId, link);
    MMC_ASSERT_RETURN(newLinkAcc != nullptr, MMC_NEW_OBJECT_FAILED);
    MMC_LOG_INFO("NEW Link " << newLinkAcc.Get());

    /* add into peer link map */
    peerLinkMap_->Add(peerId, newLinkAcc);
    MMC_LOG_DEBUG("HandleNewLink with peer id: " << req.rankId);
    MMC_LOG_INFO("HandleNewLink with peer id: " << req.rankId);
    Result ret = MMC_OK;
    if (newLinkHandler_ != nullptr) {
        ret = newLinkHandler_(newLinkAcc.Get());
    }
    return MMC_OK;
}

Result NetEngineAcc::HandleNeqRequest(const TcpReqContext &context)
{
    /* use result variable for real opcode */
    MMC_LOG_DEBUG("HandleNeqRequest Header " << context.Header().ToString());
    NetContextPtr contextPtr = MmcMakeRef<NetContextAcc>(&context).Get();
    MMC_ASSERT_RETURN(contextPtr != nullptr, MMC_NEW_OBJECT_FAILED);
    int16_t opCode = contextPtr->OpCode();
    MMC_ASSERT_RETURN(opCode < gHandlerSize, MMC_NET_REQ_HANDLE_NO_FOUND);
    if (reqReceivedHandlers_[opCode]  == nullptr) {
        /*  client do reply response */
        MMC_ASSERT_RETURN(HandleAllRequests4Response(context) == MMC_OK, MMC_ERROR);
    } else {
        /* server do function */
        MMC_ASSERT_RETURN(reqReceivedHandlers_[opCode](contextPtr) == MMC_OK, MMC_ERROR);
    }
    return MMC_OK;
}


Result NetEngineAcc::HandleMsgSent(TcpMsgSentResult result, const TcpMsgHeader &header, const TcpDataBufPtr &cbCtx)
{
    // TODO
    return MMC_OK;
}

Result NetEngineAcc::HandleLinkBroken(const TcpLinkPtr &link) const
{
    MMC_ASSERT_RETURN(link.Get() != nullptr, MMC_INVALID_PARAM);

    const auto peerId = static_cast<uint32_t>(link->UpCtx());

    auto linkAcc = MmcMakeRef<NetLinkAcc>(peerId, link);
    MMC_ASSERT_RETURN(linkAcc != nullptr, MMC_NEW_OBJECT_FAILED);

    /* add into peer link map */
    peerLinkMap_->Find(peerId, linkAcc);

    Result ret = MMC_OK;
    MMC_LOG_INFO("XXXX");
    if (linkBrokenHandler_ != nullptr) {
        ret = linkBrokenHandler_(linkAcc.Get());
    }
    if (ret != MMC_OK)
    {
        MMC_LOG_WARN("Failed to remove link with id " << peerId);
    }
    const auto result = peerLinkMap_->Remove(peerId);
    if (result) {
        return MMC_OK;
    }

    MMC_LOG_WARN("Failed to remove link with id " << peerId);
    return MMC_ERROR;
}

/*
Result NetEngineAcc::LoadDynamicLib(const std::string &dynLibPath)
{
    MMC_ASSERT_RETURN(server_ != nullptr, MMC_ERROR);
    return server_->LoadDynamicLib(dynLibPath);
}
*/

Result NetEngineAcc::ConnectToPeer(uint32_t peerId, const std::string &peerIp, uint16_t port, NetLinkPtr &newLink,
                                   bool isForce)
{
    MMC_ASSERT_RETURN(started_, MMC_NOT_STARTED);
    MMC_ASSERT_RETURN(!peerIp.empty(), MMC_INVALID_PARAM);
    MMC_ASSERT_RETURN(port != 0, MMC_INVALID_PARAM);

    TcpConnReq connReq;
    connReq.rankId = peerId;
    connReq.version = static_cast<int16_t>(NetProtoVersion::VERSION_1);
    connReq.magic = NET_SERVER_MAGIC;

    NetLinkAccPtr linkAcc;
    /* connect to peer with mutex held, in case of multiple threads connect to peer at the same time */
    std::lock_guard<std::mutex> guard(connectMutex_);
    /* double check, in case of other thread already connected */
    auto result = peerLinkMap_->Find(peerId, linkAcc);
    if (result == true) {
        if (!isForce) {
            MMC_LOG_INFO("The link to peer " << peerId << " already exists");
            return MMC_OK;
        } else {
            peerLinkMap_->Remove(peerId);
        }
    }

    MMC_LOG_DEBUG("Connecting to " << peerIp << ":" << port << "");

    /* connect */
    TcpLinkPtr realLink;
    result = server_->ConnectToPeerServer(peerIp, port, connReq, realLink);
    if (result != MMC_OK) {
        MMC_LOG_ERROR("Failed to connection to peerId: " << peerId << ", peerIpPort: " << peerIp << ":" << port);
        return result;
    }
    realLink->UpCtx(peerId);

    /* add into peer link map */
    linkAcc = MmcMakeRef<NetLinkAcc>(peerId, realLink);
    MMC_ASSERT_RETURN(linkAcc != nullptr, MMC_NEW_OBJECT_FAILED);
    peerLinkMap_->Add(peerId, linkAcc);

    /* set peer id */
    newLink = linkAcc.Get();
    newLink->UpCtx(peerId);

    return MMC_OK;
}

Result NetEngineAcc::HandleAllRequests4Response(const TcpReqContext &context)
{
    MMC_LOG_DEBUG("Get request with seqNo " << context.SeqNo());
    NetWaitHandler *out = nullptr;
    auto result = ctxStore_->GetSeqNoAndRemove<NetWaitHandler>(context.SeqNo(), out, false);
    /* check if out is nullptr */
    MMC_ASSERT_RETURN(out != nullptr, MMC_ERROR);
    if (result != MMC_OK) {
        if (out != nullptr) { /* decrease ref */
            out->DecreaseRef();
        }

        MMC_LOG_WARN("Failed to get waiter from ctx store with seqNo " << context.SeqNo() << ", probably timeout");
        return MMC_OK;
    }

    NetWaitHandlerPtr waiter = out;
    out->DecreaseRef(); /* decrease ref */

    auto dataBuf = MmcMakeRef<ock::acc::AccDataBuffer>(context.DataLen());
    MMC_ASSERT_RETURN(result == MMC_OK, MMC_NEW_OBJECT_FAILED);
    MMC_ASSERT_RETURN(dataBuf->AllocIfNeed(), MMC_NEW_OBJECT_FAILED);
    memcpy(dataBuf->DataPtrVoid(), context.DataPtr(), context.DataLen());
    dataBuf->SetDataSize(context.DataLen());

    result = waiter->Notify(context.Header().result, dataBuf.Get());
    if (result == MMC_ALREADY_NOTIFIED) {
        MMC_LOG_WARN("Already notify wait handler for seqNo " << context.SeqNo());
        return MMC_OK;
    }

    return MMC_OK;
}
}
}
