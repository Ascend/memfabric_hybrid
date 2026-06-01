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
#include "device_rdma_indirect_transport_manager.h"

#include <unistd.h>
#include <chrono>
#include <cstring>
#include <thread>
#include <regex>
#include <numeric>

#include "hybm_common_include.h"
#include "dl_acl_api.h"
#include "hybm_ptracer.h"
#include "hybm_stream_manager.h"
#include "device_rdma_common.h"
#include "device_rdma_helper.h"
#include "hybm_va_manager.h"
#include "mf_ipv4_validator.h"


namespace ock {
namespace mf {
namespace transport {
namespace device {

class DeviceThreadContext : public ThreadContext {
public:
    explicit DeviceThreadContext(uint32_t deviceId) noexcept : deviceId_(deviceId) {}
    int ThreadStartup() noexcept override
    {
        return HybmStreamManager::SetDevice(deviceId_);
    }
    void ThreadShutdown() noexcept override {}

private:
    uint32_t deviceId_;
};

constexpr int READ = 0;
constexpr int WRITE = 1;

constexpr int MAX_RETRIES = 3;
constexpr int MAX_EVENTS = 100;
constexpr int MAX_CONNECT = 1024;
constexpr int RETRY_DELAY_MS = 100;

constexpr uint64_t AGGREGATE_SIZE_LIMIT = 16UL * 1024UL * 1024UL;
constexpr uint64_t SEND_BUFFER_COUNT = 1;
constexpr uint64_t RECV_BUFFER_COUNT = 32;

thread_local std::shared_ptr<RdmaIndirectTransportManager::PendingRequestContext>
    RdmaIndirectTransportManager::pendingRequestContext_ = nullptr;
RdmaIndirectTransportManager::RdmaIndirectTransportManager()
    : running_(false), senderSideQueue_{8U, SenderPhraseProcessors()},
      receiverSideQueue_{4U, ReceiverPhraseProcessors()},
      localNetworkStorage_(nullptr), localNetworkLen_(0), localNetworkFamily_(AF_INET)
{
    BM_LOG_DEBUG("RdmaIndirectTransportManager created.");
}

RdmaIndirectTransportManager::~RdmaIndirectTransportManager()
{
    // Call CloseDevice to ensure all resources are properly cleaned up
    // This serves as a safety net in case user forgets to call CloseDevice
    if (running_ || gServerSocket_ >= 0 || gOutBandEpollFd_ >= 0 || buffer_ != nullptr) {
        BM_LOG_WARN("RdmaIndirectTransportManager destructor called without CloseDevice, cleaning up resources");
        RdmaIndirectTransportManager::CloseDevice();
    }
}

void RdmaIndirectTransportManager::DecrementPendingCount(const std::shared_ptr<PendingRequestContext> &pendingContext)
{
    auto currCount = --pendingContext->count;
    BM_LOG_DEBUG("Decrement pending count : " << currCount);

    if (currCount <= 0) {
        pendingContext->cond.notify_one();
        BM_LOG_DEBUG("All requests completed for thread: " << pendingContext->threadId);
    }
}

int RdmaIndirectTransportManager::MergeBatchCopy(const std::vector<void *> &srcAddrs,
                                                 const std::vector<void *> &dstAddrs,
                                                 const std::vector<uint64_t> &counts, MergeResult &result)
{
    result.mergedSrc.clear();
    result.mergedDst.clear();
    result.mergedCounts.clear();

    if (srcAddrs.empty() || srcAddrs.size() != dstAddrs.size() || srcAddrs.size() != counts.size()) {
        BM_LOG_ERROR("Invalid input");
        return -1;
    }

    // 第一条记录直接加入
    void *currSrc = srcAddrs[0];
    void *currDst = dstAddrs[0];
    uint64_t currCount = counts[0];

    for (size_t i = 1; i < srcAddrs.size(); ++i) {
        // 检查是否可以合并：src 和 dst 都连续
        const auto nextSrc = reinterpret_cast<uintptr_t>(srcAddrs[i]);
        const auto nextDst = reinterpret_cast<uintptr_t>(dstAddrs[i]);

        const bool srcContinuous = (nextSrc == reinterpret_cast<uintptr_t>(currSrc) + currCount);
        const bool dstContinuous = (nextDst == reinterpret_cast<uintptr_t>(currDst) + currCount);

        if (srcContinuous && dstContinuous) {
            // 可以合并，累加长度
            currCount += counts[i];
        } else {
            // 不能合并，先保存当前段
            result.mergedSrc.push_back(currSrc);
            result.mergedDst.push_back(currDst);
            result.mergedCounts.push_back(currCount);

            // 开始新的段
            currSrc = srcAddrs[i];
            currDst = dstAddrs[i];
            currCount = counts[i];
        }
    }

    result.mergedSrc.push_back(currSrc);
    result.mergedDst.push_back(currDst);
    result.mergedCounts.push_back(currCount);
    return 0;
}

int RdmaIndirectTransportManager::BatchCopy(std::vector<void *> &srcAddrs1, std::vector<void *> &dstAddrs1,
                                            std::vector<uint64_t> &counts1, uint32_t direction, void *stream)
{
    MergeResult mergeResult;
    Result ret = MergeBatchCopy(srcAddrs1, dstAddrs1, counts1, mergeResult);
    if (ret != 0) {
        BM_LOG_ERROR("merge addr failed");
        return -1;
    }

    if (stream == nullptr) {
        stream = HybmStreamManager::GetThreadAclStream();
    }

    size_t batchNum = mergeResult.mergedSrc.size();

    for (size_t i = 0; i < batchNum; ++i) {
        auto destAddr = mergeResult.mergedDst[i];
        auto srcAddr = mergeResult.mergedSrc[i];
        auto count = mergeResult.mergedCounts[i];

        ret = DlAclApi::AclrtMemcpyAsync(destAddr, count, srcAddr, count, direction, stream);
        if (ret != 0) {
            BM_LOG_ERROR("copy memory on local failed: " << ret << " stream:" << reinterpret_cast<uintptr_t>(stream)
                                                         << " direct:" << direction << std::hex << " src:" << srcAddr
                                                         << " dst:" << destAddr);
            return -1;
        }
    }
    ret = DlAclApi::AclrtSynchronizeStream(stream);
    if (ret != 0) {
        BM_LOG_ERROR("aclrtSynchronizeStream failed: " << ret << " stream:" << reinterpret_cast<uintptr_t>(stream));
    }
    return ret;
}

void RdmaIndirectTransportManager::AcceptNewConnection()
{
    BM_LOG_INFO("AcceptNewConnection accept  start");
    sockaddr_storage clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    int clientFd = accept(gServerSocket_, reinterpret_cast<sockaddr *>(&clientAddr), &addrLen);
    if (clientFd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 没有等待的连接，正常情况
            return;
        }
        BM_LOG_ERROR("AcceptNewConnection accept failed, errno: " << errno << ", error: " << strerror(errno));
        return;
    }

