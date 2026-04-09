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

#include <gtest/gtest.h>

#include "hybm_types.h"

#define private public
#define protected public
#include "device/device_rdma_transport_manager.h"
#include "dl_hccp_api.h"
#include "dl_acl_api.h"
#include "hybm_stream.h"
#undef private
#undef protected

using namespace ock::mf;
using namespace ock::mf::transport;
using namespace ock::mf::transport::device;

namespace {
struct DlAclApiFnGuard {
    aclrtMallocFunc oldAclrtMalloc{DlAclApi::pAclrtMalloc};
    aclrtFreeFunc oldAclrtFree{DlAclApi::pAclrtFree};
    aclrtMemcpyFunc oldAclrtMemcpy{DlAclApi::pAclrtMemcpy};

    ~DlAclApiFnGuard()
    {
        DlAclApi::pAclrtMalloc = oldAclrtMalloc;
        DlAclApi::pAclrtFree = oldAclrtFree;
        DlAclApi::pAclrtMemcpy = oldAclrtMemcpy;
    }
};

struct DlHccpApiFnGuard {
    raRdevGetHandleFunc oldRaRdevGetHandle{DlHccpApi::gRaRdevGetHandle};
    raInitFunc oldRaInit{DlHccpApi::gRaInit};
    tsdOpenFunc oldTsdOpen{DlHccpApi::gTsdOpen};
    raGetIfNumFunc oldRaGetIfNum{DlHccpApi::gRaGetIfNum};
    raGetIfAddrsFunc oldRaGetIfAddrs{DlHccpApi::gRaGetIfAddrs};
    raRdevInitV2Func oldRaRdevInitV2{DlHccpApi::gRaRdevInitV2};
    raGetNotifyBaseAddrFunc oldRaGetNotifyBaseAddr{DlHccpApi::gRaGetNotifyBaseAddr};
    raGetNotifyMrInfoFunc oldRaGetNotifyMrInfo{DlHccpApi::gRaGetNotifyMrInfo};
    raRegisterMrFunc oldRaRegisterMr{DlHccpApi::gRaRegisterMR};
    raSendWrV2Func oldRaSendWrV2{DlHccpApi::gRaSendWrV2};
    raDeregisterMrFunc oldRaDeregisterMr{DlHccpApi::gRaDeregisterMR};

    ~DlHccpApiFnGuard()
    {
        DlHccpApi::gRaRdevGetHandle = oldRaRdevGetHandle;
        DlHccpApi::gRaInit = oldRaInit;
        DlHccpApi::gTsdOpen = oldTsdOpen;
        DlHccpApi::gRaGetIfNum = oldRaGetIfNum;
        DlHccpApi::gRaGetIfAddrs = oldRaGetIfAddrs;
        DlHccpApi::gRaRdevInitV2 = oldRaRdevInitV2;
        DlHccpApi::gRaGetNotifyBaseAddr = oldRaGetNotifyBaseAddr;
        DlHccpApi::gRaGetNotifyMrInfo = oldRaGetNotifyMrInfo;
        DlHccpApi::gRaRegisterMR = oldRaRegisterMr;
        DlHccpApi::gRaSendWrV2 = oldRaSendWrV2;
        DlHccpApi::gRaDeregisterMR = oldRaDeregisterMr;
    }
};

int FakeRaRdevGetHandleOkNonNull(uint32_t, void** handle)
{
    *handle = reinterpret_cast<void*>(0xABCUL);
    return 0;
}

int FakeRaRdevGetHandleOkNull(uint32_t, void** handle)
{
    *handle = nullptr;
    return 0;
}

uint32_t FakeTsdOpenFail(uint32_t, uint32_t)
{
    return 1U;
}

uint32_t FakeTsdOpenOk(uint32_t, uint32_t)
{
    return 0U;
}

int FakeRaGetIfNumError(const HccpRaGetIfAttr*, uint32_t* num)
{
    *num = 0;
    return -1;
}

int FakeRaGetIfNumZero(const HccpRaGetIfAttr*, uint32_t* num)
{
    *num = 0;
    return 0;
}

int FakeRaGetIfNumTwo(const HccpRaGetIfAttr*, uint32_t* num)
{
    *num = 2;
    return 0;
}

int FakeRaGetIfAddrsError(const HccpRaGetIfAttr*, HccpInterfaceInfo*, uint32_t* num)
{
    (void)num;
    return -2;
}

int FakeRaGetIfAddrsOkEth0(const HccpRaGetIfAttr* cfg, HccpInterfaceInfo infos[], uint32_t* num)
{
    // Fill 2 entries, include eth<phyId> with AF_INET.
    if (num == nullptr || *num < 2) {
        return -3;
    }
    std::memset(infos, 0, sizeof(HccpInterfaceInfo) * (*num));

    // 0: non-matching
    infos[0].family = AF_INET;
    std::snprintf(infos[0].ifname, sizeof(infos[0].ifname), "eth999");

    // 1: matching
    infos[1].family = AF_INET;
    std::snprintf(infos[1].ifname, sizeof(infos[1].ifname), "eth%u", cfg->phyId);
    inet_aton("192.168.50.10", &infos[1].ifaddr.ip.addr);
    return 0;
}

int FakeRaRdevInitV2Ok(HccpRdevInitInfo, HccpRdev, void** rdmaHandle)
{
    *rdmaHandle = reinterpret_cast<void*>(0xEEUL);
    return 0;
}

int FakeRaRdevInitV2Fail(HccpRdevInitInfo, HccpRdev, void** rdmaHandle)
{
    *rdmaHandle = nullptr;
    return -5;
}

int FakeRaDeregisterMrFail(const void*, void*)
{
    return -7;
}

static int gRaInitCallCount = 0;
int FakeRaInitFail(const HccpRaInitConfig* cfg)
{
    (void)cfg;
    gRaInitCallCount++;
    return -1;
}

static int gAclMallocCallCount = 0;
static int gAclFreeCallCount = 0;
int32_t FakeAclrtMallocOk(void** ptr, size_t size, uint32_t flags)
{
    (void)size;
    (void)flags;
    gAclMallocCallCount++;
    *ptr = reinterpret_cast<void*>(0x12340000UL);
    return 0;
}

int FakeAclrtFreeOk(void* ptr)
{
    (void)ptr;
    gAclFreeCallCount++;
    return 0;
}

int32_t FakeAclrtMemcpyOk(void* dst, size_t dstSize, const void* src, size_t srcSize, uint32_t kind)
{
    (void)dst;
    (void)dstSize;
    (void)src;
    (void)srcSize;
    (void)kind;
    return 0;
}

int FakeRaGetNotifyBaseAddrOk(void*, uint64_t* va, uint64_t* size)
{
    if (va != nullptr) {
        *va = 0x90000000ULL;
    }
    if (size != nullptr) {
        *size = 0x1000ULL;
    }
    return 0;
}

int FakeRaGetNotifyMrInfoOk(void*, HccpMrInfo* info)
{
    if (info == nullptr) {
        return -1;
    }
    info->lkey = 0x11;
    info->rkey = 0x22;
    return 0;
}

int FakeRaRegisterMROk(const void*, HccpMrInfo* info, void** mrHandle)
{
    if (info != nullptr) {
        // Fill keys so InitStreamNotifyBuf can persist them.
        info->lkey = 0x33;
        info->rkey = 0x22;
    }
    if (mrHandle == nullptr) {
        return -1;
    }
    *mrHandle = reinterpret_cast<void*>(0xBEEFUL);
    return 0;
}

int FakeRaRegisterMRFail(const void*, HccpMrInfo* info, void** mrHandle)
{
    (void)info;
    if (mrHandle != nullptr) {
        *mrHandle = nullptr;
    }
    return -3;
}

class FakeQpManager final : public DeviceQpManager {
public:
    FakeQpManager() : DeviceQpManager(0, 0, 1, sockaddr_in{}, HYBM_ROLE_PEER)
    {
    }

