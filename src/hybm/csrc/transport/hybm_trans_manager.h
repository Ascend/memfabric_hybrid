/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef HYBM_TRANSPORT_H
#define HYBM_TRANSPORT_H

#include <chrono>
#include "hybm_common_include.h"
#include "hybm_trans_common.h"

namespace ock {
namespace mf {
class TransportManager {
public:
    static TransportManagerPtr Create(TransType t);

public:
    virtual ~TransportManager() = default;

    virtual TransHandlePtr OpenDevice(const TransDeviceOptions &options) = 0;
    virtual void CloseDevice(const TransHandlePtr &h) = 0;

    virtual Result RegMemToDevice(const TransHandlePtr &h, const TransMemRegInput &in, TransMemRegOutput &out) = 0;
    virtual Result UnRegMemFromDevice(const TransMemRegOutput &out) = 0;

    virtual Result PrepareDataConn(const TransPrepareOptions &options) = 0;
    virtual void UnPrepareDataConn() = 0;

    virtual Result CreateDataConn(const TransDataConnOptions &options) = 0;
    virtual void CloseAllDataConn() = 0;
    virtual bool IsReady() = 0;

    template<class R, class P>
    Result WaitingReady(const std::chrono::duration<R, P> &timeout)
    {
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout);
        return WaitingReady(ns);
    }
    
    virtual Result WaitingReady(int64_t timeoutNs) = 0;
    virtual std::vector<TransDataConnPtr> GetDataConn() = 0;

    virtual TransDataConnAddressInfo GetDataConnAddrInfo() = 0;
};
}
}

#endif  //HYBM_TRANSPORT_H