    receiverSideQueue_.AddAcceptSocket(clientFd);
    BM_LOG_INFO("AcceptNewConnection accepted new connection from fd: " << clientFd);
}

void RdmaIndirectTransportManager::AcceptLoop()
{
    epoll_event events[MAX_EVENTS];
    pthread_setname_np(pthread_self(), "ind_accept_th");
    BM_LOG_INFO("AcceptLoop ind_accept_th start.");
    while (running_) {
        int nfds = epoll_wait(gOutBandEpollFd_, events, MAX_EVENTS, 100);
        if (nfds <= 0) {
            continue;
        }
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == gServerSocket_) {
                AcceptNewConnection();
            }
        }
    }
    BM_LOG_INFO("AcceptLoop ind_accept_th exit.");
}

static std::string ModifyNicPort(const std::string &nic, uint16_t newPort)
{
    UrlParser urlParser;
    if (!urlParser.Initialize(nic)) {
        return "";
    }
    
    std::string ip = urlParser.GetIp();
    if (urlParser.IsIpv6()) {
        return "tcp://[" + ip + "]:" + std::to_string(newPort);
    } else {
        return "tcp://" + ip + ":" + std::to_string(newPort);
    }
}

int RdmaIndirectTransportManager::InitListenerSocket(const std::string& nic)
{
    UrlParser urlParser;
    if (!urlParser.Initialize(nic)) {
        BM_LOG_ERROR("parse input nic(" << nic << ") failed using UrlParser!");
        return BM_INVALID_PARAM;
    }

    localPort_ = 0;

    // 修改端口后需要重新生成地址信息
    std::string modifiedNic = ModifyNicPort(nic, localPort_);
    UrlParser modifiedUrlParser;
    if (!modifiedUrlParser.Initialize(modifiedNic)) {
        BM_LOG_ERROR("parse modified nic(" << modifiedNic << ") failed using UrlParser!");
        return BM_INVALID_PARAM;
    }
    
    // 使用 sockaddr_storage 来存储地址，支持 IPv4 和 IPv6
    localNetworkStorage_ = modifiedUrlParser.GetSockAddr();
    localNetworkLen_ = modifiedUrlParser.GetAddrLen();
    localNetworkFamily_ = modifiedUrlParser.GetAddressFamily();
    
    gServerSocket_ = socket(localNetworkFamily_, SOCK_STREAM, 0);
    if (gServerSocket_ < 0) {
        BM_LOG_ERROR("RdmaIndirectTransportManager out-of-band socket create failed, family: " << localNetworkFamily_);
        return gServerSocket_;
    }
    int optval = 1;
    int ret = setsockopt(gServerSocket_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (ret < 0) {
        BM_LOG_ERROR("RdmaIndirectTransportManager set sock opt failed, ret:" << ret);
        close(gServerSocket_);
        return ret;
    }

    ret = bind(gServerSocket_, localNetworkStorage_, localNetworkLen_);
    if (ret < 0) {
        BM_LOG_ERROR("RdmaIndirectTransportManager bind failed on the default port, default port: " << localPort_);
        close(gServerSocket_);
        return ret;
    }

    ret = listen(gServerSocket_, MAX_CONNECT);
    if (ret != BM_OK) {
        BM_LOG_ERROR("RdmaIndirectTransportManager listen Failed, ret: " << ret);
        close(gServerSocket_);
        return ret;
    }

    // 获取系统实际分配的端口
    sockaddr_in addr{};
    addr.sin_family = localNetworkFamily_;
    addr.sin_addr.s_addr = INADDR_ANY; // 或 inet_addr("127.0.0.1")
    addr.sin_port = 0;                 // 0 = 让内核自动分配可用端口
    socklen_t len = sizeof(addr);
    if (getsockname(gServerSocket_, reinterpret_cast<sockaddr *>(&addr), &len) == -1) {
        BM_LOG_ERROR("get getsockname failed!");
        close(gServerSocket_);
        return -1;
    }
    int port = ntohs(addr.sin_port);
    localPort_ = port;

    gOutBandEpollFd_ = epoll_create1(0);
    if (gOutBandEpollFd_ == -1) {
        BM_LOG_ERROR("RdmaIndirectTransportManager epoll fd target create Failed, ret: " << ret);
        close(gServerSocket_);
        return gOutBandEpollFd_;
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = gServerSocket_;
    ret = epoll_ctl(gOutBandEpollFd_, EPOLL_CTL_ADD, gServerSocket_, &ev);
    if (ret != BM_OK) {
        BM_LOG_ERROR("RdmaIndirectTransportManager epoll_ctl add gOutBandEpollFd_ failed:" << ret);
        close(gServerSocket_);
        gServerSocket_ = -1;
        return -1;
    }
    BM_LOG_INFO("RdmaIndirectTransportManager listen: " << nic << ", port:" << localPort_ << ", family:"
                                                        << localNetworkFamily_ << ", modify:" << modifiedNic);
    return BM_OK;
}

Result RdmaIndirectTransportManager::WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_LOG_INFO("start WriteRemoteAsync started for remote rank " << rankId << ", size: " << size << ",lAddr:" << lAddr
                                                                  << ", rAddr:" << rAddr);
    if (pendingRequestContext_ == nullptr) {
        pendingRequestContext_ = std::make_shared<PendingRequestContext>();
    }

    pendingRequestContext_->count++;
    SliceList slices;
    auto &slice_list = slices.slices;
    slice_list.reserve(1);
    Slice slice;
    slice.lAddr = lAddr;
    slice.rAddr = rAddr;
    slice.size = size;
    slice.type = WRITE;
    slice.rankId = rankId;
    slice.localMemType = HybmVaManager::GetInstance().ClassifyAddress(slice.lAddr);

    slice_list.push_back(slice);
    auto ret = SendInitRequestForSlices(slices);
    if (ret != BM_OK) {
        BM_LOG_ERROR("WriteRemoteAsync started for rank " << rankId << ", size: " << size << " failed: " << ret);
        pendingRequestContext_->count--;
        return ret;
    }

    BM_LOG_INFO("WriteRemoteAsync started for rank " << rankId << ", size: " << size);
    return BM_OK;
}

Result RdmaIndirectTransportManager::ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_LOG_INFO("start ReadRemoteAsync started for rank " << rankId << ", size: " << size);
    if (pendingRequestContext_ == nullptr) {
        pendingRequestContext_ = std::make_shared<PendingRequestContext>();
    }

    pendingRequestContext_->count++;
    SliceList slices;
    auto &slice_list = slices.slices;
    slice_list.reserve(1);

    Slice slice;
    slice.lAddr = lAddr;
    slice.rAddr = rAddr;
    slice.size = size;
    slice.type = READ;
    slice.rankId = rankId;
    slice.localMemType = HybmVaManager::GetInstance().ClassifyAddress(slice.lAddr);

    slice_list.push_back(slice);
    auto ret = SendInitRequestForSlices(slices);
    if (ret != BM_OK) {
        BM_LOG_ERROR("ReadRemoteAsync started for rank " << rankId << ", size: " << size << " failed: " << ret);
        pendingRequestContext_->count--;
        return ret;
    }

    BM_LOG_INFO("ReadRemoteAsync started for rank " << rankId << ", size: " << size);
    return BM_OK;
}

Result RdmaIndirectTransportManager::ReadRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor)
{
    if (descriptor.localAddrs.size() != descriptor.globalAddrs.size() ||
        descriptor.localAddrs.size() != descriptor.counts.size()) {
        BM_LOG_ERROR("ReadRemoteBatchAsync: vector sizes mismatch, local="
                     << descriptor.localAddrs.size() << ", global=" << descriptor.globalAddrs.size()
                     << ", counts=" << descriptor.counts.size());
        return BM_INVALID_PARAM;
    }

    if (descriptor.localAddrs.empty()) {
        BM_LOG_WARN("ReadRemoteBatchAsync: empty descriptor for rank " << rankId);
        return BM_OK;
    }

    if (pendingRequestContext_ == nullptr) {
        pendingRequestContext_ = std::make_shared<PendingRequestContext>();
    }
    pendingRequestContext_->count++;
    size_t requestCount = descriptor.localAddrs.size();

    SliceList slices;
    auto &slice_list = slices.slices;
    slice_list.reserve(requestCount);

    for (size_t i = 0; i < requestCount; ++i) {
        Slice slice;
        slice.lAddr = reinterpret_cast<uint64_t>(descriptor.localAddrs[i]);
        slice.rAddr = reinterpret_cast<uint64_t>(descriptor.globalAddrs[i]);
        slice.size = descriptor.counts[i];
        slice.type = READ;
        slice.rankId = rankId;
        slice.localMemType = HybmVaManager::GetInstance().ClassifyAddress(slice.lAddr);

        if (slice.size == 0) {
            BM_LOG_WARN("ReadRemoteBatchAsync: zero-sized request at index " << i << " for rank " << rankId);
        }

        slice_list.push_back(slice);
        BM_LOG_DEBUG("ReadRemoteBatchAsync: added slice[" << i << "], lAddr=0x" << std::hex << slice.lAddr
            << ", rAddr=0x" << std::hex << slice.rAddr << ", size=" << slice.size
            << ", localMemType=" << slice.localMemType);
    }

    auto ret = SendInitRequestForSlices(slices);
    if (ret != BM_OK) {
        BM_LOG_ERROR("ReadRemoteBatchAsync started for rank " << rankId << " failed: " << ret);
        pendingRequestContext_->count--;
        return ret;
    }

    BM_LOG_DEBUG("ReadRemoteBatchAsync started for rank "
                 << rankId << ", batch size: " << requestCount
                 << ", total bytes: " << std::accumulate(descriptor.counts.begin(), descriptor.counts.end(), 0ULL));
    return BM_OK;
}

