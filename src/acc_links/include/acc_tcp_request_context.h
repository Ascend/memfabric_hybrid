/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ACC_LINKS_ACC_TCP_REQUEST_CONTEXT_H
#define ACC_LINKS_ACC_TCP_REQUEST_CONTEXT_H

#include "acc_tcp_link.h"
#include "acc_tcp_shared_buf.h"

namespace ock {
namespace acc {
/**
 * @brief Context information passed by Handler functions, i.e. AccNewReqHandler which is triggered
 * when server got a request from peer, this is context is created by server, should be used created by user.
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

    AccTcpRequestContext(const AccTcpRequestContext &b): header_(b.header_), link_(b.link_)
    {
        data_ = AccDataBuffer::Create(b.DataPtr(), b.DataLen());
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

    /**
     * @brief Get link
     *
     * @return link
     */
    const AccTcpLinkComplexPtr &Link() const;

private:
    const AccMsgHeader header_;
    const AccTcpLinkComplexPtr link_;
    AccDataBufferPtr data_;
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

inline const AccTcpLinkComplexPtr &AccTcpRequestContext::Link() const
{
    return link_;
}
}  // namespace acc
}  // namespace ock

#endif  // ACC_LINKS_ACC_TCP_REQUEST_CONTEXT_H
