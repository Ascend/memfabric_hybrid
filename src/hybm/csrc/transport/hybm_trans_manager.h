/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#ifndef HYBM_TRANSPORT_H
#define HYBM_TRANSPORT_H

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
    virtual void CloseDevice(const TransHandlePtr h) = 0;

    virtual Result RegMemToDevice(const TransHandlePtr &h, const TransMemRegInput &in, TransMemRegOutput &out) = 0;
    virtual Result UnRegMemFromDevice(const TransMemRegOutput &out);

    virtual Result PrepareDataConn(const TransPrepareOptions &options) = 0;
    virtual void UnPrepareDataConn() = 0;

    virtual Result CreateDataConn(const TransDataConnOptions &options) = 0;
    virtual void CloseAllDataConn() = 0;
    virtual std::vector<TransDataConnPtr> GetDataConn() = 0;

    virtual TransDataConnAddressInfo GetDataConnAddrInfo() = 0;
};
}
}

#endif  //HYBM_TRANSPORT_H
