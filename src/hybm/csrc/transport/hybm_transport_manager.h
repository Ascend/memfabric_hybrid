/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef MF_HYBRID_HYBM_TRANSPORT_MANAGER_H
#define MF_HYBRID_HYBM_TRANSPORT_MANAGER_H

#include <memory>
#include "hybm_types.h"
#include "hybm_transport_common.h"

namespace ock {
namespace mf {
namespace transport {

class TransportManager {
public:
    static std::shared_ptr<TransportManager> Create(TransportType type);

public:
    TransportManager()= default;

    virtual ~TransportManager() = default;

    /*
     * 1、本地IP（NIC、Device）
     * @return 0 if successful
     */
    virtual Result OpenDevice(const TransportOptions &options) = 0;

    virtual Result CloseDevice() = 0;

    virtual Result ConnectWithOptions(const HybmTransPrepareOptions &options);

    /*
     * 2、注册内存
     * @return 0 if successful
     */
    virtual Result RegisterMemoryRegion(const TransportMemoryRegion &mr) = 0;

    virtual Result UnregisterMemoryRegion(uint64_t addr) = 0;

    virtual bool QueryHasRegistered(uint64_t addr, uint64_t size) = 0;

    virtual Result QueryMemoryKey(uint64_t addr, TransportMemoryKey &key) = 0;

    virtual Result ParseMemoryKey(const TransportMemoryKey &key, uint64_t &addr, uint64_t &size) = 0;

    /*
     * 3、建链前的准备工作
     * @return 0 if successful
     */
    virtual Result Prepare(const HybmTransPrepareOptions &options) = 0;

    /*
     * 建链完成状态，删除一部分节点
     */
    virtual Result RemoveRanks(const std::vector<uint32_t> &removedRanks) = 0;

    /*
     * 4、建链
     * @return 0 if successful
     */
    virtual Result Connect() = 0;

    /*
     * 异步建链
     * @return 0 if successful
     */
    virtual Result AsyncConnect() = 0;

    /*
     * 等待异步建链完成
     * @return 0 if successful
     */
    virtual Result WaitForConnected(int64_t timeoutNs) = 0;

    /*
     * 建链完成后，更新rank配置信息，可以新增rank或减少rank
     */
    virtual Result UpdateRankOptions(const HybmTransPrepareOptions &options) = 0;

    /**
     * 查询
     */
    virtual const std::string &GetNic() const = 0;  // X

    virtual const void *GetQpInfo() const;

    /**
      * rdma单边传输
      */
    virtual Result ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) = 0;

    virtual Result WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) = 0;

    virtual Result ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) = 0;

    virtual Result WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) = 0;

    virtual Result Synchronize(uint32_t rankId) = 0;

protected:
    bool connected_{false};
};

using TransManagerPtr = std::shared_ptr<TransportManager>;
}
}
}

#endif  // MF_HYBRID_HYBM_TRANSPORT_MANAGER_H
