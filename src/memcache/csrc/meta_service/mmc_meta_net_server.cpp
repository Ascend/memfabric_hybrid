/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_meta_net_server.h"
#include "mmc_meta_service.h"
#include "mmc_msg_base.h"
#include "mmc_msg_client_meta.h"
#include "mmc_meta_service_default.h"

namespace ock {
namespace mmc {
MetaNetServer::MetaNetServer(const MmcMetaServicePtr &metaService, const std::string inputName)
    : metaService_(metaService), name_(inputName)
{
}
MetaNetServer::~MetaNetServer() {}
Result ock::mmc::MetaNetServer::Start()
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (started_) {
        MMC_LOG_INFO("MetaNetServer [" << name_ << "] already started");
        return MMC_OK;
    }

    MMC_ASSERT_RETURN(metaService_.Get() != nullptr, MMC_INVALID_PARAM);

    /* init engine */
    NetEngineOptions options;
    std::string url{metaService_->Options().discoveryURL};
    NetEngineOptions::ExtractIpPortFromUrl(url, options);
    options.name = name_;
    options.threadCount = 2;
    options.rankId = 0;
    options.startListener = true;

    NetEnginePtr server = NetEngine::Create();
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_ALLOC_REQ,
                                      std::bind(&MetaNetServer::HandleAlloc, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BM_REGISTER_REQ,
                                      std::bind(&MetaNetServer::HandleBmRegister, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_PING_REQ,
                                      std::bind(&MetaNetServer::HandlePing, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_UPDATE_REQ,
                                      std::bind(&MetaNetServer::HandleUpdate, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_GET_REQ,
                                      std::bind(&MetaNetServer::HandleGet, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_REMOVE_REQ,
                                      std::bind(&MetaNetServer::HandleRemove, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_IS_EXIST_REQ,
                                      std::bind(&MetaNetServer::HandleIsExist, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_IS_EXIST_REQ,
                                      std::bind(&MetaNetServer::HandleBatchIsExist, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::LM_PING_REQ, nullptr);
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::LM_META_REPLICATE_REQ, nullptr);
    server->RegNewLinkHandler(std::bind(&MetaNetServer::HandleNewLink, this, std::placeholders::_1));

    /* start engine */
    MMC_ASSERT_RETURN(server->Start(options) == MMC_OK, MMC_NOT_STARTED);

    engine_ = server;
    started_ = true;
    MMC_LOG_INFO("initialize meta net server success [" << name_ << "]");
    return MMC_OK;
}

Result MetaNetServer::HandleBmRegister(const NetContextPtr &context)
{
    MmcMetaServiceDefaultPtr metaServiceDefaultPtr = Convert<MmcMetaService, MmcMetaServiceDefault>(metaService_);
    BmRegisterRequest req;
    context->GetRequest<BmRegisterRequest>(req);
    MMC_LOG_INFO("HandleBmRegister  " << req.rank_);
    auto result = metaServiceDefaultPtr->BmRegister(req.rank_, req.mediaType_, req.addr_, req.capacity_);
    MMC_LOG_INFO("HandleBmRegister  " << req.rank_);
    Response resp;
    resp.ret_ = result;
    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandlePing(const NetContextPtr &context)
{
    std::string str{(char *)context->Data(), context->DataLen()};
    NetMsgUnpacker unpacker(str);
    PingMsg req;
    req.Deserialize(unpacker);
    MMC_LOG_INFO("HandlePing num " << req.num);

    NetMsgPacker packer;
    PingMsg recv;
    recv.Serialize(packer);
    std::string serializedData = packer.String();
    uint32_t retSize = serializedData.length();
    return context->Reply(req.msgId, const_cast<char *>(serializedData.c_str()), retSize);
}

Result MetaNetServer::HandleNewLink(const NetLinkPtr &link)
{
    MMC_LOG_INFO("new link " << name_ << " addr " << link.Get() );
    return MMC_OK;
}

Result MetaNetServer::HandleAlloc(const NetContextPtr &context)
{
    AllocRequest req;
    AllocResponse resp;
    context->GetRequest<AllocRequest>(req);

    MMC_LOG_INFO("HandleAlloc key " << req.key_);
    MmcMetaMgrProxyPtr metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->Alloc(req, resp);
    MMC_LOG_INFO("HandleAlloc key " << req.key_);

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleUpdate(const NetContextPtr &context)
{
    UpdateRequest req;
    Response resp;
    context->GetRequest<UpdateRequest>(req);

    MMC_LOG_INFO("HandleUpdate key " << req.key_);
    MmcMetaMgrProxyPtr metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->UpdateState(req, resp);
    MMC_LOG_INFO("HandleUpdate key " << req.key_);

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleGet(const NetContextPtr &context)
{
    GetRequest req;
    AllocResponse resp;
    context->GetRequest<GetRequest>(req);

    MMC_LOG_INFO("HandleGet key " << req.key_);
    MmcMetaMgrProxyPtr metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->Get(req, resp);
    MMC_LOG_INFO("HandleGet key " << req.key_);

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleRemove(const NetContextPtr &context)
{
    RemoveRequest req;
    Response resp;
    context->GetRequest<RemoveRequest>(req);

    MMC_LOG_INFO("HandleRemove key " << req.key_);
    MmcMetaMgrProxyPtr metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->Remove(req, resp);
    MMC_LOG_INFO("HandleRemove key " << req.key_);

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleIsExist(const NetContextPtr &context)
{
    IsExistRequest req;
    Response resp;
    context->GetRequest<IsExistRequest>(req);

    MMC_LOG_INFO("HandleIsExist key " << req.key_);
    MmcMetaMgrProxyPtr metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->ExistKey(req, resp);
    MMC_LOG_INFO("HandleIsExist key " << req.key_);

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBatchIsExist(const NetContextPtr &context)
{
    BatchIsExistRequest req;
    BatchIsExistResponse resp;
    context->GetRequest<BatchIsExistRequest>(req);

    MMC_LOG_INFO("HandleBatchIsExist keys");
    MmcMetaMgrProxyPtr metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->BatchExistKey(req, resp);
    MMC_LOG_INFO("HandleBatchIsExist keys");

    return context->Reply(req.msgId, resp);
}

void MetaNetServer::Stop()
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!started_) {
        MMC_LOG_WARN("MetaNetServer has not been started");
        return;
    }
    engine_->Stop();
    started_ = false;
}
}  // namespace mmc
}  // namespace ock
