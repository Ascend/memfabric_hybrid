/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef MEM_FABRIC_MMC_NET_ENGINE_H
#define MEM_FABRIC_MMC_NET_ENGINE_H

#include <functional>

#include "mmc_common_includes.h"
#include "mmc_net_common.h"
#include "mmc_msg_packer.h"

namespace ock {
namespace mmc {
/**
 * @brief Callback function for request received
 */
using NetReqReceivedHandler = std::function<int32_t(NetContextPtr &ctx)>;

/**
 * @brief Callback function for request sent
 */
using NetReqSentHandler = std::function<int32_t(NetContextPtr &ctx)>;

/**
 * @brief Callback function for new link
 */
using NetNewLinkHandler = std::function<int32_t(const NetLinkPtr &link)>;

/**
 * @brief Call function for link broken
 */
using NetLinkBrokenHandler = std::function<int32_t(const NetLinkPtr &link)>;

class NetContext : public MmcReferable {
public:
    ~NetContext() override = default;

    virtual int32_t Reply(int16_t responseCode, char *respData, uint32_t &respDataLen) = 0;

    /**
     * @brief Get seq number of request
     *
     * @return seq number
     */
    virtual uint32_t SeqNo() const = 0;

    /**
     * @brief Get op code of request
     *
     * @return op code
     */
    virtual int16_t OpCode() const = 0;

    /**
     * @brief Get rank id where the request sent
     *
     * @return rank id of source
     */
    virtual int16_t SrcRankId() const = 0;

    /**
     * @brief Data length of request
     *
     * @return length of data
     */
    virtual uint32_t DataLen() const = 0;

    /**
     * @brief Data pointer of request
     *
     * @return data pointer
     */
    virtual void *Data() const = 0;

    template<typename RESP>
    int32_t Reply(int16_t opCode, RESP &resp)
    {
        if (std::is_pod<RESP>::value) {
            uint32_t retSize = sizeof(RESP);
            return Reply(opCode, (char*)(&resp), retSize);
        } else {
            NetMsgPacker packer;
            resp.Serialize(packer);
            std::string serializedData = packer.String();
            uint32_t retSize = serializedData.length();
            return Reply(opCode, const_cast<char* >(serializedData.c_str()), retSize);
        }
    }

    template<typename REQ>
    int32_t GetRequest(REQ &req)
    {
        if (std::is_pod<REQ>::value) {
            std::copy_n(reinterpret_cast<char *>(Data()), DataLen(), reinterpret_cast<char *>(&req));
        } else {
            std::string str{(char *)Data(), DataLen()};
            NetMsgUnpacker unpacker(str);
            req.Deserialize(unpacker);
        }
        return MMC_OK;
    }
};

class NetLink : public MmcReferable {
public:
    /**
     * @brief Get id of net link
     *
     * @return id of net link
     */
    virtual int32_t Id() const = 0;

    /**
     * @brief Get the context value which associated with this link
     *
     * @return Context value
     */
    uint64_t UpCtx() const
    {
        return upCtx_;
    }

    /**
     * @brief Set the context value which associated with this link
     *
     * @param c            [in] context value to be set
     */
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

    /**
     * @brief Start the net engine for RPC or IPC
     *
     * @param options        [in] options of the engine
     * @return 0 if successful
     */
    virtual Result Start(const NetEngineOptions &options) = 0;

    /**
     * @brief Stop the net engine
     */
    virtual void Stop() = 0;

    /**
     * @brief Create a link to peer server
     *
     * @param peerId       [in] id of peer server
     * @param peerIp       [in] ip of peer server listen at
     * @param port         [in] port of peer server listen at
     * @param newLink      [in/out] new linked created
     * @param isForce      [in] force connect
     * @return 0 if successful
     */
    virtual Result ConnectToPeer(uint32_t peerId, const std::string &peerIp, uint16_t port, NetLinkPtr &newLink,
                                 bool isForce) = 0;

    /**
     * @brief Register callback function of request received
     *
     * @param opCode       [in] op code for the handler
     * @param h            [in] handler to be registered
     */
    void RegRequestReceivedHandler(int16_t opCode, const NetReqReceivedHandler &h);

    /**
     * @brief Register callback function of request sent
     *
     * @param opCode       [in] op code for the handler
     * @param h            [in] handler to be registered
     */
    void RegRequestSentHandler(int16_t opCode, const NetReqSentHandler &h);

    /**
     * @brief Register callback function of new link connected
     *
     * @param h            [in] handler to be registered
     */
    void RegNewLinkHandler(const NetNewLinkHandler &h);

    /**
     * @brief Register callback function of link broken
     *
     * @param h            [in] handler to be registered
     */
    void RegLinkBrokenHandler(const NetLinkBrokenHandler &h);

