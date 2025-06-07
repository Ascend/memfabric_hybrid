/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_net_engine_acc.h"
#include "mmc_net_ctx_store.h"

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
    MMC_RETURN_NOT_OK(VerifyOptions(options));

    /* initialize */
    MMC_RETURN_NOT_OK(Initialize(options));

    /* check handler */
    if (options_.startListener && newLinkHandler_ == nullptr) {
        MMC_LOG_ERROR("No new link handler function registered, call 'RegisterNewLinkHandler' to register");
        return MMC_INVALID_PARAM;
    } else if (!options_.startListener && newLinkHandler_ != nullptr) {
        MMC_LOG_ERROR("No need to register new link handler function for the engine without listener");
        return MMC_INVALID_PARAM;
    }

    /* call start inner */
    MMC_RETURN_NOT_OK(StartInner());

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
    // TODO

    /* start server, listen and thread will be started */
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(server_->Start(serverOptions, tlsOpt),
                                    "Failed to start tcp server for NetEngine " << options_.name);

    return MMC_OK;
}

Result NetEngineAcc::StopInner()
{
    /* stop server */
    if (server_ != nullptr) {
        server_->Stop();
    }

    return MMC_OK;
}

Result NetEngineAcc::Call(uint32_t targetId, const char *reqData, uint32_t reqDataLen, char *respData,
                          uint32_t &respDataLen, int32_t timeoutInSecond)
{
    MMC_ASSERT_RETURN(started_, MMC_NOT_STARTED);
    MMC_ASSERT_RETURN(reqData != nullptr, MMC_INVALID_PARAM);
    MMC_ASSERT_RETURN(respData != nullptr, MMC_INVALID_PARAM);

    // TODO

    return MMC_OK;
}

NetEngineAcc::~NetEngineAcc()
{
    Stop();
}

Result NetEngineAcc::Initialize(const NetEngineOptions &options)
{
    std::lock_guard<std::mutex> guard(mutex_);
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
    MMC_LOG_ERROR_AND_RETURN_NOT_OK(result, "Failed to initialize ctx store for communication seq number");

    /* create tcp server */
    auto tmpServer = TcpServer::Create();
    MMC_ASSERT_RETURN(tmpServer != nullptr, MMC_NEW_OBJECT_FAILED);

    /* register callbacks */
    MMC_RETURN_NOT_OK(RegisterTcpServerHandler());

    /* assign to member variables */
    server_ = tmpServer.Get();
    peerLinkMap_ = tmpLinkMap;
    ctxStore_ = tmpCtxStore;

    options_ = options;
    inited_ = true;

    return MMC_OK;
}

void NetEngineAcc::UnInitialize()
{
    std::lock_guard<std::mutex> guard(mutex_);
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

    /* add into peer link map */
    peerLinkMap_->Add(peerId, newLinkAcc);
    MMC_LOG_DEBUG("HandleNewLink with peer id: " << req.rankId);
    return MMC_OK;
}

Result NetEngineAcc::HandleNeqRequest(const TcpReqContext &context)
{
    // TODO
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

    auto peerId = static_cast<uint32_t>(link->UpCtx());

    auto result = peerLinkMap_->Remove(peerId);
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
}
}