Result RdmaIndirectTransportManager::WriteRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor)
{
    if (descriptor.localAddrs.size() != descriptor.globalAddrs.size() ||
        descriptor.localAddrs.size() != descriptor.counts.size()) {
        BM_LOG_ERROR("WriteRemoteBatchAsync: vector sizes mismatch, local="
                     << descriptor.localAddrs.size() << ", global=" << descriptor.globalAddrs.size()
                     << ", counts=" << descriptor.counts.size());
        return BM_INVALID_PARAM;
    }

    if (descriptor.localAddrs.empty()) {
        BM_LOG_WARN("WriteRemoteBatchAsync: empty descriptor for rank " << rankId);
        return BM_OK;
    }

    size_t requestCount = descriptor.localAddrs.size();
    if (pendingRequestContext_ == nullptr) {
        pendingRequestContext_ = std::make_shared<PendingRequestContext>();
    }
    pendingRequestContext_->count++;

    SliceList sliceList;
    sliceList.slices.reserve(requestCount);
    for (size_t i = 0; i < requestCount; ++i) {
        Slice slice;
        slice.lAddr = reinterpret_cast<uint64_t>(descriptor.localAddrs[i]);
        slice.rAddr = reinterpret_cast<uint64_t>(descriptor.globalAddrs[i]);
        slice.size = descriptor.counts[i];
        slice.type = WRITE;
        slice.rankId = rankId;
        slice.localMemType = HybmVaManager::GetInstance().ClassifyAddress(slice.lAddr);

        if (slice.size == 0) {
            BM_LOG_WARN("WriteRemoteBatchAsync: zero-sized request at index " << i << " for rank " << rankId);
        }

        sliceList.slices.push_back(slice);
        BM_LOG_DEBUG("WriteRemoteBatchAsync: added slice[" << i << "], lAddr=0x" << std::hex << slice.lAddr
            << ", rAddr=0x" << std::hex << slice.rAddr << ", size=" << slice.size
            << ", localMemType=" << slice.localMemType);
    }

    auto ret = SendInitRequestForSlices(sliceList);
    if (ret != BM_OK) {
        BM_LOG_ERROR("WriteRemoteBatchAsync started for rank " << rankId << " failed: " << ret);
        pendingRequestContext_->count--;
        return ret;
    }

    BM_LOG_DEBUG("WriteRemoteBatchAsync started for rank "
                 << rankId << ", batch size: " << requestCount
                 << ", total bytes: " << std::accumulate(descriptor.counts.begin(), descriptor.counts.end(), 0ULL));
    return BM_OK;
}

