/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/
#ifndef ACC_LINKS_ACC_TCP_LINK_COMPLEX_DEFAULT_H
#define ACC_LINKS_ACC_TCP_LINK_COMPLEX_DEFAULT_H

#include <list>
#include <utility>

#include "acc_tcp_link_default.h"

namespace ock {
namespace acc {
class AccTcpWorker;

/**
 * @brief Message node of message queue
 */
struct AccLinkedMessageNode {
    AccLinkedMessageNode *next = nullptr;
    AccMsgHeader header{};
    AccDataBufferPtr data{nullptr};
    AccDataBufferPtr cbCtx{nullptr};
    uint32_t headerRemain = sizeof(AccMsgHeader);
    uint32_t dataRemain = 0;

    AccLinkedMessageNode() = default;

    AccLinkedMessageNode(const AccMsgHeader &h, const AccDataBufferPtr &d, const AccDataBufferPtr &ctx)
        : header(h), data(d), cbCtx(ctx), dataRemain{d->DataLen()}
    {}

    inline bool HeaderSent() const
    {
        return headerRemain == 0;
    }

    inline bool DataSent() const
    {
        return dataRemain == 0;
    }

    inline bool Sent() const
    {
        return headerRemain == 0 && dataRemain == 0;
    }

    inline void *HeaderPtrToBeSend() const
    {
        auto baseHeaderPtr = reinterpret_cast<uintptr_t>(&header);
        return reinterpret_cast<void *>(baseHeaderPtr + (sizeof(AccMsgHeader) - headerRemain));
    }

    inline void *DataPtrToBeSend() const
    {
        return reinterpret_cast<void *>(data->DataIntPtr() + (data->DataLen() - dataRemain));
    }

    inline bool HeaderAllSent(uint32_t size)
    {
        if (headerRemain <= size) {
            headerRemain = 0;
            return true;
        }
        headerRemain -= size;
        return false;
    }

    inline bool DataAllSent(uint32_t size)
    {
        if (dataRemain <= size) {
            dataRemain = 0;
            return true;
        }
        dataRemain -= size;
        return false;
    }
};

/**
 * @brief Message queue using linked list
 */
class AccLinkedMessageQueue : public AccReferable {
public:
    explicit AccLinkedMessageQueue(uint32_t queueCap) : sizeCap_(queueCap) {}

    ~AccLinkedMessageQueue() override
    {
        /* set the tmp node */
        AccLinkedMessageNode *tmpNode = nullptr;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            tmpNode = headNode_;
            headNode_ = nullptr;
            tailNode_ = nullptr;
            size_ = 0;
        }

        if (tmpNode == nullptr) {
            return;
        }

        /* loop and delete */
        while (tmpNode != nullptr) {
            auto nodeToBeDelete = tmpNode;
            tmpNode = tmpNode->next;
            delete nodeToBeDelete;
            nodeToBeDelete = nullptr;
        }
    }

    uint32_t GetSize()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return size_;
    }

    /**
     * @brief Enqueue a header and data buffer into queue on the back
     *
     * @param h            [in] header
     * @param d            [in] data buffer ptr
     * @return 0 if successful, ACC_QUEUE_IS_FULL if full
     */
    Result EnqueueBack(const AccMsgHeader &h, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx)
    {
        ASSERT_RETURN(d.Get() != nullptr, ACC_INVALID_PARAM);

        /* create new node */
        auto tmpNode = new (std::nothrow) AccLinkedMessageNode(h, d, cbCtx);
        ASSERT_RETURN(tmpNode != nullptr, ACC_NEW_OBJECT_FAIL);
        tmpNode->dataRemain = d->DataLen();

        {
            /* check and add */
            std::lock_guard<std::mutex> guard(mutex_);
            if (size_ >= sizeCap_) {
                delete tmpNode;
                tmpNode = nullptr;
                return ACC_QUEUE_IS_FULL;
            }

            /* if the empty */
            if (headNode_ == nullptr) {
                headNode_ = tmpNode;
                tailNode_ = tmpNode;
                ++size_;
                return ACC_OK;
            }

            /* if not empty */
            auto currentTail = tailNode_;
            tailNode_ = tmpNode;
            currentTail->next = tmpNode;
            ++size_;
            return ACC_OK;
        }
    }

    /**
     * @brief Dequeue a node from front
     *
     * @return node ptr if not empty, nullptr if empty
     */
    AccLinkedMessageNode *DequeueFront()
    {
        /* check and */
        std::lock_guard<std::mutex> guard(mutex_);
        if (size_ == 0) {
            return nullptr;
        }

        auto tmpNode = headNode_;
        /* only one node */
        if (size_ == 1) {
            headNode_ = nullptr;
            tailNode_ = nullptr;
            --size_;
            return tmpNode;
        }

        /* a lot of nodes */
        headNode_ = tmpNode->next;
        --size_;
        return tmpNode;
    }

