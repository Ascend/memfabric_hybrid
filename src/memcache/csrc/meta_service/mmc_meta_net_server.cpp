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
std::string Join(const std::vector<std::string> &vec) {
    std::string result = "[";
    for (const std::string &str : vec) {
        result += "\"" + str + "\", ";
    }
    if (!vec.empty()) {
        result.pop_back();  // space
        result.pop_back();  // comma
    }
    result += "]";
    return result;
}

MetaNetServer::MetaNetServer(const MmcMetaServicePtr &metaService, const std::string inputName)
    : metaService_(metaService),
      name_(inputName)
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
    options.tlsOption = metaService_->Options().tlsConfig;

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
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_GET_REQ,
                                      std::bind(&MetaNetServer::HandleBatchGet, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_REMOVE_REQ,
                                      std::bind(&MetaNetServer::HandleRemove, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_REMOVE_REQ,
                                  std::bind(&MetaNetServer::HandleBatchRemove, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_IS_EXIST_REQ,
                                      std::bind(&MetaNetServer::HandleIsExist, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_IS_EXIST_REQ,
                                      std::bind(&MetaNetServer::HandleBatchIsExist, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BM_UNREGISTER_REQ,
                                      std::bind(&MetaNetServer::HandleBmUnregister, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_QUERY_REQ,
                                      std::bind(&MetaNetServer::HandleQuery, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_QUERY_REQ,
                                      std::bind(&MetaNetServer::HandleBatchQuery, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::LM_PING_REQ, nullptr);
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::LM_META_REPLICATE_REQ, nullptr);
    server->RegNewLinkHandler(std::bind(&MetaNetServer::HandleNewLink, this, std::placeholders::_1));
    server->RegLinkBrokenHandler(std::bind(&MetaNetServer::HandleLinkBroken, this, std::placeholders::_1));

    /* start engine */
    MMC_ASSERT_RETURN(server->Start(options) == MMC_OK, MMC_NOT_STARTED);

    engine_ = server;
    started_ = true;
    MMC_LOG_INFO("initialize meta net server success [" << name_ << "]");
    return MMC_OK;
}

Result MetaNetServer::HandleBmRegister(const NetContextPtr &context)
{
    auto metaServiceDefaultPtr = Convert<MmcMetaService, MmcMetaServiceDefault>(metaService_);
    BmRegisterRequest req;
    context->GetRequest<BmRegisterRequest>(req);
    MMC_LOG_DEBUG("HandleBmRegister rand " << req.rank_ << " capacity " << req.capacity_);
    auto result = metaServiceDefaultPtr->BmRegister(req.rank_, req.mediaType_, req.addr_, req.capacity_);
    Response resp;
    resp.ret_ = result;
    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBmUnregister(const NetContextPtr &context)
{
    auto metaServiceDefaultPtr = Convert<MmcMetaService, MmcMetaServiceDefault>(metaService_);
    BmUnregisterRequest req;
    context->GetRequest<BmUnregisterRequest>(req);
    MMC_LOG_INFO("HandleBmUnregister rank : " << req.rank_ << ", start");
    auto result = metaServiceDefaultPtr->BmUnregister(req.rank_, req.mediaType_);
    MMC_LOG_INFO("HandleBmUnregister rank : " << req.rank_ << ", get return : " << result);
    Response resp;
    resp.ret_ = result;
    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandlePing(const NetContextPtr &context)
{
    std::string str{static_cast<char *>(context->Data()), context->DataLen()};
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
    MMC_LOG_INFO(name_ << " new link addr " << link.Get());
    return MMC_OK;
}

Result MetaNetServer::HandleLinkBroken(const NetLinkPtr &link)
{
    MMC_LOG_INFO(name_ << " link addr " << link.Get() << " broken");
    auto metaServiceDefaultPtr = Convert<MmcMetaService, MmcMetaServiceDefault>(metaService_);
    int32_t rankId = link->Id();
    return metaServiceDefaultPtr->ClearResource(rankId);
}

Result MetaNetServer::HandleAlloc(const NetContextPtr &context)
{
    AllocRequest req;
    AllocResponse resp;
    context->GetRequest<AllocRequest>(req);

    MMC_LOG_DEBUG("HandleAlloc key " << req.key_ << " start.");
    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->Alloc(req, resp);
    MMC_LOG_DEBUG("HandleAlloc key " << req.key_ << " finish.");

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleUpdate(const NetContextPtr &context)
{
    UpdateRequest req;
    Response resp;
    context->GetRequest<UpdateRequest>(req);

    MMC_LOG_DEBUG("HandleUpdate key " << req.key_ << " start.");
    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->UpdateState(req, resp);
    MMC_LOG_DEBUG("HandleUpdate key " << req.key_ << " finish.");

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleGet(const NetContextPtr &context)
{
    GetRequest req;
    AllocResponse resp;
    context->GetRequest<GetRequest>(req);

    MMC_LOG_DEBUG("HandleGet key " << req.key_ << " start.");
    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->Get(req, resp);
    MMC_LOG_DEBUG("HandleGet key " << req.key_ << " finish.");

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBatchGet(const NetContextPtr &context)
{
    BatchGetRequest req;
    BatchAllocResponse resp;
    context->GetRequest<BatchGetRequest>(req);

    MMC_LOG_DEBUG("HandleBatchGet keys (size  " << req.keys_.size() << ") start: " << Join(req.keys_));
    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->BatchGet(req, resp);
    MMC_LOG_DEBUG("HandleBatchGet keys (size  " << req.keys_.size() << ") finish: " << Join(req.keys_));

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleRemove(const NetContextPtr &context)
{
    RemoveRequest req;
    Response resp;
    context->GetRequest<RemoveRequest>(req);

    MMC_LOG_DEBUG("HandleRemove key " << req.key_ << " start.");
    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->Remove(req, resp);
    MMC_LOG_DEBUG("HandleRemove key " << req.key_ << " finish.");

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBatchRemove(const NetContextPtr &context)
{
    BatchRemoveRequest req;
    BatchRemoveResponse resp;
    context->GetRequest<BatchRemoveRequest>(req);

    MMC_LOG_DEBUG("HandleBatchRemove keys (size  " << req.keys_.size() << ") start: " << Join(req.keys_));
    MmcMetaMgrProxyPtr metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->BatchRemove(req, resp);
    MMC_LOG_DEBUG("HandleBatchRemove keys (size  " << req.keys_.size() << ") finish: " << Join(req.keys_));

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleIsExist(const NetContextPtr &context)
{
    IsExistRequest req;
    IsExistResponse resp;
    context->GetRequest<IsExistRequest>(req);

    MMC_LOG_DEBUG("HandleIsExist key " << req.key_ << " start.");
    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    MMC_LOG_DEBUG("HandleIsExist key " << req.key_ << " finish.");
    metaMgrProxy->ExistKey(req, resp);

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBatchIsExist(const NetContextPtr &context)
{
    BatchIsExistRequest req;
    BatchIsExistResponse resp;
    context->GetRequest<BatchIsExistRequest>(req);

    MMC_LOG_DEBUG("HandleBatchIsExist keys (size " << req.keys_.size() << ") start: " << Join(req.keys_));
    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->BatchExistKey(req, resp);
    MMC_LOG_DEBUG("HandleBatchIsExist keys (size " << req.keys_.size() << ") finish: " << Join(req.keys_));

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleQuery(const NetContextPtr &context)
{
    QueryRequest req;
    QueryResponse resp;
    context->GetRequest<QueryRequest>(req);

    MMC_LOG_DEBUG("HandleQuery key " << req.key_ << " start.");
    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->Query(req, resp);
    MMC_LOG_DEBUG("HandleQuery key " << req.key_ << " finish.");

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBatchQuery(const NetContextPtr &context)
{
    BatchQueryRequest req;
    BatchQueryResponse resp;
    context->GetRequest<BatchQueryRequest>(req);

    MMC_LOG_INFO("HandleBatchQuery keys (size " << req.keys_.size() << ") start: " << Join(req.keys_));
    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    metaMgrProxy->BatchQuery(req, resp);
    MMC_LOG_INFO("HandleBatchQuery keys (size " << req.keys_.size() << ") finish: " << Join(req.keys_));

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