    /**
     * @brief Do RPC/IPC call to peer in sync way, wait for response back
     *
     * @tparam REQ         [in] type of request
     * @tparam RESP        [in] type of response
     * @param peerId       [in] peer id
     * @param req          [in] request
     * @param resp         [in/out] response
     * @param userResult   [in/out] result from peer, if call returned 0
     * @param timeoutInSecond [in] timeout in second
     * @return
     */
    template <typename REQ, typename RESP>
    Result Call(uint32_t peerId, int16_t opCode, const REQ &req, RESP &resp, int32_t timeoutInSecond)
    {
        char *respData = nullptr;
        uint32_t respLen = 0;
        if (std::is_pod<REQ>::value && std::is_pod<RESP>::value) {
            /* do call */
            respLen = sizeof(RESP);
            respData = reinterpret_cast<char *>(&resp);
            return Call(peerId, opCode, reinterpret_cast<char *>(const_cast<REQ *>(&req)), sizeof(REQ), &respData,
                        respLen, timeoutInSecond);
        } else if (std::is_pod<REQ>::value && !std::is_pod<RESP>::value) {
            /* do call */
            respLen = UINT32_MAX;
            respData = nullptr;
            auto result = Call(peerId, opCode, reinterpret_cast<char *>(const_cast<REQ *>(&req)), sizeof(REQ),
                               &respData, respLen, timeoutInSecond);
            MMC_RETURN_ERROR(result, "NetEngine call error, op " << opCode << ", peerId " << peerId);

            /* deserialize */
            std::string respStr(respData, respLen);
            NetMsgUnpacker unpacker(respStr);
            result = resp.Deserialize(unpacker);
            if (respData != nullptr) {
                free(respData);
            }
            MMC_RETURN_ERROR(result, "deserialize failed");

            return result;
        } else if (!std::is_pod<REQ>::value && std::is_pod<RESP>::value) {
            /* serialize request */
            NetMsgPacker packer;
            req.Serialize(packer);
            std::string serializedData = packer.String();

            /* do call */
            respLen = sizeof(RESP);
            respData = reinterpret_cast<char *>(&resp);
            return Call(peerId, opCode, serializedData.c_str(), serializedData.length(), &respData, respLen,
                        timeoutInSecond);
        } else {
            NetMsgPacker packer;
            req.Serialize(packer);
            std::string serializedData = packer.String();

            /* do call */
            respLen = sizeof(RESP);
            respData = nullptr;
            Result result = Call(peerId, opCode, serializedData.c_str(), serializedData.length(), &respData, respLen,
                                 timeoutInSecond);
            MMC_RETURN_ERROR(result, "NetEngine call error, op " << opCode << ", peerId " << peerId);

            /* deserialize */
            std::string respStr(respData, respLen);
            if (respData != nullptr) {
                free(respData);
            }
            NetMsgUnpacker unpacker(respStr);
            result = resp.Deserialize(unpacker);
            MMC_RETURN_ERROR(result, "deserialize failed");

            return result;
        }
    }

    /**
     * @brief Do RPC/IPC call to peer in sync way, wait for response back
     *
     * @tparam REQ         [in] type of request
     * @param peerId       [in] peer id
     * @param req          [in] request to send
     * @param timeoutInSecond [in] timeout in second
     * @return 0 if successful
     */
    template <typename REQ>
    Result Send(uint32_t peerId, const REQ &req, int32_t timeoutInSecond)
    {
        if (std::is_pod<REQ>::value) {
            /* do send */
            return Send(peerId, static_cast<char *>(req), sizeof(REQ), timeoutInSecond);
        } else {
            /* serialize request */
            NetMsgPacker packer;
            auto result = packer.Serialize(req);
            const std::string serializedData = packer.String();

            /* do send */
            return Send(peerId, serializedData.c_str(), serializedData.length(), timeoutInSecond);
        }
    }

    /**
     * @brief Do RPC/IPC call to peer in sync way, wait for response back
     *
     * @param peerId       [in] peer id
     * @param reqData      [in] data to be sent to target
     * @param reqDataLen   [in] data len to be sent to target
     * @param respData     [in/out] data replied by target
     * @param respDataLen  [in/out] data length replied
     * @param timeoutInSecond [in] timeout in second
     * @return 0 if successful, MMC_TIMEOUT for timeout, negative value for inner local size error
     */
    virtual Result Call(uint32_t targetId, int16_t opCode, const char *reqData, uint32_t reqDataLen, char **respData,
                        uint32_t &respDataLen, int32_t timeoutInSecond) = 0;

    /**
     * @brief Send a request to peer with response
     *
     * @param peerId       [in] peer id
     * @param reqData      [in] request data to be sent
     * @param reqDataLen   [in] data length
     * @param timeoutInSecond [in] timeout in second
     * @return 0 if successful
     */
    virtual Result Send(uint32_t peerId, const char *reqData, uint32_t reqDataLen, int32_t timeoutInSecond) = 0;

protected:
    constexpr static int16_t gHandlerMax = UN32;
    constexpr static int16_t gHandlerMin = 0;

protected:
    int16_t gHandlerSize = 0;
    NetReqReceivedHandler reqReceivedHandlers_[gHandlerMax];
    NetReqSentHandler reqSentHandlers_[gHandlerMax];
    NetNewLinkHandler newLinkHandler_ = nullptr;
    NetLinkBrokenHandler linkBrokenHandler_ = nullptr;

    std::mutex mutex_;
};

inline void NetEngine::RegRequestReceivedHandler(int16_t opCode, const NetReqReceivedHandler &h)
{
    MMC_ASSERT_RET_VOID(opCode >= 0 && opCode < gHandlerMax);

    std::lock_guard<std::mutex> guard(mutex_);
    reqReceivedHandlers_[opCode] = h;
    gHandlerSize++;
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

#endif  // MEM_FABRIC_MMC_NET_ENGINE_H