QueueMessage RdmaIndirectTransportManager::GenerateInitRequest(SliceList &slices) noexcept
{
    QueueMessage initRequest;
    initRequest.head.request = 1U;
    initRequest.head.opCode = 0;
    initRequest.head.srcRankId = localRankId_;
    initRequest.head.dstRankId = slices.slices[0].rankId;
    initRequest.head.bodySize = sizeof(slices.enqueueTime) + slices.slices.size() * sizeof(Slice);
    initRequest.head.requestId = (static_cast<uint64_t>(localRankId_) << 32U) + requestIdGen.fetch_add(1UL);
    initRequest.head.timestamp = TP_CURRENT_TIME_NS;
    initRequest.body.resize(initRequest.head.bodySize);
    auto dest = initRequest.body.data();
    std::copy_n(reinterpret_cast<uint8_t*>(&slices.enqueueTime), sizeof(slices.enqueueTime), dest);
    dest += sizeof(slices.enqueueTime);
    for (auto &slice : slices.slices) {
        std::copy_n(reinterpret_cast<uint8_t*>(&slice), sizeof(Slice), dest);
        dest += sizeof(Slice);
    }
    return std::move(initRequest);
}

int RdmaIndirectTransportManager::SendInitRequestForSlices(SliceList &slices) noexcept
{
    auto context = new (std::nothrow) SendMessageContext(slices, pendingRequestContext_);
    if (context == nullptr) {
        BM_LOG_ERROR("create send side context failed.");
        return BM_ERROR;
    }

    auto initRequest = GenerateInitRequest(slices);
    auto ret = senderSideQueue_.BeginRequest(std::move(initRequest), context);
    if (ret != BM_OK) {
        delete context;
        BM_LOG_ERROR("WriteRemoteBatchAsync started for rank " << initRequest.head.dstRankId << " failed: " << ret);
        return ret;
    }

    return BM_OK;
}

std::unordered_map<uint16_t, SendPhProcess> RdmaIndirectTransportManager::SenderPhraseProcessors() noexcept
{
    std::unordered_map<uint16_t, SendPhProcess> processors;
    processors.emplace(0, [this](const QueueMessage &res, QueueMessage &nextReq, bool &finished, void *ctx) {
        return SenderSidePhrase0(res, nextReq, finished, ctx);
    });
    processors.emplace(1, [this](const QueueMessage &res, QueueMessage &nextReq, bool &finished, void *ctx) {
        return SenderSidePhrase1(res, nextReq, finished, ctx);
    });
    return processors;
}

std::unordered_map<uint16_t, RecvPhProcess> RdmaIndirectTransportManager::ReceiverPhraseProcessors() noexcept
{
    std::unordered_map<uint16_t, RecvPhProcess> processors;
    processors.emplace(0, [this](const QueueMessage &request, QueueMessage &response) {
        return ReceiveSidePhrase0(request, response);
    });
    processors.emplace(1, [this](const QueueMessage &request, QueueMessage &response) {
        return ReceiveSidePhrase1(request, response);
    });
    return processors;
}

int RdmaIndirectTransportManager::SenderSidePhrase0(const QueueMessage &res, QueueMessage &nextReq, bool &finished,
                                                    void *ctx) noexcept
{
    int ret;
    uint32_t traceId;
    uint64_t timestamp = 0;

    auto context = static_cast<SendMessageContext *>(ctx);
    if (res.head.errorCode != 0) {
        BM_LOG_ERROR("phase0 response failed for message: " << res.head);
        DecrementPendingCount(context->pendingContext);
        finished = true;
        delete context;
        return BM_ERROR;
    }

    auto stream = HybmStreamManager::GetThreadAclStream();
    auto respBody = static_cast<const Phrase0Response *>(static_cast<const void *>(res.body.data()));
    auto rtRank = res.head.dstRankId;
    if (context->sliceList.slices[0].type == READ) {
        TP_TRACE_TRACE_BEGIN(TP_INDIRECT_SENDER_PHASE_0_R, &timestamp)
        traceId = TP_INDIRECT_SENDER_PHASE_0_R;
    } else {
        TP_TRACE_TRACE_BEGIN(TP_INDIRECT_SENDER_PHASE_0_W, &timestamp)
        traceId = TP_INDIRECT_SENDER_PHASE_0_W;
    }

    nextReq.head = res.head;
    nextReq.head.request = 1;
    nextReq.head.opCode = 1;
    nextReq.head.bodySize = 0;
    finished = false;

    uint64_t localRdmaAddr;
    if (!sendBufferQueue_.Pop(localRdmaAddr)) {
        TP_TRACE_TRACE_END(traceId, timestamp, BM_MALLOC_FAILED)
        BM_LOG_ERROR("allocate sender side local rdma buffer failed.");
        return BM_MALLOC_FAILED;
    }

    uint64_t totalDataSize = 0;
    std::vector<void *> scatterAddrs;
    std::vector<void *> gatherAddrs;
    std::vector<uint64_t> counts;
    for (auto &slice : context->sliceList.slices) {
        scatterAddrs.push_back((void *)(slice.lAddr));
        gatherAddrs.push_back((void *)(localRdmaAddr + totalDataSize));
        counts.push_back(slice.size);
        totalDataSize += slice.size;
    }

    if (context->sliceList.slices[0].type == WRITE) {
        uint32_t copyDir = (context->sliceList.slices[0].localMemType == LOCAL_HOST)
            ? ACL_MEMCPY_HOST_TO_DEVICE : ACL_MEMCPY_DEVICE_TO_DEVICE;
        TP_TRACE_BEGIN(TP_INDIRECT_SENDER_PHASE_0_W_D2D);
        ret = BatchCopy(scatterAddrs, gatherAddrs, counts, copyDir, stream);
        TP_TRACE_END(TP_INDIRECT_SENDER_PHASE_0_W_D2D, ret);
        if (ret != BM_OK) {
            TP_TRACE_TRACE_END(traceId, timestamp, ret)
            sendBufferQueue_.Push(localRdmaAddr);
            BM_LOG_ERROR("sender phase0 sync failed, ret: " << ret << ", remote rank:" << rtRank
                                                            << ", local_rank:" << localRankId_);
            return ret;
        }
        TP_TRACE_BEGIN(TP_INDIRECT_SENDER_PHASE_0_W_D2R);
        ret = RdmaTransportManager::WriteRemote(rtRank, localRdmaAddr, respBody->rdmaAddress, totalDataSize);
        TP_TRACE_END(TP_INDIRECT_SENDER_PHASE_0_W_D2R, ret);
        if (ret != BM_OK) {
            sendBufferQueue_.Push(localRdmaAddr);
            BM_LOG_ERROR("sender phase0 WriteRemote failed, rankId: " << res.head.srcRankId << ", remote rank:"
                                                                      << rtRank << ", local_rank:" << localRankId_);
            return ret;
        }
    } else {
        TP_TRACE_BEGIN(TP_INDIRECT_SENDER_PHASE_0_R_R2D);
        ret = RdmaTransportManager::ReadRemote(rtRank, localRdmaAddr, respBody->rdmaAddress, totalDataSize);
        TP_TRACE_END(TP_INDIRECT_SENDER_PHASE_0_R_R2D, ret);
        if (ret != BM_OK) {
            TP_TRACE_TRACE_END(traceId, timestamp, ret)
            sendBufferQueue_.Push(localRdmaAddr);
            BM_LOG_ERROR("sender phase0 ReadRemote failed, rankId: " << res.head.srcRankId << ", remote rank:" << rtRank
                                                                     << ", local_rank:" << localRankId_);
            return ret;
        }
        uint32_t copyDir = (context->sliceList.slices[0].localMemType == LOCAL_HOST)
            ? ACL_MEMCPY_DEVICE_TO_HOST : ACL_MEMCPY_DEVICE_TO_DEVICE;
        TP_TRACE_BEGIN(TP_INDIRECT_SENDER_PHASE_0_R_D2D);
        ret = BatchCopy(gatherAddrs, scatterAddrs, counts, copyDir, stream);
        TP_TRACE_END(TP_INDIRECT_SENDER_PHASE_0_R_D2D, ret);
        if (ret != BM_OK) {
            TP_TRACE_TRACE_END(traceId, timestamp, ret)
            sendBufferQueue_.Push(localRdmaAddr);
            BM_LOG_ERROR("sender phase0 sync failed, ret: " << ret << ", remote rank:" << rtRank
                                                            << ", local_rank:" << localRankId_);
            return ret;
        }
    }

    sendBufferQueue_.Push(localRdmaAddr);
    TP_TRACE_TRACE_END(traceId, timestamp, BM_OK)
    return BM_OK;
}

