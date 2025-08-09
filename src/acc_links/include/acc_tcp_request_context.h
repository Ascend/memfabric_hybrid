/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef ACC_LINKS_ACC_TCP_REQUEST_CONTEXT_H
#define ACC_LINKS_ACC_TCP_REQUEST_CONTEXT_H

#include "acc_tcp_link.h"
#include "acc_tcp_shared_buf.h"

namespace ock {
namespace acc {
/**
 * @brief Context information passed by Handler functions, i.e. AccNewReqHandler which is triggered when server got a request from peer,
 * this is context is created by server, should be used created by user.
 *
 * In the handler (callback function), user can get many request information from context,
 * for example, messageType, dataLen, seqNo etc,
 * user can also reply message back using context directly,
 *
 * This context can be passed other thread for async reply.
 */
class ACC_API AccTcpRequestContext {
public:
    AccTcpRequestContext(const AccMsgHeader &h, const AccDataBufferPtr &d, const AccTcpLinkComplexPtr &l)
        : header_(h),
          data_(d),
          link_(l)
    {
    }

    /**
     * @brief Reply a message to peer with result
     *
     * @param result       [in] response result
     * @param d            [in] data to be response
     * @return 0 if successful
     */
    virtual int32_t Reply(int16_t result, const AccDataBufferPtr &d) const;

    /**
     * @brief Get message type
     *
     * @return message type
     */
    int16_t MsgType() const;

    /**
     * @brief Get data pointer
     *
     * @return pointer of request data
     */
    void* DataPtr() const;

    /**
     * @brief Get length of data
     *
     * @return length of data
     */
    uint32_t DataLen() const;

    /**
     * Get the seqNo
     *
     * @return seq no
     */
    uint32_t SeqNo() const;

    /**
     * @brief Get header sent from peer
     *
     * @return header
     */
    const AccMsgHeader &Header() const;

private:
    const AccMsgHeader header_;
    const AccDataBufferPtr data_;
    const AccTcpLinkComplexPtr link_;
};

inline int16_t AccTcpRequestContext::MsgType() const
{
    return header_.type;
}

inline void* AccTcpRequestContext::DataPtr() const
{
    if (data_.Get() == nullptr) {
        return nullptr;
    }
    return data_->DataPtrVoid();
}

inline uint32_t AccTcpRequestContext::DataLen() const
{
    if (data_.Get() == nullptr) {
        return 0;
    }
    return data_->DataLen();
}

inline uint32_t AccTcpRequestContext::SeqNo() const
{
    return header_.seqNo;
}

inline const AccMsgHeader &AccTcpRequestContext::Header() const
{
    return header_;
}
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_TCP_REQUEST_CONTEXT_H