    /**
     * @brief Push a node back on front place, ignore the cap
     *
     * @param node         [in] node to be pushed front
     * @return 0 if successful
     */
    Result EnqueueFront(AccLinkedMessageNode *node)
    {
        ASSERT_RETURN(node != nullptr, ACC_INVALID_PARAM);

        std::lock_guard<std::mutex> guard(mutex_);
        /* no need to consider the cap */

        /* if the empty */
        if (headNode_ == nullptr) {
            headNode_ = node;
            tailNode_ = node;
            ++size_;
            return ACC_OK;
        }

        /* if not empty */
        node->next = headNode_;
        headNode_ = node;
        ++size_;
        return ACC_OK;
    }

    /**
     * @brief Take away all messages in the queue
     *
     * @return Linked message node
     */
    inline AccLinkedMessageNode *TakeAwayMessages()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        size_ = 0;
        auto tmpNode = headNode_;
        headNode_ = nullptr;
        tailNode_ = nullptr;
        return tmpNode;
    }

private:
    uint32_t sizeCap_ = UNO_256;               /* cap of the send queue */
    uint32_t size_ = 0;                        /* size */
    AccLinkedMessageNode *headNode_ = nullptr; /* headerNode of message */
    AccLinkedMessageNode *tailNode_ = nullptr; /* headerNode of message */
    std::mutex mutex_;                         /* send queue mutex */
};
using AccLinkedMessageQueuePtr = AccRef<AccLinkedMessageQueue>;

/**
 * @brief Complex link for work polling in non-blocking mode
 */
class AccTcpLinkComplexDefault : public AccTcpLinkDefault {
public:
    AccTcpLinkComplexDefault(int fd, std::string ipPort, uint32_t id, SSL *ssl = nullptr)
        : AccTcpLinkDefault(fd, std::move(ipPort), id, ssl)
    {}

    ~AccTcpLinkComplexDefault() override
    {
        UnInitialize();
    }

    Result Initialize(uint16_t sendQueueCap, int32_t workIndex, AccTcpWorker *worker);
    void UnInitialize();

    Result NonBlockSend(int16_t msgType, const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx) override;
    Result NonBlockSend(int16_t msgType, uint32_t seqNo, const AccDataBufferPtr &d,
                        const AccDataBufferPtr &cbCtx) override;
    Result NonBlockSend(int16_t msgType, int16_t opCode, uint32_t seqNo, const AccDataBufferPtr &d,
                        const AccDataBufferPtr &cbCtx) override;

    Result EnqueueAndModifyEpoll(const AccMsgHeader &h, const AccDataBufferPtr &d,
                                 const AccDataBufferPtr &cbCtx) override;

protected:
    AccLinkedMessageNode *DequeueFront() noexcept;
    Result EnqueueFront(AccLinkedMessageNode *node) noexcept;

    AccLinkedMessageNode *TakeAwayMessages();

    ssize_t PollInRecv(void *ptr, ssize_t len) noexcept;
    ssize_t PollOutWrite(void *ptr, ssize_t len) noexcept;
    Result HandlePollIn() noexcept;
    Result HandlePollOut(AccMsgHeader &header, AccDataBufferPtr &cbCtx) noexcept;
    Result SendPostProcess(int32_t errorNumber) noexcept;

protected:
    AccLinkReceiveState receiveState_{};      /* state of receiving message for worker polling only */
    AccMsgHeader header_{};                   /* header to be received for worker polling only */
    AccDataBufferPtr data_{nullptr};          /* data being received for worker polling only */
    AccLinkedMessageQueuePtr queue_{nullptr}; /* send message queue */
    std::atomic<uint32_t> seqNo_{0};          /* seqNo */
    uint32_t workerIndex_ = 0;                /* attached to which worker */
    AccTcpWorker *worker_ = nullptr;

    friend class AccTcpWorker;
    friend class AccTcpRequestContext;
    friend class AccTcpServerDefault;
};
using AccTcpLinkComplexDefaultPtr = AccRef<AccTcpLinkComplexDefault>;

inline AccLinkedMessageNode *AccTcpLinkComplexDefault::DequeueFront() noexcept
{
    ASSERT_RETURN(queue_.Get() != nullptr, nullptr);
    return queue_->DequeueFront();
}

inline Result AccTcpLinkComplexDefault::EnqueueFront(AccLinkedMessageNode *node) noexcept
{
    ASSERT_RETURN(queue_.Get() != nullptr, ACC_NOT_INITIALIZED);
    return queue_->EnqueueFront(node);
}