int RdmaIndirectTransportManager::SenderSidePhrase1(const QueueMessage &res, QueueMessage &nextReq, bool &finished,
                                                    void *ctx) noexcept
{
    uint32_t traceId;
    uint64_t timestamp = 0;
    (void)nextReq;
    auto context = static_cast<SendMessageContext *>(ctx);
    if (context->sliceList.slices[0].type == READ) {
        TP_TRACE_TRACE_BEGIN(TP_INDIRECT_SENDER_PHASE_1_R, &timestamp)
        traceId = TP_INDIRECT_SENDER_PHASE_1_R;
    } else {
        TP_TRACE_TRACE_BEGIN(TP_INDIRECT_SENDER_PHASE_1_W, &timestamp)
        traceId = TP_INDIRECT_SENDER_PHASE_1_W;
    }

    finished = true;
    DecrementPendingCount(context->pendingContext);
    delete context;
    TP_TRACE_TRACE_END(traceId, timestamp, BM_OK)
    return BM_OK;
}

int RdmaIndirectTransportManager::ReceiveSidePhrase0(const QueueMessage &request, QueueMessage &response) noexcept
{
    uint32_t traceId;
    uint64_t localBaseAddr;
    uint64_t timestamp = 0;
    auto reqBody = request.body.data();
    auto slices = static_cast<const Slice *>(static_cast<const void *>(reqBody + sizeof(uint64_t)));
    auto sliceCount = (request.head.bodySize - sizeof(uint64_t)) / sizeof(Slice);
    if (slices[0].type == READ) {
        TP_TRACE_TRACE_BEGIN(TP_INDIRECT_RECEIVER_PHASE_0_R, &timestamp)
        traceId = TP_INDIRECT_RECEIVER_PHASE_0_R;
    } else {
        TP_TRACE_TRACE_BEGIN(TP_INDIRECT_RECEIVER_PHASE_0_W, &timestamp)
        traceId = TP_INDIRECT_RECEIVER_PHASE_0_W;
    }

    response.head = request.head;
    response.head.request = 0;
    response.head.bodySize = 0;
    if (!recvBufferQueue_.Pop(localBaseAddr)) {
        TP_TRACE_TRACE_END(traceId, timestamp, BM_MALLOC_FAILED)
        response.head.errorCode = static_cast<int8_t>(-1);
        BM_LOG_ERROR("allocate receive buffer failed.");
        return BM_MALLOC_FAILED;
    }

    auto context = new (std::nothrow) ReceiveMessageContext(localBaseAddr);
    if (context == nullptr) {
        TP_TRACE_TRACE_END(traceId, timestamp, BM_MALLOC_FAILED)
        recvBufferQueue_.Push(localBaseAddr);
        response.head.errorCode = static_cast<int8_t>(-1);
        BM_LOG_ERROR("receive side allocate context failed!");
        return BM_MALLOC_FAILED;
    }

    context->offsets.resize(sliceCount);
    for (auto i = 0U; i < sliceCount; i++) {
        uint64_t hva = HybmVaManager::GetInstance().TransformVa(slices[i].rAddr, HVM_GVA, HVM_HVA);
        uint64_t scatterAddr = (hva != 0) ? hva : slices[i].rAddr;
        BM_LOG_DEBUG("original addr: 0x" << std::hex << slices[i].rAddr << ", transformed addr: 0x" << hva);

        context->offsets[i] = context->totalDataSize;
        context->totalDataSize += slices[i].size;
        context->scatterAddrs.emplace_back((void *)scatterAddr);
        context->gatherAddrs.emplace_back((void *)(localBaseAddr + context->offsets[i]));
        context->counts.push_back(slices[i].size);
    }

    if (context->totalDataSize > AGGREGATE_SIZE_LIMIT) {
        TP_TRACE_TRACE_END(traceId, timestamp, BM_ERROR)
        recvBufferQueue_.Push(localBaseAddr);
        delete context;
        response.head.errorCode = static_cast<int8_t>(-1);
        BM_LOG_ERROR("HandleIncomingTask total data size "
                     << context->totalDataSize << "slice_count:" << sliceCount << " exceeds limit "
                     << AGGREGATE_SIZE_LIMIT << ", fd: " << request.head.socketFd
                     << "remote rank_id:" << slices[0].rankId << ",local_rank:" << localRankId_);
        return BM_ERROR;
    }

    if ((context->type = slices[0].type) == READ) {
        int ret;
        TP_TRACE_BEGIN(TP_INDIRECT_RECEIVER_PHASE_0_R_H2D);
        ret = BatchCopy(context->scatterAddrs, context->gatherAddrs, context->counts, ACL_MEMCPY_HOST_TO_DEVICE);
        TP_TRACE_END(TP_INDIRECT_RECEIVER_PHASE_0_R_H2D, ret);
        if (ret != 0) {
            TP_TRACE_TRACE_END(traceId, timestamp, BM_ERROR)
            recvBufferQueue_.Push(localBaseAddr);
            response.head.errorCode = static_cast<int8_t>(-1);
            delete context;
            BM_LOG_ERROR("HandleIncomingTask sync failed, ret: " << ret << ", fd: " << request.head.socketFd
                                                                 << ", remote rank_id: " << slices[0].rankId
                                                                 << ", local_rank: " << localRankId_);
            return BM_ERROR;
        }
    }

    std::unique_lock<std::mutex> locker{receiveContextMutex_};
    receiveContexts_.emplace(request.head.requestId, context);
    locker.unlock();

    response.head.bodySize = sizeof(Phrase0Response);
    response.body.resize(sizeof(Phrase0Response));

    auto resBody = static_cast<Phrase0Response *>(static_cast<void *>(response.body.data()));
    resBody->enqueueTime = *static_cast<const uint64_t *>(static_cast<const void *>(reqBody));
    resBody->rdmaAddress = localBaseAddr;
    std::copy_n(&localBaseAddr, sizeof(uint64_t), response.body.data());
    TP_TRACE_TRACE_END(traceId, timestamp, BM_OK)

    return BM_OK;
}

