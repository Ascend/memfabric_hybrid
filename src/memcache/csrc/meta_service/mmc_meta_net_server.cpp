/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "mmc_meta_net_server.h"
#include "mmc_meta_service.h"
#include "mmc_msg_base.h"
#include "mmc_msg_client_meta.h"
#include "mmc_meta_service_default.h"
#include "mmc_ptracer.h"

namespace ock {
namespace mmc {
std::string Join(const std::vector<std::string> &vec)
{
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
Result ock::mmc::MetaNetServer::Start(NetEngineOptions &options)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (started_) {
        MMC_LOG_INFO("MetaNetServer [" << name_ << "] already started");
        return MMC_OK;
    }

    MMC_ASSERT_RETURN(metaService_.Get() != nullptr, MMC_INVALID_PARAM);

    NetEnginePtr server = NetEngine::Create();
    MMC_ASSERT_RETURN(server != nullptr, MMC_MALLOC_FAILED);
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_ALLOC_REQ,
                                      std::bind(&MetaNetServer::HandleAlloc, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BM_REGISTER_REQ,
                                      std::bind(&MetaNetServer::HandleBmRegister, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_PING_REQ,
                                      std::bind(&MetaNetServer::HandlePing, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_UPDATE_REQ,
                                      std::bind(&MetaNetServer::HandleUpdate, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_UPDATE_REQ,
                                      std::bind(&MetaNetServer::HandleBatchUpdate, this, std::placeholders::_1));
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
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_ALLOC_REQ,
                                      std::bind(&MetaNetServer::HandleBatchAlloc, this, std::placeholders::_1));
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::LM_PING_REQ, nullptr);
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::LM_META_REPLICATE_REQ, nullptr);
    server->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::LM_BLOB_COPY_REQ, nullptr);
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
    MMC_ASSERT_RETURN(metaServiceDefaultPtr != nullptr, MMC_ERROR);
    MMC_ASSERT_RETURN(context != nullptr, MMC_ERROR);
    BmRegisterRequest req;
    context->GetRequest<BmRegisterRequest>(req);
    TP_TRACE_BEGIN(TP_MMC_META_BM_REGISTER);
    auto result = metaServiceDefaultPtr->BmRegister(req.rank_, req.mediaType_, req.addr_, req.capacity_, req.blobMap_);
    TP_TRACE_END(TP_MMC_META_BM_REGISTER, result);
    MMC_LOG_INFO("HandleBmRegister rank:" << req.rank_ << ", rebuild blob size " << req.blobMap_.size()
                                          << ", ret:" << result);
    Response resp;
    resp.ret_ = result;
    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBmUnregister(const NetContextPtr &context)
{
    auto metaServiceDefaultPtr = Convert<MmcMetaService, MmcMetaServiceDefault>(metaService_);
    BmUnregisterRequest req;
    Response resp;
    resp.ret_ = MMC_OK;
    context->GetRequest<BmUnregisterRequest>(req);
    for (auto type : req.mediaType_) {
        TP_TRACE_BEGIN(TP_MMC_META_BM_UNREGISTER);
        auto result = metaServiceDefaultPtr->BmUnregister(req.rank_, type);
        TP_TRACE_END(TP_MMC_META_BM_UNREGISTER, result);
        MMC_LOG_INFO("HandleBmUnregister rank:" << req.rank_ << ", media:" << type << ", ret:" << result);
        if (result != MMC_OK) {
            MMC_LOG_ERROR("HandleBmUnregister rank:" << req.rank_ << ", media:" << type << ", ret:" << result);
            resp.ret_ = result;
        }
    }
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
    MMC_LOG_INFO(name_ << " new link");
    return MMC_OK;
}

Result MetaNetServer::HandleLinkBroken(const NetLinkPtr &link)
{
    MMC_LOG_INFO(name_ << " link broken");
    auto metaServiceDefaultPtr = Convert<MmcMetaService, MmcMetaServiceDefault>(metaService_);
    int32_t rankId = link->Id();
    TP_TRACE_BEGIN(TP_MMC_META_CLEAR_RESOURCE);
    auto ret = metaServiceDefaultPtr->ClearResource(rankId);
    TP_TRACE_END(TP_MMC_META_CLEAR_RESOURCE, ret);
    return ret;
}

Result MetaNetServer::HandleAlloc(const NetContextPtr &context)
{
    MMC_ASSERT_RETURN(context != nullptr, MMC_ERROR);
    AllocRequest req;
    AllocResponse resp;
    context->GetRequest<AllocRequest>(req);
    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_PUT);
    const auto result = metaMgrProxy->Alloc(req, resp);
    TP_TRACE_END(TP_MMC_META_PUT, result);
    if (result != MMC_OK) {
        if (result != MMC_DUPLICATED_OBJECT) {
            MMC_LOG_ERROR("HandleAlloc key " << req.key_ << " failed, error code=" << result);
        }
    } else {
        MMC_LOG_DEBUG("HandleAlloc key " << req.key_ << " success.");
    }
    resp.result_ = result;

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBatchAlloc(const NetContextPtr &context)
{
    BatchAllocRequest req;
    BatchAllocResponse resp;

    Result getResult = context->GetRequest<BatchAllocRequest>(req);
    if (getResult != MMC_OK) {
        MMC_LOG_ERROR("Failed to get BatchAllocRequest: " << getResult);
        return getResult;
    }

    MMC_LOG_INFO("HandleBatchAlloc start. Keys count: " << req.keys_.size()
                 << ", OperateId: " << req.operateId_
                 << ", Flags: " << req.flags_);
    
    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_BATCH_PUT);
    Result batchResult = metaMgrProxy->BatchAlloc(req, resp);
    TP_TRACE_END(TP_MMC_META_BATCH_PUT, batchResult);
    if (batchResult != MMC_OK) {
        MMC_LOG_ERROR("BatchAlloc failed. Keys count: " << req.keys_.size()
                     << ", Error: " << batchResult);
    }
    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleUpdate(const NetContextPtr &context)
{
    UpdateRequest req;
    Response resp;
    context->GetRequest<UpdateRequest>(req);

    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_UPDATE);
    metaMgrProxy->UpdateState(req, resp);
    TP_TRACE_END(TP_MMC_META_UPDATE, resp.ret_);
    MMC_LOG_DEBUG("HandleUpdate key " << req.key_ << " finish.");

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBatchUpdate(const NetContextPtr &context)
{
    BatchUpdateRequest req;
    BatchUpdateResponse resp;
    context->GetRequest<BatchUpdateRequest>(req);

    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_BATCH_UPDATE);
    auto ret = metaMgrProxy->BatchUpdateState(req, resp);
    TP_TRACE_END(TP_MMC_META_BATCH_UPDATE, ret);
    MMC_LOG_INFO("HandleBatchUpdate keys (size " << req.keys_.size() << ") finish: " << Join(req.keys_));

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleGet(const NetContextPtr &context)
{
    GetRequest req;
    AllocResponse resp;
    context->GetRequest<GetRequest>(req);

    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_GET);
    metaMgrProxy->Get(req, resp);
    TP_TRACE_END(TP_MMC_META_GET, resp.result_);
    MMC_LOG_DEBUG("HandleGet key " << req.key_ << " finish.");

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBatchGet(const NetContextPtr &context)
{
    BatchGetRequest req;
    BatchAllocResponse resp;
    context->GetRequest<BatchGetRequest>(req);

    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_BATCH_GET);
    auto ret = metaMgrProxy->BatchGet(req, resp);
    TP_TRACE_END(TP_MMC_META_BATCH_GET, ret);
    MMC_LOG_DEBUG("HandleBatchGet keys (size  " << req.keys_.size() << ") finish: " << Join(req.keys_));

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleRemove(const NetContextPtr &context)
{
    RemoveRequest req;
    Response resp;
    context->GetRequest<RemoveRequest>(req);

    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_REMOVE);
    metaMgrProxy->Remove(req, resp);
    TP_TRACE_END(TP_MMC_META_REMOVE, resp.ret_);
    MMC_LOG_DEBUG("HandleRemove key " << req.key_ << " finish.");

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBatchRemove(const NetContextPtr &context)
{
    BatchRemoveRequest req;
    BatchRemoveResponse resp;
    context->GetRequest<BatchRemoveRequest>(req);

    MmcMetaMgrProxyPtr metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_BATCH_REMOVE);
    auto ret = metaMgrProxy->BatchRemove(req, resp);
    TP_TRACE_END(TP_MMC_META_BATCH_REMOVE, ret);
    MMC_LOG_DEBUG("HandleBatchRemove keys (size  " << req.keys_.size() << ") finish: " << Join(req.keys_));

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleIsExist(const NetContextPtr &context)
{
    IsExistRequest req;
    IsExistResponse resp;
    context->GetRequest<IsExistRequest>(req);

    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_EXIST);
    metaMgrProxy->ExistKey(req, resp);
    TP_TRACE_END(TP_MMC_META_EXIST, resp.ret_);
    MMC_LOG_DEBUG("HandleIsExist key " << req.key_ << " finish.");

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBatchIsExist(const NetContextPtr &context)
{
    BatchIsExistRequest req;
    BatchIsExistResponse resp;
    context->GetRequest<BatchIsExistRequest>(req);

    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_BATCH_EXIST);
    auto ret = metaMgrProxy->BatchExistKey(req, resp);
    TP_TRACE_END(TP_MMC_META_BATCH_EXIST, ret);
    MMC_LOG_DEBUG("HandleBatchIsExist keys (size " << req.keys_.size() << ") finish: " << Join(req.keys_));

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleQuery(const NetContextPtr &context)
{
    QueryRequest req;
    QueryResponse resp;
    context->GetRequest<QueryRequest>(req);

    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_QUERY);
    auto ret = metaMgrProxy->Query(req, resp);
    TP_TRACE_END(TP_MMC_META_QUERY, ret);
    MMC_LOG_DEBUG("HandleQuery key " << req.key_ << " finish.");

    return context->Reply(req.msgId, resp);
}

Result MetaNetServer::HandleBatchQuery(const NetContextPtr &context)
{
    BatchQueryRequest req;
    BatchQueryResponse resp;
    context->GetRequest<BatchQueryRequest>(req);

    auto &metaMgrProxy = metaService_->GetMetaMgrProxy();
    TP_TRACE_BEGIN(TP_MMC_META_BATCH_QUERY);
    auto ret = metaMgrProxy->BatchQuery(req, resp);
    TP_TRACE_END(TP_MMC_META_BATCH_QUERY, ret);
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