    int SetRemoteRankInfo(const std::unordered_map<uint32_t, ConnectRankInfo>&) noexcept override
    {
        return setRemoteRet;
    }
    int WaitingConnectionReady() noexcept override
    {
        return waitingReadyRet;
    }
    int Startup(void*) noexcept override
    {
        return startupRet;
    }
    void Shutdown() noexcept override
    {
        shutdownCalled = true;
    }
    int RemoveRanks(const std::unordered_set<uint32_t>& ranks) noexcept override
    {
        (void)ranks;
        removeRanksCalled = true;
        return removeRanksRet;
    }
    const void* GetQpInfoAddress() const noexcept override
    {
        return qpInfoAddress;
    }
    UserQpInfo* GetQpHandleWithRankId(uint32_t) noexcept override
    {
        if (!returnQpHandle) {
            return nullptr;
        }
        userQpInfo_.qpHandle = qpHandleValue;
        return &userQpInfo_;
    }
    void PutQpHandle(UserQpInfo*) const noexcept override
    {
        putQpCalled = true;
    }
    bool CheckQpReady(const std::vector<uint32_t>& rankIds) const noexcept override
    {
        (void)rankIds;
        return checkQpReadyRet;
    }

    bool shutdownCalled{false};
    bool removeRanksCalled{false};
    int setRemoteRet{BM_OK};
    int startupRet{BM_OK};
    int removeRanksRet{BM_OK};
    int waitingReadyRet{BM_OK};
    bool checkQpReadyRet{true};
    const void* qpInfoAddress{reinterpret_cast<void*>(0x1234UL)};
    bool returnQpHandle{false};
    mutable bool putQpCalled{false};
    void* qpHandleValue{reinterpret_cast<void*>(0x55UL)};

private:
    mutable UserQpInfo userQpInfo_{};
};

class TestableRdmaTransportManager final : public RdmaTransportManager {
public:
    Result AsyncConnect() override
    {
        return asyncConnectRet;
    }
    Result WaitForConnected(int64_t) override
    {
        return waitConnectedRet;
    }

    Result asyncConnectRet{BM_OK};
    Result waitConnectedRet{BM_OK};
};
}  // namespace

TEST(RdmaTransportManagerTest, CloseDeviceClearsStateAndShutdownsQpManager)
{
    RdmaTransportManager mgr;
    auto qp = std::make_shared<FakeQpManager>();
    mgr.qpManager_ = qp;
    mgr.started_ = true;
    mgr.rdmaHandle_ = reinterpret_cast<void*>(0x1UL);
    mgr.ranksMRs_.resize(3);
    mgr.notifyRemoteInfo_.resize(3);
    mgr.deviceChipInfo_ = std::make_shared<DeviceChipInfo>(0);

    auto ret = mgr.CloseDevice();
    EXPECT_EQ(ret, BM_OK);
    EXPECT_TRUE(qp->shutdownCalled);
    EXPECT_EQ(mgr.qpManager_, nullptr);
    EXPECT_FALSE(mgr.started_);
    EXPECT_EQ(mgr.rdmaHandle_, nullptr);
    EXPECT_TRUE(mgr.ranksMRs_.empty());
    EXPECT_TRUE(mgr.notifyRemoteInfo_.empty());
    EXPECT_EQ(mgr.deviceChipInfo_, nullptr);
}

TEST(RdmaTransportManagerTest, RegisterMemoryRegionReturnsDlFailedWhenRaRegisterUnavailable)
{
    RdmaTransportManager mgr;
    mgr.rdmaHandle_ = reinterpret_cast<void*>(0x1UL);

    TransportMemoryRegion mr{};
    mr.addr = 0x1000;
    mr.size = 0x2000;
    mr.flags = 0;  // avoid DRAM path in ConvertHccpMrInfo

    auto ret = mgr.RegisterMemoryRegion(mr);
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    EXPECT_TRUE(mgr.registerMRS_.empty());
}