int RdmaIndirectTransportManager::ReceiveSidePhrase1(const QueueMessage &request, QueueMessage &response) noexcept
{
    uint32_t traceId;
    uint64_t timestamp = 0;
    response.head = request.head;
    response.head.request = 0;
    response.head.bodySize = 0;

    std::unique_lock<std::mutex> locker{receiveContextMutex_};
    auto pos = receiveContexts_.find(request.head.requestId);
    if (pos == receiveContexts_.end()) {
        locker.unlock();
        response.head.errorCode = static_cast<int8_t>(-1);
        BM_LOG_ERROR("phrase1 request: " << request.head << " context not found.");
        return BM_ERROR;
    }
    auto context = pos->second;
    receiveContexts_.erase(pos);
    locker.unlock();
    if (context->type == READ) {
        TP_TRACE_TRACE_BEGIN(TP_INDIRECT_RECEIVER_PHASE_1_R, &timestamp)
        traceId = TP_INDIRECT_RECEIVER_PHASE_1_R;
    } else {
        TP_TRACE_TRACE_BEGIN(TP_INDIRECT_RECEIVER_PHASE_1_W, &timestamp)
        traceId = TP_INDIRECT_RECEIVER_PHASE_1_W;
    }

    if (context->type == WRITE) {
        TP_TRACE_BEGIN(TP_INDIRECT_RECEIVER_PHASE_1_W_D2H);
        int ret = BatchCopy(context->gatherAddrs, context->scatterAddrs, context->counts, ACL_MEMCPY_DEVICE_TO_HOST);
        TP_TRACE_END(TP_INDIRECT_RECEIVER_PHASE_1_W_D2H, ret);
        if (ret != 0) {
            TP_TRACE_TRACE_END(traceId, timestamp, BM_ERROR)
            recvBufferQueue_.Push(context->localRdmaAddr);
            delete context;
            response.head.errorCode = static_cast<int8_t>(-1);
            BM_LOG_ERROR("HandleIncomingTask sync failed, ret: " << ret << ", fd: " << request.head.socketFd
                                                                 << "remote rank_id" << request.head.srcRankId
                                                                 << "local_rank:" << localRankId_);
            return BM_ERROR;
        }
    }

    recvBufferQueue_.Push(context->localRdmaAddr);
    delete context;
    TP_TRACE_TRACE_END(traceId, timestamp, BM_OK)
    return BM_OK;
}

Result RdmaIndirectTransportManager::InitializeDirectLoop()
{
    pid_t pid = getpid();
    int32_t deviceLogicId = 0; // 这里需要正确的deviceLogicId
    if (int32_t ret = DlAclApi::AclrtGetDevice(&deviceLogicId)) {
        BM_LOG_ERROR("RdmaIndirectTransportManager: aclrtGetDeviceFunc failed, ret: " << ret);
        return ret;
    }

    threadContext_ = std::make_shared<DeviceThreadContext>(deviceLogicId);
    if (!senderSideQueue_.Start(threadContext_)) {
        BM_LOG_ERROR("sender side queue start failed!");
        return BM_ERROR;
    }

    if (!receiverSideQueue_.Start(threadContext_)) {
        BM_LOG_ERROR("receiver side queue start failed!");
        senderSideQueue_.Stop();
        return BM_ERROR;
    }

    outBandAcceptThread_ = std::thread([this]() { AcceptLoop(); });
    BM_LOG_INFO("RdmaIndirectTransportManager: InitializeDirectLoop, pid: " << pid
                                                                            << ", deviceLogicId: " << deviceLogicId);
    return BM_OK;
}

Result RdmaIndirectTransportManager::OpenDevice(const TransportOptions &options)
{
    BM_LOG_INFO("RdmaIndirectTransportManager opening device with indirect mode support, rankId: "
                << options.rankId << ", rankCount: " << options.rankCount);

    localRankId_ = options.rankId;
    rankCount_ = options.rankCount;

    // 先调用父类的OpenDevice
    int ret = RdmaTransportManager::OpenDevice(options);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Parent OpenDevice failed: " << ret);
        return ret;
    }

    // 准备中转内存, 并填入到内存队列中
    ret = DlAclApi::AclrtMalloc(&buffer_, AGGREGATE_SIZE_LIMIT * (SEND_BUFFER_COUNT + RECV_BUFFER_COUNT), 0);
    if (ret != BM_OK) {
        BM_LOG_ERROR("RdmaIndirectTransportManager Failed to allocate device memory, ret:" << ret);
        RdmaTransportManager::CloseDevice(); // 清理已分配的资源
        return ret;
    }
    BM_LOG_INFO("Allocated device buffer, size: " << (AGGREGATE_SIZE_LIMIT * (SEND_BUFFER_COUNT + RECV_BUFFER_COUNT))
                                                  << ", addr: " << buffer_);

    TransportMemoryRegion mr;
    mr.addr = (uint64_t)buffer_;
    mr.size = AGGREGATE_SIZE_LIMIT * (SEND_BUFFER_COUNT + RECV_BUFFER_COUNT);
    ret = RdmaTransportManager::RegisterMemoryRegion(mr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("RdmaIndirectTransportManager Failed to RegisterMemoryRegion device memory, ret:" << ret);
        DlAclApi::AclrtFree(buffer_);
        RdmaTransportManager::CloseDevice();
        return ret;
    }

    ret = RdmaTransportManager::QueryMemoryKey((uint64_t)buffer_, swapMemKey_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("RdmaIndirectTransportManager Failed to QueryMemoryKey device memory, ret:" << ret);
        RdmaTransportManager::UnregisterMemoryRegion((uint64_t)buffer_);
        DlAclApi::AclrtFree(buffer_);
        RdmaTransportManager::CloseDevice();
        return ret;
    }

    auto ptr = reinterpret_cast<uint64_t>(buffer_);
    for (auto i = 0ULL; i < SEND_BUFFER_COUNT; i++) {
        BM_LOG_INFO("push buffer to sendBufferQueue_ : " << ptr);
        sendBufferQueue_.Push(ptr);
        ptr += AGGREGATE_SIZE_LIMIT;
    }
    for (auto i = 0ULL; i < RECV_BUFFER_COUNT; i++) {
        BM_LOG_INFO("push buffer to recvBufferQueue_ : " << ptr);
        recvBufferQueue_.Push(ptr);
        ptr += AGGREGATE_SIZE_LIMIT;
    }

    if (const char *socketUrl = std::getenv("MF_SOCKET_URL"); socketUrl == nullptr) {
        BM_LOG_WARN("MF_SOCKET_URL is not set, using url:" << options.nic);
        localNic_ = options.nic;
    } else {
        localNic_ = socketUrl;
    }
    ret = InitListenerSocket(localNic_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("RdmaIndirectTransportManager Failed to initListenerSocket, ret:" << ret);
        RdmaTransportManager::UnregisterMemoryRegion((uint64_t)buffer_);
        DlAclApi::AclrtFree(buffer_);
        RdmaTransportManager::CloseDevice();
        return ret;
    }

    nics_ = std::vector<std::string>(rankCount_, "");
    running_ = true;
    ret = InitializeDirectLoop();
    if (ret != BM_OK) {
        nics_.clear();
        running_ = false;
        BM_LOG_ERROR("RdmaIndirectTransportManager Failed to InitializeDirectLoop, ret:" << ret);
        close(gServerSocket_);
        gServerSocket_ = -1;
        close(gOutBandEpollFd_);
        gOutBandEpollFd_ = -1;
        RdmaTransportManager::UnregisterMemoryRegion((uint64_t)buffer_);
        DlAclApi::AclrtFree(buffer_);
        RdmaTransportManager::CloseDevice();
        return ret;
    }

    BM_LOG_INFO("RdmaIndirectTransportManager opened successfully");
    return BM_OK;
}

