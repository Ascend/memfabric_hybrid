/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_NET_ENGINE_H
#define MEM_FABRIC_MMC_NET_ENGINE_H

#include <functional>

#include "mmc_common_includes.h"
#include "mmc_net_common.h"
#include "mmc_msg_packer.h"

namespace ock {
namespace mmc {

using NetReqReceivedHandler = std::function<int32_t(NetContextPtr &ctx)>;
using NetReqSentHandler = std::function<int32_t(NetContextPtr &ctx)>;
using NetNewLinkHandler = std::function<int32_t(const NetLinkPtr &link)>;
using NetLinkBrokenHandler = std::function<int32_t(const NetLinkPtr &link)>;

class NetContext : public MmcReferable {
public:
    ~NetContext() override = default;

    virtual int32_t Reply() = 0;

    virtual uint32_t SeqNo() = 0;
    virtual int16_t OpCode() = 0;
    virtual int16_t SrcRankId() = 0;
    virtual uint32_t DataLen() = 0;
    virtual void *Data() = 0;
};

class NetLink : public MmcReferable {
public:
    virtual int32_t Id() const = 0;

    uint64_t UpCtx() const
    {
        return upCtx_;
    }

    void UpCtx(const uint64_t c)
    {
        upCtx_ = c;
    }

private:
    uint64_t upCtx_ = 0;
};

class NetEngine : public MmcReferable {
public:
    static NetEnginePtr Create();

public:
    ~NetEngine() override = default;

    virtual Result Start(const NetEngineOptions &options) = 0;
    virtual void Stop() = 0;

    virtual Result ConnectToPeer(uint32_t peerId, const std::string &peerIp, uint16_t port, NetLinkPtr &newLink,
                                 bool isForce) = 0;

    void RegRequestReceivedHandler(int16_t opCode, const NetReqReceivedHandler &h);
    void RegRequestSentHandler(int16_t opCode, const NetReqSentHandler &h);
    void RegNewLinkHandler(const NetNewLinkHandler &h);
    void RegLinkBrokenHandler(const NetLinkBrokenHandler &h);

    virtual Result Call(uint32_t targetId, const char *reqData, uint32_t reqDataLen, char *respData,
                        uint32_t &respDataLen, int32_t timeoutInSecond) = 0;

    template <typename REQ, typename RESP>
    Result Call(uint32_t peerId, const REQ &req, RESP &resp, int32_t timeoutInSecond)
    {
        char *respData = nullptr;
        uint32_t respLen = 0;
        if (std::is_pod<REQ>::value && std::is_pod<RESP>::value) {
            /* do call */
            respLen = sizeof(RESP);
            return Call(peerId, static_cast<char *>(req), sizeof(REQ), static_cast<char *>(resp), respLen,
                        timeoutInSecond);
        } else if (std::is_pod<REQ>::value && !std::is_pod<RESP>::value) {
            /* do call */
            respLen = UINT32_MAX;
            auto result = Call(peerId, static_cast<char *>(req), sizeof(REQ), resp, respLen, timeoutInSecond);
            MMC_RETURN_NOT_OK(result);

            /* deserialize */
            NetMsgUnpacker unpacker({resp, respLen});
            result = resp->Deserialize(unpacker);
            MMC_LOG_ERROR_AND_RETURN_NOT_OK(result, "deserialize failed");

            return result;
        } else if (!std::is_pod<REQ>::value && std::is_pod<RESP>::value) {
            /* serialize request */
            NetMsgPacker packer;
            auto result = packer.Serialize(req);
            std::string serializedData = packer.String();

            /* do call */
            respLen = sizeof(RESP);
            return Call(peerId, serializedData.c_str(), serializedData.length(), static_cast<char *>(resp), respLen,
                        timeoutInSecond);
        } else {
            NetMsgPacker packer;
            auto result = packer.Serialize(req);
            std::string serializedData = packer.String();

            /* do call */
            respLen = sizeof(RESP);
            result = Call(peerId, serializedData.c_str(), serializedData.length(), resp, respLen, timeoutInSecond);
            MMC_RETURN_NOT_OK(result);

            /* deserialize */
            NetMsgUnpacker unpacker({resp, respLen});
            result = resp->Deserialize(unpacker);
            MMC_LOG_ERROR_AND_RETURN_NOT_OK(result, "deserialize failed");

            return result;
        }
    }

protected:
    constexpr static int16_t gHandlerMax = UN16;
    constexpr static int16_t gHandlerMin = 0;

protected:
    NetReqReceivedHandler reqReceivedHandlers_[gHandlerMax];
    NetReqSentHandler reqSentHandlers_[gHandlerMax];
    NetNewLinkHandler newLinkHandler_ = nullptr;
    NetLinkBrokenHandler linkBrokenHandler_ = nullptr;

    std::mutex mutex_;
};

inline void NetEngine::RegRequestReceivedHandler(int16_t opCode, const NetReqReceivedHandler &h)
{
    MMC_ASSERT_RET_VOID(h != nullptr);
    MMC_ASSERT_RET_VOID(opCode >= 0 && opCode < gHandlerMax);

    std::lock_guard<std::mutex> guard(mutex_);
    reqReceivedHandlers_[opCode] = h;
}

inline void NetEngine::RegRequestSentHandler(int16_t opCode, const NetReqSentHandler &h)
{
    MMC_ASSERT_RET_VOID(h != nullptr);
    MMC_ASSERT_RET_VOID(opCode >= 0 && opCode < gHandlerMax);

    std::lock_guard<std::mutex> guard(mutex_);
    reqSentHandlers_[opCode] = h;
}

inline void NetEngine::RegNewLinkHandler(const NetNewLinkHandler &h)
{
    MMC_ASSERT_RET_VOID(h != nullptr);

    std::lock_guard<std::mutex> guard(mutex_);
    newLinkHandler_ = h;
}

inline void NetEngine::RegLinkBrokenHandler(const NetLinkBrokenHandler &h)
{
    MMC_ASSERT_RET_VOID(h != nullptr);

    std::lock_guard<std::mutex> guard(mutex_);
    linkBrokenHandler_ = h;
}

}
}

#endif  //MEM_FABRIC_MMC_NET_ENGINE_H