inline AccLinkedMessageNode *AccTcpLinkComplexDefault::TakeAwayMessages()
{
    ASSERT_RETURN(queue_.Get() != nullptr, nullptr);
    return queue_->TakeAwayMessages();
}

inline ssize_t AccTcpLinkComplexDefault::PollInRecv(void *ptr, ssize_t len) noexcept
{
    if (LIKELY(ssl_ == nullptr)) {
        return ::recv(fd_, ptr, len, 0);
    } else {
        return OpenSslApiWrapper::SslRead(ssl_, ptr, len);
    }
}

inline ssize_t AccTcpLinkComplexDefault::PollOutWrite(void *ptr, ssize_t len) noexcept
{
    if (LIKELY(ssl_ == nullptr)) {
        return ::write(fd_, ptr, len);
    } else {
        return OpenSslApiWrapper::SslWrite(ssl_, ptr, len);
    }
}

inline Result AccTcpLinkComplexDefault::HandlePollIn() noexcept
{
    const auto headDataPtr = reinterpret_cast<uintptr_t>(&header_);

    /* receive header */
    ssize_t result = 0;
    if (receiveState_.ShouldReceiveHeader()) {
        result = PollInRecv(reinterpret_cast<void *>(headDataPtr + receiveState_.ReceivedHeaderLen()),
                            receiveState_.headerToBeReceived);
        if (LIKELY((result) > 0)) {
            if (receiveState_.HeaderSatisfied(result)) { /* header is full, continue to receive body */
                // validate header
                if (UNLIKELY(!data_->AllocIfNeed(header_.bodyLen))) {
                    LOG_ERROR("Failed to expand receive buffer to " << header_.bodyLen << ", probably out of memory");
                    receiveState_.ResetHeader();
                    return ACC_MALLOC_FAIL;
                }
                receiveState_.bodyToBeReceived = header_.bodyLen; /* expand memory size */
                data_->SetDataSize(0);
            } else { /* header is not fully, need to continue to receive */
                return ACC_LINK_EAGAIN;
            }
        } else {                            /* ECONNRESET is broken during io, SUCCESS is broken during idle time. */
            const auto errorNumber = errno; // avoid errno writed by log
            if (errorNumber == ECONNRESET || errorNumber == 0) {
                LOG_INFO("Link " << id_ << " receive header failed, reset by peer, errno " << errorNumber);
                return ACC_LINK_ERROR; /* socket is closed by peer, socket is error */
            }
            /* if errno is eagain is normal, need to continue to receive */
            /* else meaning failed to read from socket, socket is error */
            if (errorNumber != EAGAIN) {
                LOG_ERROR("Link " << id_ << " receive header failed, errno " << errorNumber);
            }

            return (errorNumber == EAGAIN ? ACC_LINK_EAGAIN : ACC_LINK_ERROR);
        }
    }

    /* receive body */
    auto dataPtr = data_->DataIntPtr() + (header_.bodyLen - static_cast<size_t>(receiveState_.bodyToBeReceived));
    result = PollInRecv(reinterpret_cast<void *>(dataPtr), receiveState_.bodyToBeReceived);
    if (LIKELY((result) > 0)) {
        if (receiveState_.BodySatisfied(result)) { /* body is full */
            receiveState_.ResetHeader();
            data_->SetDataSize(header_.bodyLen);
            return ACC_LINK_MSG_READY; /* message fully received, we can do the upper call */
        }

        LOG_DEBUG("Receive sock " << id_ << " not full body size: " << receiveState_.bodyToBeReceived);
        /* body is not fully received, continue to receive */
        return ACC_LINK_EAGAIN;
    } else {                            /* ECONNRESET is broken during io, SUCCESS is broken during idle time. */
        const auto errorNumber = errno; // avoid errno writed by log
        if (errorNumber == ECONNRESET || errorNumber == 0) {
            LOG_INFO("Link " << id_ << " receive body failed, reset by peer, errno " << errorNumber);
            return ACC_LINK_ERROR; /* socket is closed by peer, socket is error */
        }
        /* if errno is eagain is normal, need to continue to receive */
        /* else meaning failed to read from socket,socket is error */
        if (errorNumber != EAGAIN) {
            LOG_ERROR("Link " << id_ << " receive body failed, errno " << errorNumber);
        }

        return (errorNumber == EAGAIN ? ACC_LINK_EAGAIN : ACC_LINK_ERROR);
    }
}