Result RdmaIndirectTransportManager::CloseDevice()
{
    BM_LOG_INFO("RdmaIndirectTransportManager CloseDevice start");
    
    // Check if already closed to make this function idempotent
    if (!running_ && gServerSocket_ < 0 && gOutBandEpollFd_ < 0 && buffer_ == nullptr) {
        BM_LOG_DEBUG("RdmaIndirectTransportManager already closed, skip");
        return RdmaTransportManager::CloseDevice();
    }
    
    // Stop running flag and notify waiting threads
    if (running_) {
        running_ = false;
        initiatorCond_.notify_all();
    }

    // Join accept thread if it's running
    if (outBandAcceptThread_.joinable()) {
        outBandAcceptThread_.join();
    }
    
    // Stop queues and close sockets
    senderSideQueue_.Stop();
    receiverSideQueue_.Stop();
    senderSideQueue_.CloseAllSockets();
    receiverSideQueue_.CloseAllSockets();

    // Close epoll fd
    if (gOutBandEpollFd_ >= 0) {
        close(gOutBandEpollFd_);
        gOutBandEpollFd_ = -1;
    }

    // Close server socket
    if (gServerSocket_ >= 0) {
        close(gServerSocket_);
        gServerSocket_ = -1;
    }
    
    // Free buffer if allocated
    if (buffer_ != nullptr) {
        RdmaTransportManager::UnregisterMemoryRegion((uint64_t)buffer_);
        DlAclApi::AclrtFree(buffer_);
        buffer_ = nullptr;
    }
    
    // Clear nics vector
    nics_.clear();
    
    // Clear thread resource context
    threadContext_ = nullptr;
    
    // Call parent class CloseDevice to clean up RDMA resources
    auto ret = RdmaTransportManager::CloseDevice();
    if (ret != BM_OK) {
        BM_LOG_ERROR("RdmaIndirectTransportManager Failed to call parent CloseDevice, ret:" << ret);
        return ret;
    }
    
    BM_LOG_INFO("RdmaIndirectTransportManager CloseDevice successful");
    return BM_OK;
}

Result RdmaIndirectTransportManager::RegisterMemoryRegion(const TransportMemoryRegion &mr)
{
    BM_LOG_INFO("indirect transport manager skip external register");
    return BM_OK;
}

const TransportPrivateData RdmaIndirectTransportManager::GetPrivateData() const
{
    TransportPrivateData data = {};
    std::string nic = ModifyNicPort(localNic_, localPort_);
    std::copy_n(nic.c_str(), std::min(nic.size(), sizeof(data.ip) - 1), data.ip);
    data.ip[sizeof(data.ip) - 1] = '\0';
    data.key = swapMemKey_;
    BM_LOG_INFO("RdmaIndirectTransportManager GetPrivateData nic:" << data.ip << ", port:" << localPort_);
    return data;
}

Result RdmaIndirectTransportManager::UnregisterMemoryRegion(uint64_t addr)
{
    return BM_OK;
}

bool RdmaIndirectTransportManager::QueryHasRegistered(uint64_t addr, uint64_t size)
{
    (void)addr;
    (void)size;
    return true; // 全部是中转，相当于全部是注册的
}

Result RdmaIndirectTransportManager::QueryMemoryKey(uint64_t addr, TransportMemoryKey &key)
{
    return BM_OK;
}

Result RdmaIndirectTransportManager::Prepare(const HybmTransPrepareOptions &options)
{
    for (const auto &[rankId, snd] : options.options) {
        if (rankId >= rankCount_) {
            BM_LOG_ERROR("Failed to update rank info ranId: " << rankId << " not match rank count: " << rankCount_);
            return BM_INVALID_PARAM;
        }
    }

    HybmTransPrepareOptions modifiedOptions = options;
    for (auto &[rankId, info] : modifiedOptions.options) {
        info.memKeys.push_back(info.privateData.key);
        BM_LOG_INFO("Prepare ranId: " << rankId << ", localRankId:" << localRankId_ << ", ip:" << info.privateData.ip);
        if (rankId == localRankId_) {
            continue;
        }
        nics_[rankId] = info.privateData.ip;
        BM_LOG_INFO("Prepare ranId: " << rankId << ", nic: " << nics_[rankId]);
    }
    return RdmaTransportManager::Prepare(modifiedOptions);
}

Result RdmaIndirectTransportManager::RemoveRanks(const std::vector<uint32_t> &removedRanks)
{
    return RdmaTransportManager::RemoveRanks(removedRanks);
}