TEST(RdmaTransportManagerTest, UnregisterMemoryRegionNotFoundReturnsInvalidParam)
{
    RdmaTransportManager mgr;
    auto ret = mgr.UnregisterMemoryRegion(0x1234);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST(RdmaTransportManagerTest, UnregisterMemoryRegionDeregisterFailDoesNotErase)
{
    RdmaTransportManager mgr;
    mgr.rdmaHandle_ = reinterpret_cast<void*>(0x1UL);
    mgr.deviceId_ = 0;

    RegMemResult reg{};
    reg.regAddress = 0x2000;
    reg.address = 0x3000;  // make it go through HalHostUnregisterEx branch after deregister; but we fail earlier
    reg.size = 0x100;
    reg.mrHandle = reinterpret_cast<void*>(0x99UL);
    mgr.registerMRS_.emplace(0x2000, reg);

    auto ret = mgr.UnregisterMemoryRegion(0x2000);
    // DlHccpApi::RaDeregisterMR() will return BM_UNDER_API_UNLOAD when not loaded -> BM_DL_FUNCTION_FAILED.
    EXPECT_EQ(ret, BM_DL_FUNCTION_FAILED);
    EXPECT_EQ(mgr.registerMRS_.size(), 1U);
}

TEST(RdmaTransportManagerTest, QueryHasRegisteredFollowsLowerBoundSemantics)
{
    RdmaTransportManager mgr;
    RegMemResult reg{};
    reg.regAddress = 0x1000;
    reg.address = 0x1000;
    reg.size = 0x100;
    mgr.registerMRS_.emplace(0x1000, reg);

    // Exact key match -> true
    EXPECT_TRUE(mgr.QueryHasRegistered(0x1000, 0x10));

    // Address inside region but not equal to key: lower_bound returns end -> false (as implemented)
    // 实际实现用 lower_bound(addr) + range 检查；当只有一个 MR 且其 key < addr 时，
    // lower_bound(addr) 会返回 end，函数会直接返回 false。这里为了确保 lower_bound 命中该 MR，
    // 选择 addr 小于等于 key 的场景。
    EXPECT_TRUE(mgr.QueryHasRegistered(0x1000, 0x80));

    // Address before key: lower_bound hits 0x1000 and passes check -> true (as implemented)
    // Address before key: lower_bound points to 0x1000, but the range check uses pos->first + size < addr + size,
    // so it returns false here.
    EXPECT_FALSE(mgr.QueryHasRegistered(0x0F00, 0x10));
}

TEST(RdmaTransportManagerTest, QueryMemoryKeyNotRegisteredReturnsInvalidParam)
{
    RdmaTransportManager mgr;
    TransportMemoryKey key{};
    auto ret = mgr.QueryMemoryKey(0x1000, key);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

TEST(RdmaTransportManagerTest, PrepareRejectsInvalidOptionsEarly)
{
    RdmaTransportManager mgr;
    mgr.role_ = HYBM_ROLE_PEER;
    mgr.rankId_ = 0;
    mgr.rankCount_ = 2;

    // 1) options.size() > rankCount_ -> BM_INVALID_PARAM
    HybmTransPrepareOptions opts;
    TransportRankPrepareInfo info;
    info.nic = "tcp://127.0.0.1:1234";
    opts.options.emplace(0U, info);
    opts.options.emplace(1U, info);
    opts.options.emplace(2U, info);  // extra
    EXPECT_EQ(mgr.Prepare(opts), BM_INVALID_PARAM);

    // 2) contains rankId >= rankCount_ -> BM_INVALID_PARAM
    HybmTransPrepareOptions opts3;
    opts3.options.emplace(0U, info);
    opts3.options.emplace(9U, info);
    EXPECT_EQ(mgr.Prepare(opts3), BM_INVALID_PARAM);
}

TEST(RdmaTransportManagerTest, PrepareRequiresQpManager)
{
    RdmaTransportManager mgr;
    mgr.role_ = HYBM_ROLE_PEER;
    mgr.rankId_ = 0;
    mgr.rankCount_ = 1;

    HybmTransPrepareOptions opts;
    TransportRankPrepareInfo info;
    info.nic = "tcp://127.0.0.1:1234";
    opts.options.emplace(0U, info);

    // qpManager_ == nullptr -> BM_MALLOC_FAILED
    EXPECT_EQ(mgr.Prepare(opts), BM_MALLOC_FAILED);
}

TEST(RdmaTransportManagerTest, PrepareParseNicAndQpManagerFailuresArePropagated)
{
    RdmaTransportManager mgr;
    mgr.role_ = HYBM_ROLE_PEER;
    mgr.rankId_ = 0;
    mgr.rankCount_ = 1;
    mgr.rdmaHandle_ = reinterpret_cast<void*>(0x1UL);
    // Prepare() 内部会写 ranksMRs_[rankId] / notifyRemoteInfo_[rankId]，测试里需要预先按 rankCount_ 初始化避免 UB
    mgr.ranksMRs_.resize(mgr.rankCount_);
    mgr.notifyRemoteInfo_.resize(mgr.rankCount_);

    auto qp = std::make_shared<FakeQpManager>();
    mgr.qpManager_ = qp;

    // 1) ParseDeviceNic fail -> BM_INVALID_PARAM
    HybmTransPrepareOptions bad;
    TransportRankPrepareInfo badInfo;
    badInfo.nic = "bad-nic";
    bad.options.emplace(0U, badInfo);
    EXPECT_EQ(mgr.Prepare(bad), BM_INVALID_PARAM);

    // 2) SetRemoteRankInfo fail -> propagate
    HybmTransPrepareOptions good;
    TransportRankPrepareInfo info;
    info.nic = "tcp://127.0.0.1:1234";
    good.options.emplace(0U, info);
    qp->setRemoteRet = BM_ERROR;
    EXPECT_EQ(mgr.Prepare(good), BM_ERROR);

    // 3) Startup fail -> propagate
    qp->setRemoteRet = BM_OK;
    qp->startupRet = BM_ERROR;
    EXPECT_EQ(mgr.Prepare(good), BM_ERROR);

    // 4) Success path (with empty memKeys to avoid OptionsToRankMRs heavy deps)
    qp->startupRet = BM_OK;
    EXPECT_EQ(mgr.Prepare(good), BM_OK);
}

TEST(RdmaTransportManagerTest, RemoveRanksRequiresQpManager)
{
    RdmaTransportManager mgr;
    std::vector<uint32_t> removed{0};
    EXPECT_EQ(mgr.RemoveRanks(removed), BM_MALLOC_FAILED);
}

TEST(RdmaTransportManagerTest, RemoveRanksEmptyOrOutOfRangeIsOkAndCleansNotify)
{
    RdmaTransportManager mgr;
    auto qp = std::make_shared<FakeQpManager>();
    mgr.qpManager_ = qp;
    mgr.rankCount_ = 2;
    mgr.ranksMRs_.resize(2);
    mgr.notifyRemoteInfo_.resize(2);
    mgr.notifyRemoteInfo_[0] = {1, 2};
    mgr.notifyRemoteInfo_[1] = {3, 4};

    // ranksMRs_ empty for these ranks -> ranksSet stays empty -> BM_OK
    std::vector<uint32_t> removed{9, 0, 1};
    EXPECT_EQ(mgr.RemoveRanks(removed), BM_OK);
    EXPECT_FALSE(qp->removeRanksCalled);
    EXPECT_EQ(mgr.notifyRemoteInfo_[0].first, 0U);
    EXPECT_EQ(mgr.notifyRemoteInfo_[1].first, 0U);
}

TEST(RdmaTransportManagerTest, RemoveRanksCallsQpManagerAndPropagatesError)
{
    RdmaTransportManager mgr;
    auto qp = std::make_shared<FakeQpManager>();
    mgr.qpManager_ = qp;
    mgr.rankCount_ = 2;
    mgr.ranksMRs_.resize(2);
    mgr.notifyRemoteInfo_.resize(2);

    // Make rank 1 have remote MR entry so ranksSet not empty
    RegMemResult reg{};
    reg.address = 0x1000;
    reg.regAddress = 0x1000;
    reg.size = 0x10;
    mgr.ranksMRs_[1].emplace(0x1000, reg);

    qp->removeRanksRet = BM_ERROR;
    EXPECT_EQ(mgr.RemoveRanks({1}), BM_ERROR);
    EXPECT_TRUE(qp->removeRanksCalled);
}

TEST(RdmaTransportManagerTest, ConnectPropagatesAsyncWaitAndQpReadyErrors)
{
    TestableRdmaTransportManager mgr;
    mgr.qpManager_ = std::make_shared<FakeQpManager>();
    mgr.rankCount_ = 1;
    mgr.ranksMRs_.resize(1);

    mgr.asyncConnectRet = BM_ERROR;
    EXPECT_EQ(mgr.Connect(), BM_ERROR);

    mgr.asyncConnectRet = BM_OK;
    mgr.waitConnectedRet = BM_ERROR;
    EXPECT_EQ(mgr.Connect(), BM_ERROR);

    mgr.waitConnectedRet = BM_OK;
    EXPECT_EQ(mgr.Connect(), BM_OK);
}

TEST(RdmaTransportManagerTest, AsyncConnectAlwaysOk)
{
    RdmaTransportManager mgr;
    // 功能：异步连接入口（当前实现直接返回 BM_OK）
    // 使用：Prepare() 之后可先调用 AsyncConnect()，再 WaitForConnected()。
    EXPECT_EQ(mgr.AsyncConnect(), BM_OK);
}

TEST(RdmaTransportManagerTest, WaitForConnectedNullQpManagerAndErrorAndOk)
{
    RdmaTransportManager mgr;
    // 功能：等待所有连接建立完成（依赖 qpManager_->WaitingConnectionReady()）。
    // 使用：Connect() 内部会调用 WaitForConnected(-1)。

    // 1) qpManager_ 为空：直接报错 BM_ERROR
    EXPECT_EQ(mgr.WaitForConnected(-1), BM_ERROR);

    // 2) WaitingConnectionReady 返回错误：透传错误码
    auto qp = std::make_shared<FakeQpManager>();
    qp->waitingReadyRet = BM_TIMEOUT;
    mgr.qpManager_ = qp;
    EXPECT_EQ(mgr.WaitForConnected(-1), BM_TIMEOUT);

    // 3) WaitingConnectionReady 返回 OK：WaitForConnected 返回 OK
    qp->waitingReadyRet = BM_OK;
    EXPECT_EQ(mgr.WaitForConnected(-1), BM_OK);
}

TEST(RdmaTransportManagerTest, WaitQpReadyNullQpManagerAndOkPath)
{
    RdmaTransportManager mgr;
    // 功能：在连接建立后检查 qp 是否 ready（带超时循环）。
    // 使用：Connect()/UpdateRankOptions() 最终都要 WaitQpReady()。

    // 1) qpManager_ 为空：BM_MALLOC_FAILED
    EXPECT_EQ(mgr.WaitQpReady(), BM_MALLOC_FAILED);

    // 2) qpManager_->CheckQpReady == true：立即返回 BM_OK（不需要等待超时）
    auto qp = std::make_shared<FakeQpManager>();
    qp->checkQpReadyRet = true;
    mgr.qpManager_ = qp;
    mgr.rankCount_ = 1;
    mgr.ranksMRs_.resize(1);  // 供循环安全访问
    EXPECT_EQ(mgr.WaitQpReady(), BM_OK);
}

TEST(RdmaTransportManagerTest, UpdateRankOptionsNullQpManagerInvalidNicAndSetRemoteFailAndOk)
{
    RdmaTransportManager mgr;
    // 功能：动态更新 ranks 的 nic/memKeys，并等待 qp ready。
    // 使用：弹性/重连场景下更新远端信息（类似 Prepare，但不重复 Startup）。

    HybmTransPrepareOptions opts;
    TransportRankPrepareInfo info;
    info.nic = "tcp://127.0.0.1:1234";
    opts.options.emplace(0U, info);

    // 1) qpManager_ 为空：BM_ERROR
    EXPECT_EQ(mgr.UpdateRankOptions(opts), BM_ERROR);

    // 安装 qpManager_
    auto qp = std::make_shared<FakeQpManager>();
    mgr.qpManager_ = qp;
    mgr.rankCount_ = 1;
    mgr.ranksMRs_.resize(1);

    // 2) nic 非法：BM_INVALID_PARAM
    HybmTransPrepareOptions bad;
    TransportRankPrepareInfo badInfo;
    badInfo.nic = "bad-nic";
    bad.options.emplace(0U, badInfo);
    EXPECT_EQ(mgr.UpdateRankOptions(bad), BM_INVALID_PARAM);

    // 3) SetRemoteRankInfo 失败：透传
    qp->setRemoteRet = BM_ERROR;
    EXPECT_EQ(mgr.UpdateRankOptions(opts), BM_ERROR);

    // 4) OK：SetRemoteRankInfo=OK + memKeys 为空，OptionsToRankMRs 不会写入 ranksMRs_，WaitQpReady 立即 OK
    qp->setRemoteRet = BM_OK;
    qp->checkQpReadyRet = true;
    EXPECT_EQ(mgr.UpdateRankOptions(opts), BM_OK);
}

TEST(RdmaTransportManagerTest, GetNicAndGetQpInfo)
{
    RdmaTransportManager mgr;
    // 功能：GetNic 返回 OpenDevice() 生成的 nicInfo_；GetQpInfo 返回 qpManager_ 提供的 qp info 地址。
    // 使用：上层可能用 GetNic() 做 debug/打印，用 GetQpInfo() 将 qp 信息下发或查询。

    mgr.nicInfo_ = "tcp://1.2.3.4:5678";
    EXPECT_EQ(mgr.GetNic(), std::string("tcp://1.2.3.4:5678"));

    // qpManager_ 为空：GetQpInfo 返回 nullptr
    EXPECT_EQ(mgr.GetQpInfo(), nullptr);

    // qpManager_ 非空：返回 qpManager_->GetQpInfoAddress()
    auto qp = std::make_shared<FakeQpManager>();
    qp->qpInfoAddress = reinterpret_cast<void*>(0xBEEFUL);
    mgr.qpManager_ = qp;
    EXPECT_EQ(mgr.GetQpInfo(), reinterpret_cast<const void*>(0xBEEFUL));
}

TEST(RdmaTransportManagerTest, ReadWriteRemoteReturnErrorWhenQpManagerMissing)
{
    RdmaTransportManager mgr;
    // 功能：ReadRemote/WriteRemote 是 RemoteIO 的同步封装。
    // 使用：真正的 RDMA 操作入口；需要 Prepare/Connect 后、且 qpManager_ 有效。

    EXPECT_EQ(mgr.ReadRemote(0, 0x1000, 0x2000, 16), BM_ERROR);
    EXPECT_EQ(mgr.WriteRemote(0, 0x1000, 0x2000, 16), BM_ERROR);
}

TEST(RdmaTransportManagerTest, ReadWriteRemoteAsyncReturnErrorWhenQpManagerMissing)
{
    RdmaTransportManager mgr;
    // 功能：ReadRemoteAsync/WriteRemoteAsync 是 RemoteIO 的异步封装（sync=false）。
    // 使用：需要 Prepare/Connect 后 qpManager_ 正常；否则会从 RemoteIO 早退报错。
    EXPECT_EQ(mgr.ReadRemoteAsync(0, 0x1000, 0x2000, 16), BM_ERROR);
    EXPECT_EQ(mgr.WriteRemoteAsync(0, 0x1000, 0x2000, 16), BM_ERROR);
}

TEST(RdmaTransportManagerTest, SynchronizeRankIdNullQpManagerAndNoQpHandleAndPutCalled)
{
    RdmaTransportManager mgr;
    // 功能：Synchronize(rankId) 对指定 rank 的 qp 发送 notify + 等待完成。
    // 使用：sync 写/读路径或上层显式 barrier。

    // 1) qpManager_ 为空：BM_MALLOC_FAILED
    EXPECT_EQ(mgr.Synchronize(0), BM_MALLOC_FAILED);

    // 2) qpManager_ 存在但找不到 qp：BM_ERROR
    auto qp = std::make_shared<FakeQpManager>();
    mgr.qpManager_ = qp;
    EXPECT_EQ(mgr.Synchronize(0), BM_ERROR);
    EXPECT_FALSE(qp->putQpCalled);

    // 3) qp 存在：即使内部 stream 未初始化导致失败，也必须 PutQpHandle 归还句柄
    qp->returnQpHandle = true;
    mgr.rankCount_ = 1;
    mgr.notifyRemoteInfo_.resize(1);
    auto ret = mgr.Synchronize(0);
    EXPECT_NE(ret, BM_OK);  // 设备侧 stream 在 UT 环境通常未初始化，会走 BM_ASSERT_RETURN 失败路径
    EXPECT_TRUE(qp->putQpCalled);
}

TEST(RdmaTransportManagerTest, OpenTsdPropagatesErrorAndCachesSuccess)
{
    DlHccpApiFnGuard guard;
    // 功能：OpenTsd() 负责打开 TSD，一次成功后用 static 标志缓存，后续不再重复打开。
    // 使用：OpenDevice() 过程中会调用。

    DlHccpApi::gTsdOpen = &FakeTsdOpenFail;
    EXPECT_FALSE(RdmaTransportManager::OpenTsd(0, 1));

    DlHccpApi::gTsdOpen = &FakeTsdOpenOk;
    EXPECT_TRUE(RdmaTransportManager::OpenTsd(0, 1));

    // 缓存生效：即使底层变失败，也应直接返回 true（不再调用 TsdOpen）
    DlHccpApi::gTsdOpen = &FakeTsdOpenFail;
    EXPECT_TRUE(RdmaTransportManager::OpenTsd(0, 1));
}

TEST(RdmaTransportManagerTest, RetireDeviceIpCoversErrorPathsAndCachesSuccess)
{
    DlHccpApiFnGuard guard;
    in_addr ip{};

    // 1) RaGetIfNum 返回错误：false
    DlHccpApi::gRaGetIfNum = &FakeRaGetIfNumError;
    DlHccpApi::gRaGetIfAddrs = &FakeRaGetIfAddrsOkEth0;
    EXPECT_FALSE(RdmaTransportManager::RetireDeviceIp(0, ip));

    // 2) count==0：false
    DlHccpApi::gRaGetIfNum = &FakeRaGetIfNumZero;
    EXPECT_FALSE(RdmaTransportManager::RetireDeviceIp(0, ip));

    // 3) RaGetIfAddrs 失败：false
    DlHccpApi::gRaGetIfNum = &FakeRaGetIfNumTwo;
    DlHccpApi::gRaGetIfAddrs = &FakeRaGetIfAddrsError;
    EXPECT_FALSE(RdmaTransportManager::RetireDeviceIp(0, ip));

    // 4) 成功：找到 eth<deviceId> 的 AF_INET，写回 ip，并缓存
    DlHccpApi::gRaGetIfAddrs = &FakeRaGetIfAddrsOkEth0;
    EXPECT_TRUE(RdmaTransportManager::RetireDeviceIp(0, ip));
    EXPECT_NE(ip.s_addr, 0U);

    // 缓存分支：即使把底层函数置空，依然应直接返回 true 并复用 retiredIp
    DlHccpApi::gRaGetIfNum = nullptr;
    DlHccpApi::gRaGetIfAddrs = nullptr;
    in_addr ip2{};
    EXPECT_TRUE(RdmaTransportManager::RetireDeviceIp(0, ip2));
    EXPECT_EQ(ip2.s_addr, ip.s_addr);
}

TEST(RdmaTransportManagerTest, PrepareOpenDeviceCoversHandleReusePath)
{
    DlHccpApiFnGuard guard;
    // 功能：PrepareOpenDevice() 先尝试复用已打开的 device rdmaHandle；复用失败才走 OpenTsd/RaInit/RetireIp/RdevInit。
    // 使用：OpenDevice() 中用于准备底层 RDMA 环境。

    // RaRdevGetHandle 成功且句柄非空：走“复用句柄 + RetireDeviceIp”路径（避免进入 RaInit 的 sleep）
    DlHccpApi::gRaRdevGetHandle = &FakeRaRdevGetHandleOkNonNull;
    DlHccpApi::gRaGetIfNum = &FakeRaGetIfNumTwo;
    DlHccpApi::gRaGetIfAddrs = &FakeRaGetIfAddrsOkEth0;
    void* handle = nullptr;
    in_addr ip{};
    EXPECT_TRUE(RdmaTransportManager::PrepareOpenDevice(0, 0, 1, ip, handle));
    EXPECT_NE(handle, nullptr);
    EXPECT_NE(ip.s_addr, 0U);
}

TEST(RdmaTransportManagerTest, RaInitCallsUnderApiOnceAndThenCaches)
{
    DlHccpApiFnGuard guard;
    DlHccpApi::gRaInit = &FakeRaInitFail;
    gRaInitCallCount = 0;

    // 1st call: should reach RaInit() and call under-api; may sleep once inside implementation.
    EXPECT_FALSE(RdmaTransportManager::RaInit(0));
    EXPECT_EQ(gRaInitCallCount, 1);

    // 2nd call: cached static flag -> no further under-api calls (and no further sleep).
    EXPECT_TRUE(RdmaTransportManager::RaInit(0));
    EXPECT_EQ(gRaInitCallCount, 1);
}

TEST(RdmaTransportManagerTest, GetRoceDbAddrReturnsExpectedWhenChipInfoReady)
{
    RdmaTransportManager mgr;
    mgr.deviceChipInfo_ = std::make_shared<DeviceChipInfo>(0);
    ASSERT_NE(mgr.deviceChipInfo_, nullptr);

    // Fill chip info fields directly for deterministic test.
    mgr.deviceChipInfo_->chipId_ = 2;
    mgr.deviceChipInfo_->dieId_ = 1;
    mgr.deviceChipInfo_->chipBaseAddr_ = 0x1000;
    mgr.deviceChipInfo_->chipOffset_ = 0x100000;
    mgr.deviceChipInfo_->chipDieOffset_ = 0x10000;

    // Constants are defined in the .cpp:
    // RT_ASCEND910B1_ROCEE_BASE_ADDR=0x2000000000, RT_ASCEND910B1_ROCEE_VF_DB_CFG0_REG=0x230
    const uint64_t expected = 0x2000000000ULL + 0x230ULL +
                              mgr.deviceChipInfo_->chipOffset_ * static_cast<uint64_t>(mgr.deviceChipInfo_->chipId_) +
                              mgr.deviceChipInfo_->chipDieOffset_ * mgr.deviceChipInfo_->dieId_ +
                              mgr.deviceChipInfo_->chipBaseAddr_;

    EXPECT_EQ(mgr.GetRoceDbAddrForRdmaDbSendTask(), expected);
}

TEST(RdmaTransportManagerTest, ConstructSqeNoSinkModeFillsFieldsWhenDbAddrNonZero)
{
    RdmaTransportManager mgr;
    mgr.deviceChipInfo_ = std::make_shared<DeviceChipInfo>(0);
    mgr.deviceChipInfo_->chipId_ = 0;
    mgr.deviceChipInfo_->dieId_ = 0;
    mgr.deviceChipInfo_->chipBaseAddr_ = 0;
    mgr.deviceChipInfo_->chipOffset_ = 0;
    mgr.deviceChipInfo_->chipDieOffset_ = 0;

    // Create a minimal stream object without Initialize; only GetId/GetWqeFlag are used.
    auto st = std::make_shared<HybmStream>(0, 0, 0);
    st->streamId_ = 7;
    st->wqeFlag_ = true;

    send_wr_rsp rsp{};
    rsp.db.db_info = 0xAABBCCDD11223344ULL;
    rtStarsSqe_t cmd{};
    mgr.ConstructSqeNoSinkModeForRdmaDbSendTask(rsp, cmd, st);

    const auto& sqe = cmd.writeValueSqe;
    EXPECT_EQ(sqe.header.type, RT_STARS_SQE_TYPE_WRITE_VALUE);
    EXPECT_EQ(sqe.header.wr_cqe, st->GetWqeFlag());
    EXPECT_EQ(sqe.header.rt_stream_id, static_cast<uint16_t>(st->GetId()));
    // Constants are defined in the .cpp:
    // RT_STARS_WRITE_VALUE_SUB_TYPE_RDMA_DB_SEND=2, RT_STARS_WRITE_VALUE_SIZE_TYPE_64BIT=3
    EXPECT_EQ(sqe.sub_type, 2U);
    EXPECT_EQ(sqe.awsize, 3U);
    EXPECT_EQ(sqe.write_value_part0, static_cast<uint32_t>(rsp.db.db_info & MASK_32_BIT));
    EXPECT_EQ(sqe.write_value_part1, static_cast<uint32_t>(rsp.db.db_info >> UINT32_BIT_NUM));
}

TEST(RdmaTransportManagerTest, InitStreamNotifyBufSuccessAndRegisterFailPaths)
{
    DlHccpApiFnGuard hccpGuard;
    DlAclApiFnGuard aclGuard;

    RdmaTransportManager mgr;
    mgr.rankCount_ = 2;
    mgr.rdmaHandle_ = reinterpret_cast<void*>(0x1UL);

    DlHccpApi::gRaGetNotifyBaseAddr = &FakeRaGetNotifyBaseAddrOk;
    DlHccpApi::gRaGetNotifyMrInfo = &FakeRaGetNotifyMrInfoOk;
    DlAclApi::pAclrtMalloc = &FakeAclrtMallocOk;
    DlAclApi::pAclrtMemcpy = &FakeAclrtMemcpyOk;
    DlAclApi::pAclrtFree = &FakeAclrtFreeOk;

    // 1) Register MR fail -> must free ptr and return BM_DL_FUNCTION_FAILED
    DlHccpApi::gRaRegisterMR = &FakeRaRegisterMRFail;
    gAclMallocCallCount = 0;
    gAclFreeCallCount = 0;
    EXPECT_EQ(mgr.InitStreamNotifyBuf(), BM_DL_FUNCTION_FAILED);
    EXPECT_EQ(gAclMallocCallCount, 1);
    EXPECT_EQ(gAclFreeCallCount, 1);

    // 2) Register MR ok -> fills notifyInfo_ and initializes notifyRemoteInfo_
    DlHccpApi::gRaRegisterMR = &FakeRaRegisterMROk;
    gAclMallocCallCount = 0;
    gAclFreeCallCount = 0;
    EXPECT_EQ(mgr.InitStreamNotifyBuf(), BM_OK);
    EXPECT_EQ(mgr.notifyInfo_.notifyAddr, 0x90000000ULL);
    EXPECT_EQ(mgr.notifyInfo_.len, 4U);
    EXPECT_EQ(mgr.notifyInfo_.notifyLkey, 0x11U);
    EXPECT_NE(mgr.notifyInfo_.srcAddr, 0ULL);
    EXPECT_EQ(mgr.notifyInfo_.srcRkey, 0x22U);
    ASSERT_EQ(mgr.notifyRemoteInfo_.size(), 2U);
    EXPECT_EQ(mgr.notifyRemoteInfo_[0].first, 0U);
    EXPECT_EQ(mgr.notifyRemoteInfo_[0].second, 0U);
    EXPECT_EQ(gAclMallocCallCount, 1);
    EXPECT_EQ(gAclFreeCallCount, 0);  // success path does not free
}

TEST(RdmaTransportManagerTest, SynchronizeQpHandleReturnsErrorWhenNoStream)
{
    RdmaTransportManager mgr;
    mgr.rankCount_ = 1;
    mgr.notifyRemoteInfo_.resize(1);
    mgr.notifyRemoteInfo_[0] = {0x1111ULL, 0x2222U};

    // In UT env, HybmStreamManager can't create a real stream without HAL; should early-return BM_ERROR.
    EXPECT_EQ(mgr.Synchronize(reinterpret_cast<void*>(0x1UL), 0), BM_ERROR);
}

TEST(RdmaTransportManagerTest, RaRdevInitSuccessAndCachesHandle)
{
    DlHccpApiFnGuard guard;
    // 功能：RaRdevInit() 调 DlHccpApi::RaRdevInitV2 初始化 rdev，并用 static storedRdmaHandle 缓存。
    // 使用：PrepareOpenDevice() 最后一步。
    DlHccpApi::gRaRdevInitV2 = &FakeRaRdevInitV2Ok;

    void* handle = nullptr;
    in_addr ip{};
    inet_aton("10.0.0.2", &ip);
    EXPECT_TRUE(RdmaTransportManager::RaRdevInit(0, ip, handle));
    EXPECT_NE(handle, nullptr);

    // 缓存分支：即使底层函数被置空，也能返回 true 并复用之前的 handle
    DlHccpApi::gRaRdevInitV2 = nullptr;
    void* handle2 = nullptr;
    EXPECT_TRUE(RdmaTransportManager::RaRdevInit(0, ip, handle2));
    EXPECT_EQ(handle2, handle);
}

TEST(RdmaTransportManagerTest, ClearAllRegisterMRsClearsMapEvenWhenNoHandle)
{
    RdmaTransportManager mgr;
    // 功能：ClearAllRegisterMRs 清理 registerMRS_；若 rdmaHandle_ 为空则不尝试 deregister，直接清空。
    RegMemResult reg{};
    reg.mrHandle = reinterpret_cast<void*>(0x1UL);
    mgr.registerMRS_.emplace(0x1000, reg);
    mgr.registerMRS_.emplace(0x2000, reg);
    mgr.rdmaHandle_ = nullptr;
    mgr.ClearAllRegisterMRs();
    EXPECT_TRUE(mgr.registerMRS_.empty());
}

TEST(RdmaTransportManagerTest, ClearAllRegisterMRsDeregisterFailsButStillClears)
{
    DlHccpApiFnGuard guard;
    RdmaTransportManager mgr;
    // 功能：即使底层 RaDeregisterMR 失败，也应继续并最终清空 map（避免重复注销/泄露）。
    DlHccpApi::gRaDeregisterMR = &FakeRaDeregisterMrFail;
    mgr.rdmaHandle_ = reinterpret_cast<void*>(0x1UL);
    RegMemResult reg{};
    reg.mrHandle = reinterpret_cast<void*>(0x2UL);
    mgr.registerMRS_.emplace(0x1000, reg);
    mgr.ClearAllRegisterMRs();
    EXPECT_TRUE(mgr.registerMRS_.empty());
}

TEST(RdmaTransportManagerTest, GetRegAddressNotFoundAndOk)
{
    RdmaTransportManager mgr;
    // 功能：GetRegAddress 从 MR map 找到覆盖 inputAddr+size 的 MR，并计算 regAddress 偏移和 key。
    MemoryRegionMap map;
    RegMemResult reg{};
    reg.address = 0x1000;
    reg.regAddress = 0xA000;
    reg.size = 0x100;
    reg.lkey = 11;
    reg.rkey = 22;
    map.emplace(0x1000, reg);

    uint64_t outAddr = 0;
    uint32_t key = 0;
    EXPECT_EQ(mgr.GetRegAddress(map, 0x2000, 0x10, true, outAddr, key), BM_INVALID_PARAM);

    // MemoryRegionMap uses std::greater comparator; lower_bound(0x1080) will still hit key 0x1000 here.
    EXPECT_EQ(mgr.GetRegAddress(map, 0x1080, 0x10, true, outAddr, key), BM_OK);
    EXPECT_EQ(outAddr, 0xA080);
    EXPECT_EQ(key, 11U);

    EXPECT_EQ(mgr.GetRegAddress(map, 0x1008, 0x10, true, outAddr, key), BM_OK);
    EXPECT_EQ(outAddr, 0xA008);
    EXPECT_EQ(key, 11U);
}

TEST(RdmaTransportManagerTest, CorrectHostRegWrCoversLocalMissingRankMissingRemoteMissing)
{
    RdmaTransportManager mgr;
    mgr.rankCount_ = 2;
    mgr.ranksMRs_.resize(2);

    send_wr_v2 wr{};
    sg_list sg{.addr = 0x1000, .len = 16, .lkey = 0};
    wr.buf_list = &sg;
    wr.dst_addr = 0x2000;

    // 1) local lAddr 未注册：BM_INVALID_PARAM
    EXPECT_EQ(mgr.CorrectHostRegWr(1, 0x1000, 0x2000, 16, wr), BM_INVALID_PARAM);

    // 注册本地 MR
    RegMemResult local{};
    local.address = 0x1000;
    local.regAddress = 0x9000;
    local.size = 0x100;
    local.lkey = 7;
    mgr.registerMRS_.emplace(0x1000, local);

    // 2) ranksMRs_[rankId] 为空：BM_INVALID_PARAM
    EXPECT_EQ(mgr.CorrectHostRegWr(1, 0x1000, 0x2000, 16, wr), BM_INVALID_PARAM);

    // 3) remote rAddr 未注册：BM_INVALID_PARAM
    RegMemResult remote{};
    remote.address = 0x3000;
    remote.regAddress = 0xB000;
    remote.size = 0x100;
    remote.rkey = 99;
    mgr.ranksMRs_[1].emplace(0x3000, remote);
    EXPECT_EQ(mgr.CorrectHostRegWr(1, 0x1000, 0x2000, 16, wr), BM_INVALID_PARAM);

    // 4) success：wr.buf_list.addr/lkey + wr.dst_addr/rkey 被修正
    mgr.ranksMRs_[1].clear();
    remote.address = 0x2000;
    remote.regAddress = 0xC000;
    mgr.ranksMRs_[1].emplace(0x2000, remote);
    EXPECT_EQ(mgr.CorrectHostRegWr(1, 0x1008, 0x2004, 16, wr), BM_OK);
    EXPECT_EQ(wr.buf_list->addr, 0x9008);
    EXPECT_EQ(wr.buf_list->lkey, 7U);
    EXPECT_EQ(wr.dst_addr, 0xC004);
    EXPECT_EQ(wr.rkey, 99U);
}

TEST(RdmaTransportManagerTest, OptionsToRankMRsSkipsOutOfRangeAndUpdatesNotifyAndDedup)
{
    RdmaTransportManager mgr;
    mgr.rankCount_ = 2;
    mgr.ranksMRs_.resize(2);
    mgr.notifyRemoteInfo_.resize(2);

    HybmTransPrepareOptions opts;

    // 1) out-of-range node should be skipped
    TransportRankPrepareInfo badNode;
    badNode.nic = "tcp://127.0.0.1:1";
    opts.options.emplace(9U, badNode);

    // 2) valid node with memKey; include notify so notifyRemoteInfo_ is updated
    RegMemKeyUnion u{};
    u.deviceKey.type = TT_HCCP;
    u.deviceKey.address = 0x1000;
    u.deviceKey.size = 0x10;
    u.deviceKey.notifyAddr = 0x8888;
    u.deviceKey.notifyRkey = 0x66;

    TransportRankPrepareInfo good;
    good.nic = "tcp://127.0.0.1:2";
    good.memKeys.push_back(u.commonKey);
    // duplicate key to hit "already exists, skip emplace"
    good.memKeys.push_back(u.commonKey);
    opts.options.emplace(1U, good);

    mgr.OptionsToRankMRs(opts);

    EXPECT_EQ(mgr.ranksMRs_[1].size(), 1U);
    EXPECT_EQ(mgr.notifyRemoteInfo_[1].first, 0x8888U);
    EXPECT_EQ(mgr.notifyRemoteInfo_[1].second, 0x66U);
}

TEST(RdmaTransportManagerTest, RemoteIOReturnsErrorWhenNoQpAndWhenNoStreamAndShowsMissingPut)
{
    RdmaTransportManager mgr;
    auto qp = std::make_shared<FakeQpManager>();
    mgr.qpManager_ = qp;
    mgr.rankCount_ = 1;
    mgr.ranksMRs_.resize(1);

    // 1) qp 为空：BM_ERROR
    qp->returnQpHandle = false;
    EXPECT_EQ(mgr.RemoteIO(0, 0x1000, 0x2000, 16, false, false), BM_ERROR);

    // 2) qp 非空，但 stream 未初始化（UT 环境下通常为 nullptr）：BM_ERROR
    // 注意：当前实现中 hStream 为空会直接 return，未 PutQpHandle，这是潜在资源泄露风险；此处用 UT 记录该行为。
    qp->returnQpHandle = true;
    qp->putQpCalled = false;
    EXPECT_EQ(mgr.RemoteIO(0, 0x1000, 0x2000, 16, false, false), BM_ERROR);
    EXPECT_FALSE(qp->putQpCalled);
}
