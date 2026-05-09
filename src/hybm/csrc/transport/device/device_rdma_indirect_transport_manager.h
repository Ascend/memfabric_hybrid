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

#ifndef MF_HYBRID_DEVICE_RDMA_INDIRECT_TRANSPORT_MANAGER_H
#define MF_HYBRID_DEVICE_RDMA_INDIRECT_TRANSPORT_MANAGER_H

#include <mutex>
#include <memory>
#include <vector>
#include <atomic>
#include <string>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <functional>
#include <utility>

#include <sys/syscall.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "hybm_va_manager.h"
#include "device_rdma_common.h"
#include "device_rdma_transport_manager.h"
#include "sender_side_queue.h"
#include "receiver_side_queue.h"
#include "device_rdma_helper.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

class RdmaIndirectTransportManager : public RdmaTransportManager {
public:
    RdmaIndirectTransportManager();
    ~RdmaIndirectTransportManager() override;

    Result OpenDevice(const TransportOptions &options) override;
    Result CloseDevice() override;
    Result RegisterMemoryRegion(const TransportMemoryRegion &mr) override;
    Result UnregisterMemoryRegion(uint64_t addr) override;
    bool QueryHasRegistered(uint64_t addr, uint64_t size) override;
    Result QueryMemoryKey(uint64_t addr, TransportMemoryKey &key) override;
    Result Prepare(const HybmTransPrepareOptions &options) override;
    Result RemoveRanks(const std::vector<uint32_t> &removedRanks) override;
    Result Connect() override;
    Result AsyncConnect() override;
    Result WaitForConnected(int64_t timeoutNs) override;
    Result UpdateRankOptions(const HybmTransPrepareOptions &options) override;
    const std::string &GetNic() const override;
    const void *GetQpInfo() const override;
    const TransportPrivateData GetPrivateData() const override;

    Result ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;
    Result WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;
    Result ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;
    Result WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size) override;
    Result Synchronize(uint32_t rankId) override;
    Result ReadRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor) override;
    Result WriteRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor) override;

private:
    struct Slice {
        uint64_t lAddr;
        uint64_t rAddr;
        uint64_t size;
        int type;
        uint32_t rankId;
        AddrType localMemType;
        Slice() : lAddr(0), rAddr(0), size(0), type(0), rankId(0), localMemType(ADDRESS_CATEGORY_BUTT) {}
    };

    struct SliceList {
        std::vector<Slice> slices;
        uint64_t enqueueTime{0};
    };

    struct PendingRequestContext {
        std::atomic<int> count{0};
        std::condition_variable cond;
        std::mutex mutex;
        const pid_t threadId ;
        PendingRequestContext() : threadId{static_cast<pid_t>(syscall(SYS_gettid))} {}
    };

    struct SendMessageContext {
        SliceList sliceList;
        std::shared_ptr<PendingRequestContext> pendingContext;
        SendMessageContext(SliceList sl, std::shared_ptr<PendingRequestContext> prc) noexcept
            : sliceList{std::move(sl)}, pendingContext{std::move(prc)}
        {}
    };

    struct ReceiveMessageContext {
        std::vector<void *> scatterAddrs;
        std::vector<void *> gatherAddrs;
        std::vector<uint64_t> counts;
        std::vector<uint64_t> offsets;
        uint64_t totalDataSize{0};
        uint64_t localRdmaAddr;
        int type{0};
        explicit ReceiveMessageContext(uint64_t localAddr) noexcept : localRdmaAddr{localAddr} {}
    };

    struct Phrase0Response {
        uint64_t enqueueTime{0};
        uint64_t rdmaAddress{0};
    };

    struct MergeResult {
        std::vector<void *> mergedSrc;
        std::vector<void *> mergedDst;
        std::vector<uint64_t> mergedCounts;
    };

    std::unordered_map<uint16_t, SendPhProcess> SenderPhraseProcessors() noexcept;
    std::unordered_map<uint16_t, RecvPhProcess> ReceiverPhraseProcessors() noexcept;
    int SenderSidePhrase0(const QueueMessage &res, QueueMessage &nextReq, bool &finished, void *ctx) noexcept;
    int SenderSidePhrase1(const QueueMessage &res, QueueMessage &nextReq, bool &finished, void *ctx) noexcept;
    int ReceiveSidePhrase0(const QueueMessage &request, QueueMessage &response) noexcept;
    int ReceiveSidePhrase1(const QueueMessage &request, QueueMessage &response) noexcept;
    QueueMessage GenerateInitRequest(SliceList &slices) noexcept;
    int SendInitRequestForSlices(SliceList &slices) noexcept;

    Result InitializeDirectLoop(); // 启动带外发送/接收线程/聚合线程
    void AcceptLoop();             // 本端带外发送任务
    void AcceptNewConnection();
    int ConnectToRemote(const std::string &nic, uint32_t remoteRankId, uint32_t localRankId);
    int InitListenerSocket(const std::string& nic);
    void DecrementPendingCount(const std::shared_ptr<PendingRequestContext> &pendingContext);
    int BatchCopy(std::vector<void *> &srcAddrs, std::vector<void *> &dstAddrs, std::vector<uint64_t> &counts,
                  uint32_t direction, void *stream = nullptr);
    int MergeBatchCopy(const std::vector<void *> &srcAddrs, const std::vector<void *> &dstAddrs,
                       const std::vector<uint64_t> &counts, MergeResult &result);

private:
    std::atomic<uint32_t> requestIdGen{};
    std::atomic_bool running_;        // 负责监控整个Manager的生命周期
    int gServerSocket_{};             // 接收侧fd，用于该Rank建立接收端，处理带外发送过来的数据
    std::thread outBandAcceptThread_; // 负责带外数据交互，接收端接收连接
    std::unique_ptr<RdmaTransportManager> rdmaTransportMgr_;
    std::shared_ptr<ThreadResourceContext> threadResourceContext_;
    SenderSideQueue senderSideQueue_;
    ReceiverSideQueue receiverSideQueue_;
    BlockingQueue sendBufferQueue_;
    BlockingQueue recvBufferQueue_;

    std::mutex receiveContextMutex_;
    std::unordered_map<uint64_t, ReceiveMessageContext *> receiveContexts_;

    void *buffer_{};
    std::condition_variable initiatorCond_;      // 请求队列条件变量
    int gOutBandEpollFd_{};                      // 用于处理接收任务得 fd
    uint16_t localPort_{};                       // 本地监听的端口
    const struct sockaddr *localNetworkStorage_; // 使用指针存储 sockaddr_storage，支持 IPv4 和 IPv6
    socklen_t localNetworkLen_;                  // 地址长度
    int localNetworkFamily_;                     // 地址族 (AF_INET or AF_INET6)
    std::string localNic_;
    std::vector<std::string> nics_;
    uint32_t rankCount_{0};
    uint32_t localRankId_{0};
    TransportMemoryKey swapMemKey_{};
    // 同步机制相关的成员变量
    static thread_local std::shared_ptr<PendingRequestContext> pendingRequestContext_;
};

} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock

#endif // MF_HYBRID_DEVICE_RDMA_INDIRECT_TRANSPORT_MANAGER_H