int RdmaIndirectTransportManager::ConnectToRemote(const std::string &nic, uint32_t remoteRankId, uint32_t localRankId)
{
    if (senderSideQueue_.ExistRankIdSocket(remoteRankId)) {
        return BM_OK;
    }

    UrlParser urlParser;
    if (!urlParser.Initialize(nic)) {
        BM_LOG_ERROR("parse input nic(" << nic << ") failed using UrlParser!");
        return BM_INVALID_PARAM;
    }

    int sockfd = socket(urlParser.GetAddressFamily(), SOCK_STREAM, 0);
    if (sockfd < 0) {
        BM_LOG_ERROR("socket creation failed, localRankId: " << localRankId << ", remoteRankId: " << remoteRankId
                                                             << ", errno: " << errno << ", error: " << strerror(errno));
        return -1;
    }

    // 使用 UrlParser 获取的地址信息
    auto serverAddr = urlParser.GetSockAddr();
    auto addrLen = urlParser.GetAddrLen();

    // 带重试的连接
    int ret = -1;
    for (int i = 0; i < MAX_RETRIES && ret < 0; i++) {
        ret = connect(sockfd, serverAddr, addrLen);
        if (ret < 0) {
            if (i < MAX_RETRIES - 1) {
                BM_LOG_WARN("connect attempt " << (i + 1) << " failed, localRankId: " << localRankId
                                               << ", remoteRankId: " << remoteRankId << ", retrying in "
                                               << RETRY_DELAY_MS << "ms, nic:" << nic);
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            }
        }
    }

    if (ret < 0) {
        close(sockfd);
        BM_LOG_ERROR("connect failed after " << MAX_RETRIES << " attempts, localRankId: " << localRankId
                                             << ", remoteRankId: " << remoteRankId << ", errno: " << errno
                                             << ", error: " << strerror(errno) << ", nic:" << nic);
        return -1;
    }

    BM_LOG_INFO("ConnectToRemote successful, localRankId: " << localRankId << ", remoteRankId: " << remoteRankId
                                                            << ", fd: " << sockfd);
    senderSideQueue_.AddRankIdSocket(remoteRankId, sockfd);
    return sockfd;
}

Result RdmaIndirectTransportManager::Connect()
{
    for (uint32_t i = 0; i < rankCount_; i++) {
        if (i == localRankId_) {
            continue;
        }
        
        if (nics_[i].empty()) {
            BM_LOG_DEBUG("NIC info for rank " << i << " is not ready yet, skip connecting");
            continue;
        }
        
        UrlParser urlParser;
        if (!urlParser.Initialize(nics_[i])) {
            BM_LOG_ERROR("parse nic(" << nics_[i] << ") failed using UrlParser!");
            continue;
        }
        
        if (ConnectToRemote(nics_[i], i, localRankId_) < 0) {
            return BM_ERROR;
        }
        BM_LOG_INFO("ConnectToRemote Prepare ranId: " << i << ", nic: " << nics_[i]);
    }
    return RdmaTransportManager::Connect();
}

Result RdmaIndirectTransportManager::AsyncConnect()
{
    return BM_OK;
}

Result RdmaIndirectTransportManager::WaitForConnected(int64_t timeoutNs)
{
    return RdmaTransportManager::WaitForConnected(timeoutNs);
}

Result RdmaIndirectTransportManager::UpdateRankOptions(const HybmTransPrepareOptions &options)
{
    for (const auto &[rankId, snd] : options.options) {
        if (rankId >= rankCount_) {
            BM_LOG_ERROR("Failed to update rank info ranId: " << rankId << " not match rank count: " << rankCount_);
            return BM_INVALID_PARAM;
        }
    }

    HybmTransPrepareOptions modifiedOptions = options;
    for (auto &[rankId, info] : modifiedOptions.options) {
        if (rankId == localRankId_) {
            continue;
        }
        info.memKeys.emplace_back(info.privateData.key);
        nics_[rankId] = info.privateData.ip;
        
        if (ConnectToRemote(nics_[rankId], rankId, localRankId_) < 0) {
            return BM_ERROR;
        }
        BM_LOG_INFO("ConnectToRemote Prepare ranId: " << rankId << ", nic: " << nics_[rankId]);
    }
    return RdmaTransportManager::UpdateRankOptions(modifiedOptions);
}

const std::string &RdmaIndirectTransportManager::GetNic() const
{
    return RdmaTransportManager::GetNic();
}

const void *RdmaIndirectTransportManager::GetQpInfo() const
{
    return RdmaTransportManager::GetQpInfo();
}

Result RdmaIndirectTransportManager::ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_LOG_DEBUG("ReadRemote: synchronous read from rank " << rankId << ", size: " << size);

    // 调用异步接口
    Result ret = ReadRemoteAsync(rankId, lAddr, rAddr, size);
    if (ret != BM_OK) {
        BM_LOG_ERROR("ReadRemoteAsync failed, rankId: " << rankId << ", ret: " << ret);
        return ret;
    }

    // 等待操作完成
    ret = Synchronize(rankId);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Synchronize failed after ReadRemoteAsync, rankId: " << rankId);
        return ret;
    }

    BM_LOG_DEBUG("ReadRemote completed successfully, rankId: " << rankId << ", size: " << size);
    return BM_OK;
}

Result RdmaIndirectTransportManager::WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    BM_LOG_DEBUG("WriteRemote: synchronous write to rank " << rankId << ", size: " << size);

    // 调用异步接口
    Result ret = WriteRemoteAsync(rankId, lAddr, rAddr, size);
    if (ret != BM_OK) {
        BM_LOG_ERROR("WriteRemoteAsync failed, rankId: " << rankId << ", ret: " << ret);
        return ret;
    }

    // 等待操作完成
    ret = Synchronize(rankId);
    if (ret != BM_OK) {
        BM_LOG_ERROR("Synchronize failed after WriteRemoteAsync, rankId: " << rankId);
        return ret;
    }

    BM_LOG_DEBUG("WriteRemote completed successfully, rankId: " << rankId << ", size: " << size);
    return BM_OK;
}

Result RdmaIndirectTransportManager::Synchronize(uint32_t rankId)
{
    BM_LOG_DEBUG("Synchronize called for rank " << rankId);
    if (pendingRequestContext_ == nullptr) {
        BM_LOG_DEBUG("No pending requests, returning immediately");
        return BM_OK;
    }

    // 等待所有请求完成
    std::unique_lock<std::mutex> lock(pendingRequestContext_->mutex);
    pendingRequestContext_->cond.wait(lock, []() {
        return pendingRequestContext_->count <= 0;
    });

    BM_LOG_DEBUG("Synchronize completed for rank " << rankId);
    return BM_OK;
}
} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock