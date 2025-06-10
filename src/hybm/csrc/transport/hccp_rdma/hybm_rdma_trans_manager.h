/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_RDMA_TRANS_MANAGER_H
#define MF_HYBRID_HYBM_RDMA_TRANS_MANAGER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "hybm_trans_manager.h"

namespace ock {
namespace mf {
class RdmaTransportManager : public TransportManager {
public:
    TransHandlePtr OpenDevice(const TransDeviceOptions &options) override;
    void CloseDevice(const TransHandlePtr &h) override;
    Result RegMemToDevice(const TransHandlePtr &h, const TransMemRegInput &in, TransMemRegOutput &out) override;
    Result UnRegMemFromDevice(const TransMemRegOutput &out) override;
    Result PrepareDataConn(const TransPrepareOptions &options) override;
    void UnPrepareDataConn() override;
    Result CreateDataConn(const TransDataConnOptions &options) override;
    void CloseAllDataConn() override;
    std::vector<TransDataConnPtr> GetDataConn() override;
    TransDataConnAddressInfo GetDataConnAddrInfo() override;

private:
    bool OpenTsd();
    bool RaInit();
    bool RetireDeviceIp();
    bool RaRdevInit();

private:
    uint32_t deviceId_;
    struct in_addr deviceIp_;
    void *rdmaHandle_{nullptr};
};
}
}

#endif  // MF_HYBRID_HYBM_RDMA_TRANS_MANAGER_H