inline Result AccTcpLinkComplexDefault::HandlePollOut(AccMsgHeader &header, AccDataBufferPtr &cbCtx) noexcept
{
    ASSERT_RETURN(queue_.Get() != nullptr, ACC_NOT_INITIALIZED);
    AccLinkedMessageNode *oneMsg = queue_->DequeueFront();
    if (UNLIKELY(oneMsg == nullptr)) {
        return ACC_OK;
    }

    ASSERT_RETURN(!oneMsg->Sent(), ACC_OK);
    header = oneMsg->header;
    cbCtx = oneMsg->cbCtx;

    /* send header if not sent */
    if (!oneMsg->HeaderSent()) {
        auto result = PollOutWrite(oneMsg->HeaderPtrToBeSend(), oneMsg->headerRemain);
        if (LIKELY(result > 0)) {
            if (!oneMsg->HeaderAllSent(result)) { /* not all sent */
                queue_->EnqueueFront(oneMsg);
                return ACC_LINK_EAGAIN;
            }
            /* if no data body send finished */
            if (oneMsg->DataSent()) {
                delete oneMsg;
                oneMsg = nullptr;
                return ACC_LINK_MSG_SENT;
            }

            /* continue to send data part */
        } else {
            delete oneMsg;
            oneMsg = nullptr;
            return SendPostProcess(errno);
        }
    }

    /* send data if not sent */
    if (!oneMsg->DataSent()) {
        auto result = PollOutWrite(oneMsg->DataPtrToBeSend(), oneMsg->dataRemain);
        if (LIKELY(result > 0)) {
            if (!oneMsg->DataAllSent(result)) { /* not all sent */
                queue_->EnqueueFront(oneMsg);
                return ACC_LINK_EAGAIN;
            }

            delete oneMsg;
            oneMsg = nullptr;
            return ACC_LINK_MSG_SENT;
        } else {
            delete oneMsg;
            oneMsg = nullptr;
            return SendPostProcess(errno);
        }
    }

    delete oneMsg;
    oneMsg = nullptr;
    return ACC_OK;
}

inline Result AccTcpLinkComplexDefault::SendPostProcess(int32_t errorNumber) noexcept
{
    if (errorNumber == ECONNRESET) {
        LOG_ERROR("Failed to send msg to peer in link " << id_ << ", reset by peer");
        return ACC_LINK_ERROR;
    }

    if (errorNumber == EAGAIN) { /* send buff is full not send */
        return ACC_LINK_EAGAIN;
    }

    LOG_ERROR("Failed to send msg to peer in link " << id_ << ", errno " << errorNumber);
    return ACC_LINK_ERROR;
}

inline Result AccTcpLinkComplexDefault::NonBlockSend(int16_t msgType, const AccDataBufferPtr &d,
                                                     const AccDataBufferPtr &cbCtx)
{
    ASSERT_RETURN(msgType >= MIN_MSG_TYPE && msgType < MAX_MSG_TYPE, ACC_INVALID_PARAM);
    ASSERT_RETURN(d.Get() != nullptr, ACC_INVALID_PARAM);

    if (UNLIKELY(!Established())) {
        LOG_ERROR_LIMIT("Failed to send message with message type " << msgType << " as the link is broken");
        return ACC_LINK_ERROR;
    }

    return EnqueueAndModifyEpoll({msgType, d->DataLen(), seqNo_++}, d, cbCtx);
}

inline Result AccTcpLinkComplexDefault::NonBlockSend(int16_t msgType, uint32_t seqNo, const AccDataBufferPtr &d,
                                                     const AccDataBufferPtr &cbCtx)
{
    ASSERT_RETURN(msgType >= MIN_MSG_TYPE && msgType < MAX_MSG_TYPE, ACC_INVALID_PARAM);
    ASSERT_RETURN(d.Get() != nullptr, ACC_INVALID_PARAM);

    if (UNLIKELY(!Established())) {
        LOG_ERROR_LIMIT("Failed to send message with message type " << msgType << " as the link is broken");
        return ACC_LINK_ERROR;
    }

    return EnqueueAndModifyEpoll({msgType, d->DataLen(), seqNo}, d, cbCtx);
}

inline Result AccTcpLinkComplexDefault::NonBlockSend(int16_t msgType, int16_t opCode, uint32_t seqNo,
                                                     const AccDataBufferPtr &d, const AccDataBufferPtr &cbCtx)
{
    ASSERT_RETURN(msgType >= MIN_MSG_TYPE && msgType < MAX_MSG_TYPE, ACC_INVALID_PARAM);
    ASSERT_RETURN(d.Get() != nullptr, ACC_INVALID_PARAM);

    if (UNLIKELY(!Established())) {
        LOG_ERROR("Failed to send message with message type " << msgType << " as the link is broken");
        return ACC_LINK_ERROR;
    }
    return EnqueueAndModifyEpoll({msgType, opCode, d->DataLen(), seqNo}, d, cbCtx);
}
} // namespace acc
} // namespace ock

#endif // ACC_LINKS_ACC_TCP_LINK_COMPLEX_DEFAULT_H