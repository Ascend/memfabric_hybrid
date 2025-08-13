/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include "mmc_local_service_default.h"
#include "mmc_meta_net_client.h"
#include "mmc_msg_base.h"
#include "mmc_msg_client_meta.h"

namespace ock {
namespace mmc {
std::map<std::string, MmcRef<MetaNetClient>> MetaNetClientFactory::instances_;
std::mutex MetaNetClientFactory::instanceMutex_;

MetaNetClient::~MetaNetClient() {}
MetaNetClient::MetaNetClient(const std::string &serverUrl, const std::string& inputName)
    : serverUrl_(serverUrl), name_(inputName)
{}

Result MetaNetClient::Start(const NetEngineOptions &config)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (started_) {
        MMC_LOG_INFO("MetaNetServer [" << name_ << "] already started");
        return MMC_OK;
    }

    /* init engine */

    NetEnginePtr client = NetEngine::Create();
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_PING_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_ALLOC_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_UPDATE_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_UPDATE_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_GET_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_GET_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_REMOVE_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_REMOVE_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BM_REGISTER_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_IS_EXIST_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_IS_EXIST_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BM_UNREGISTER_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_QUERY_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_QUERY_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::ML_BATCH_ALLOC_REQ, nullptr);
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::LM_PING_REQ,
                                      std::bind(&MetaNetClient::HandlePing, this, std::placeholders::_1));
    client->RegRequestReceivedHandler(LOCAL_META_OPCODE_REQ::LM_META_REPLICATE_REQ,
                                      std::bind(&MetaNetClient::HandleMetaReplicate, this, std::placeholders::_1));
    client->RegLinkBrokenHandler(std::bind(&MetaNetClient::HandleLinkBroken, this, std::placeholders::_1));
    /* start engine */
    MMC_ASSERT_RETURN(client->Start(config) == MMC_OK, MMC_NOT_STARTED);

    engine_ = client;
    rankId_ = config.rankId;
    started_ = true;
    MMC_LOG_INFO("initialize meta net server success [" << name_ << "]");
    return MMC_OK;
}

void MetaNetClient::Stop()
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!started_) {
        MMC_LOG_WARN("MetaNetClient has not been started");
        return;
    }
    engine_->Stop();
    started_ = false;
}

Result MetaNetClient::Connect(const std::string &url)
{
    NetEngineOptions options;
    NetEngineOptions::ExtractIpPortFromUrl(url, options);
    MMC_RETURN_ERROR(engine_->ConnectToPeer(rankId_, options.ip, options.port, link2Index_, false),
                     "MetaNetClient Connect " << url << " failed");
    ip_ = options.ip;
    port_ = options.port;
    return MMC_OK;
}

Result MetaNetClient::HandleMetaReplicate(const NetContextPtr &context)
{
    MetaReplicateRequest req;
    Response resp;
    context->GetRequest<MetaReplicateRequest>(req);

    MMC_LOG_INFO("HandleMetaReplicate key " << req.key_);
    if (req.op_ == 0) {
        if (replicateHandler_ != nullptr) {
            resp.ret_ = replicateHandler_(req.key_, req.blob_);
        } else {
            MMC_LOG_ERROR("replicateHandler_ is nullptr");
            resp.ret_ = MMC_ERROR;
        }
    } else if (req.op_ == 1) {
        if (replicateRemoveHandler_ != nullptr) {
            resp.ret_ = replicateRemoveHandler_(req.key_);
        } else {
            MMC_LOG_ERROR("replicateRemoveHandler_ is nullptr");
            resp.ret_ = MMC_ERROR;
        }
    } else {
        MMC_LOG_ERROR("HandleMetaReplicate unknown op: " << req.op_);
        resp.ret_ = MMC_ERROR;
    }

    return context->Reply(req.msgId, resp);
}

Result MetaNetClient::HandlePing(const NetContextPtr &context)
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

Result MetaNetClient::HandleLinkBroken(const NetLinkPtr &link)
{
    MMC_LOG_INFO(name_ << " link broken");
    for (uint32_t count = 0; count < retryCount_; count++) {
        Result ret = engine_->ConnectToPeer(rankId_, ip_, port_, link2Index_, false);
        if (ret != MMC_OK) {
            MMC_LOG_ERROR("MetaNetClient Connect " << ip_ << ", port " << port_ << " failed");
        } else {
            if (retryHandler_ != nullptr) {
                return retryHandler_();
            }
            return MMC_OK;
        }
    }
    return MMC_ERROR;
}
}
}