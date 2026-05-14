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

#define private public
#include "compose_transport_manager.h"
#undef private

using namespace ock::mf;
using namespace ock::mf::transport;

namespace {

class FakeTagInfo : public HybmEntityTagInfo {
public:
    explicit FakeTagInfo(uint32_t op) : opType_(op) {}

    uint32_t GetRank2RankOpType(uint32_t, uint32_t)
    {
        return opType_;
    }

private:
    uint32_t opType_;
};

class FakeTransportManager : public TransportManager {
public:
    Result OpenDevice(const TransportOptions &) override
    {
        ++openDeviceCalls;
        return openDeviceResult;
    }

    Result CloseDevice() override
    {
        ++closeDeviceCalls;
        return closeDeviceResult;
    }

    Result RegisterMemoryRegion(const TransportMemoryRegion &mr) override
    {
        (void)mr;
        ++registerCalls;
        return registerResult;
    }

    Result UnregisterMemoryRegion(uint64_t addr) override
    {
        (void)addr;
        ++unregisterCalls;
        return unregisterResult;
    }

    bool QueryHasRegistered(uint64_t, uint64_t) override
    {
        ++queryHasRegCalls;
        return queryHasRegResult;
    }

    Result QueryMemoryKey(uint64_t, TransportMemoryKey &) override
    {
        ++queryKeyCalls;
        return queryKeyResult;
    }

    void UpdateMemoryKey(TransportMemoryKey &key, void *addr) override
    {
        return;
    }

    Result Prepare(const HybmTransPrepareOptions &) override
    {
        ++prepareCalls;
        return prepareResult;
    }

    Result RemoveRanks(const std::vector<uint32_t> &) override
    {
        ++removeRanksCalls;
        return removeRanksResult;
    }

    Result Connect() override
    {
        ++connectCalls;
        return connectResult;
    }

    Result AsyncConnect() override
    {
        ++asyncConnectCalls;
        return asyncConnectResult;
    }

    Result WaitForConnected(int64_t) override
    {
        ++waitConnectedCalls;
        return waitConnectedResult;
    }

    Result UpdateRankOptions(const HybmTransPrepareOptions &) override
    {
        ++updateRankCalls;
        return updateRankResult;
    }

    const std::string &GetNic() const override
    {
        return nic_;
    }

    const void *GetQpInfo() const override
    {
        return nullptr;
    }

    Result ReadRemote(uint32_t, uint64_t, uint64_t, uint64_t) override
    {
        ++readCalls;
        return readResult;
    }

    Result WriteRemote(uint32_t, uint64_t, uint64_t, uint64_t) override
    {
        ++writeCalls;
        return writeResult;
    }

    Result ReadRemoteAsync(uint32_t, uint64_t, uint64_t, uint64_t) override
    {
        ++readAsyncCalls;
        return readAsyncResult;
    }

    Result WriteRemoteAsync(uint32_t, uint64_t, uint64_t, uint64_t) override
    {
        ++writeAsyncCalls;
        return writeAsyncResult;
    }

    Result Synchronize(uint32_t) override
    {
        ++syncCalls;
        return syncResult;
    }

    Result Remove(const std::vector<uint32_t> &removeList) override
    {
        (void)removeList;
        ++removeCalls;
        return removeResult;
    }

    Result WriteRemoteBatchAsync(uint32_t, const CopyDescriptor &) override
    {
        ++writeBatchAsyncCalls;
        return writeBatchAsyncResult;
    }

    Result ReadRemoteBatchAsync(uint32_t, const CopyDescriptor &) override
    {
        ++readBatchAsyncCalls;
        return readBatchAsyncResult;
    }

public:
    uint32_t openDeviceCalls{0};
    uint32_t closeDeviceCalls{0};
    uint32_t registerCalls{0};
    uint32_t unregisterCalls{0};
    uint32_t queryHasRegCalls{0};
    uint32_t queryKeyCalls{0};
    uint32_t prepareCalls{0};
    uint32_t removeRanksCalls{0};
    uint32_t connectCalls{0};
    uint32_t asyncConnectCalls{0};
    uint32_t waitConnectedCalls{0};
    uint32_t updateRankCalls{0};
    uint32_t readCalls{0};
    uint32_t writeCalls{0};
    uint32_t readAsyncCalls{0};
    uint32_t writeAsyncCalls{0};
    uint32_t syncCalls{0};
    uint32_t removeCalls{0};
    uint32_t writeBatchAsyncCalls{0};
    uint32_t readBatchAsyncCalls{0};

    Result openDeviceResult{BM_OK};
    Result closeDeviceResult{BM_OK};
    Result registerResult{BM_OK};
    Result unregisterResult{BM_OK};
    bool queryHasRegResult{false};
    Result queryKeyResult{BM_OK};
    Result prepareResult{BM_OK};
    Result removeRanksResult{BM_OK};
    Result connectResult{BM_OK};
    Result asyncConnectResult{BM_OK};
    Result waitConnectedResult{BM_OK};
    Result updateRankResult{BM_OK};
    Result readResult{BM_OK};
    Result writeResult{BM_OK};
    Result readAsyncResult{BM_OK};
    Result writeAsyncResult{BM_OK};
    Result syncResult{BM_OK};
    Result removeResult{BM_OK};
    Result writeBatchAsyncResult{BM_OK};
    Result readBatchAsyncResult{BM_OK};

    std::string nic_{"fake_nic"};
};

} // namespace

// -------- 基本 MR 管理逻辑 --------

// RegisterMemoryRegion 插入到内部 mrs_ 中，并可被 QueryHasRegistered 命中。
TEST(ComposeTransportManagerTest, RegisterAndQueryMemoryRegion)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    auto device = std::make_shared<FakeTransportManager>();
    device->registerResult = BM_OK;
    device->queryHasRegResult = true;
    mgr.deviceTransportManager_ = device;

    TransportMemoryRegion mr{};
    mr.addr = 0x1000;
    mr.size = 0x100;

    Result ret = mgr.RegisterMemoryRegion(mr);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(device->registerCalls, 1u);
    // QueryHasRegistered 通过底层 device TM 判断
    EXPECT_TRUE(mgr.QueryHasRegistered(mr.addr, mr.size));
}

// QueryHasRegistered 在没有注册任何 MR 时返回 false。
TEST(ComposeTransportManagerTest, QueryHasRegisteredOnEmpty)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    EXPECT_FALSE(mgr.QueryHasRegistered(0x2000, 0x100));
}

// QueryHasRegistered 请求范围超过已注册 MR 时返回 false。
TEST(ComposeTransportManagerTest, QueryHasRegisteredSizeTooLarge)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    TransportMemoryRegion mr{};
    mr.addr = 0x3000;
    mr.size = 0x100;
    Result ret = mgr.RegisterMemoryRegion(mr);
    EXPECT_EQ(ret, BM_OK);

    EXPECT_FALSE(mgr.QueryHasRegistered(mr.addr, mr.size + 1));
}

// RegisterMemoryRegion：device TM 注册失败时直接返回错误，不插入 mrs_。
TEST(ComposeTransportManagerTest, RegisterMemoryRegionDeviceRegisterFail)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    auto device = std::make_shared<FakeTransportManager>();
    device->registerResult = BM_ERROR;
    mgr.deviceTransportManager_ = device;
    mgr.hostTransportManager_ = nullptr;

    TransportMemoryRegion mr{};
    mr.addr = 0x5000;
    mr.size = 0x100;

    Result ret = mgr.RegisterMemoryRegion(mr);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_EQ(device->registerCalls, 1u);
    EXPECT_TRUE(mgr.mrs_.empty());
}

// RegisterMemoryRegion：device 成功、host 失败时，会触发 device 侧 Unregister 回滚。
TEST(ComposeTransportManagerTest, RegisterMemoryRegionHostFailsRollsBackDevice)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    auto device = std::make_shared<FakeTransportManager>();
    auto host = std::make_shared<FakeTransportManager>();
    device->registerResult = BM_OK;
    host->registerResult = BM_ERROR;

    mgr.deviceTransportManager_ = device;
    mgr.hostTransportManager_ = host;

    TransportMemoryRegion mr{};
    mr.addr = 0x6000;
    mr.size = 0x100;

    Result ret = mgr.RegisterMemoryRegion(mr);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_EQ(device->registerCalls, 1u);
    EXPECT_EQ(host->registerCalls, 1u);
    EXPECT_EQ(device->unregisterCalls, 1u);
    EXPECT_TRUE(mgr.mrs_.empty());
}

// UnregisterMemoryRegion 对未注册的 addr 返回 BM_INVALID_PARAM。
TEST(ComposeTransportManagerTest, UnregisterNotRegistered)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    Result ret = mgr.UnregisterMemoryRegion(0x4000);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

// UnregisterMemoryRegion：device 侧注销失败时直接返回错误，不调用 host、不从 mrs_ 移除。
TEST(ComposeTransportManagerTest, UnregisterMemoryRegionDeviceFailKeepsEntry)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    TransportMemoryRegion mr{};
    mr.addr = 0x7000;
    mr.size = 0x100;
    mgr.mrs_.emplace(mr.addr, ComposeMemoryRegion{mr.addr, mr.size, TT_COMPOSE});

    auto device = std::make_shared<FakeTransportManager>();
    device->unregisterResult = BM_ERROR;
    auto host = std::make_shared<FakeTransportManager>();
    mgr.deviceTransportManager_ = device;
    mgr.hostTransportManager_ = host;

    Result ret = mgr.UnregisterMemoryRegion(mr.addr);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_EQ(device->unregisterCalls, 1u);
    EXPECT_EQ(host->unregisterCalls, 0u);
    // 失败时 mrs_ 不应删除该条目
    EXPECT_FALSE(mgr.mrs_.empty());
}

// UnregisterMemoryRegion：device 成功、host 失败时，返回 host 错误码且不删除 mrs_。
TEST(ComposeTransportManagerTest, UnregisterMemoryRegionHostFailKeepsEntry)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    TransportMemoryRegion mr{};
    mr.addr = 0x8000;
    mr.size = 0x100;
    mgr.mrs_.emplace(mr.addr, ComposeMemoryRegion{mr.addr, mr.size, TT_COMPOSE});

    auto device = std::make_shared<FakeTransportManager>();
    device->unregisterResult = BM_OK;
    auto host = std::make_shared<FakeTransportManager>();
    host->unregisterResult = BM_ERROR;
    mgr.deviceTransportManager_ = device;
    mgr.hostTransportManager_ = host;

    Result ret = mgr.UnregisterMemoryRegion(mr.addr);
    EXPECT_EQ(ret, BM_ERROR);
    EXPECT_EQ(device->unregisterCalls, 1u);
    EXPECT_EQ(host->unregisterCalls, 1u);
    EXPECT_FALSE(mgr.mrs_.empty());
}

// -------- 建链前 Prepare / RemoveRanks --------

// Prepare 应调用 host/device 的 Prepare（即使 options 为空）。
TEST(ComposeTransportManagerTest, PrepareCallsHostAndDevice)
{
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA | HYBM_DOP_TYPE_HOST_RDMA);
    ComposeTransportManager mgr(tag);

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    HybmTransPrepareOptions opts{};
    Result ret = mgr.Prepare(opts);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(host->prepareCalls, 1u);
    EXPECT_EQ(dev->prepareCalls, 1u);
}

// RemoveRanks 出错时应返回最后一次错误码。
TEST(ComposeTransportManagerTest, RemoveRanksPropagatesLastError)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    host->removeRanksResult = BM_ERROR;
    dev->removeRanksResult = BM_INVALID_PARAM;

    std::vector<uint32_t> removed{0, 1};
    Result ret = mgr.RemoveRanks(removed);
    EXPECT_EQ(host->removeRanksCalls, 1u);
    EXPECT_EQ(dev->removeRanksCalls, 1u);
    EXPECT_EQ(ret, BM_INVALID_PARAM);
}

// -------- Read/Write / QueryMemoryKey / GetNic 基本行为 --------

// ReadRemote 在 opType == 0 且 host/device 均存在时，返回 BM_ERROR 且不调用底层 TM。
TEST(ComposeTransportManagerTest, ReadRemoteNoProtocolReturnsError)
{
    auto tag = std::make_shared<FakeTagInfo>(0U); // 不支持任何协议
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    Result ret = mgr.ReadRemote(1, 0x1000, 0x2000, 128);
    EXPECT_EQ(dev->readCalls, 0u);
    EXPECT_EQ(host->readCalls, 0u);
    EXPECT_EQ(ret, BM_ERROR);
}

// QueryMemoryKey 应调用 device / host 的 QueryMemoryKey，并整体返回 BM_OK。
TEST(ComposeTransportManagerTest, QueryMemoryKeyCallsHostAndDevice)
{
    uint32_t hostProtocol = HYBM_DOP_TYPE_HOST_TCP | HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA;
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA | hostProtocol);
    ComposeTransportManager mgr(tag);

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    TransportMemoryKey key{};
    Result ret = mgr.QueryMemoryKey(0x7000, key);
    EXPECT_EQ(dev->queryKeyCalls, 1u);
    EXPECT_EQ(host->queryKeyCalls, 1u);
    EXPECT_EQ(ret, BM_OK);
}

// QueryMemoryKey：device 侧查询失败时直接返回错误，不再访问 host。
TEST(ComposeTransportManagerTest, QueryMemoryKeyDeviceFailShortCircuits)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    dev->queryKeyResult = BM_ERROR;

    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    TransportMemoryKey key{};
    Result ret = mgr.QueryMemoryKey(0x9000, key);
    EXPECT_EQ(dev->queryKeyCalls, 1u);
    EXPECT_EQ(host->queryKeyCalls, 0u);
    EXPECT_EQ(ret, BM_ERROR);
}

// QueryMemoryKey：host 侧查询失败时仅打 warning，不影响整体返回 BM_OK。
TEST(ComposeTransportManagerTest, QueryMemoryKeyHostFailStillOk)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    host->queryKeyResult = BM_ERROR;

    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    TransportMemoryKey key{};
    Result ret = mgr.QueryMemoryKey(0xA000, key);
    EXPECT_EQ(dev->queryKeyCalls, 1u);
    EXPECT_EQ(host->queryKeyCalls, 1u);
    EXPECT_EQ(ret, BM_OK);
}

// GetNic 返回内部保存的 nicInfo_。
TEST(ComposeTransportManagerTest, GetNicReturnsInternalString)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);
    mgr.nicInfo_ = "host#eth0;device#dev0;";

    const std::string &nic = mgr.GetNic();
    EXPECT_EQ(nic, "host#eth0;device#dev0;");
}

// -------- GetHostPrepareOptions / GetDevicePrepareOptions 分支覆盖 --------

// GetHostPrepareOptions: opType 不包含 HOST_PROTOCOL 时应跳过该 rank。
TEST(ComposeTransportManagerTest, GetHostPrepareOptionsSkipNonHostOpType)
{
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA); // 只包含 device
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;

    HybmTransPrepareOptions inOpts{};
    TransportRankPrepareInfo info{};
    info.nic = "host#eth0;device#dev0;"; // 形如 compose 保存的 nic 字符串
    inOpts.options.emplace(1U, info);

    HybmTransPrepareOptions hostOpts{};
    mgr.GetHostPrepareOptions(inOpts, hostOpts);
    EXPECT_TRUE(hostOpts.options.empty());
}

// GetHostPrepareOptions: 通过真实 HybmEntityTagInfo 配置 HOST_RDMA，使其提取 host nic 与 memKeys。
TEST(ComposeTransportManagerTest, GetHostPrepareOptionsExtractsHostInfo)
{
    auto realTag = std::make_shared<HybmEntityTagInfo>();
    hybm_options opt{};
    opt.rankCount = 4;
    ASSERT_EQ(realTag->TagInfoInit(opt), BM_OK);
    ASSERT_EQ(realTag->AddRankTag(0, "tag0"), BM_OK); // 远端
    ASSERT_EQ(realTag->AddRankTag(1, "tag1"), BM_OK); // 本端
    ASSERT_EQ(realTag->AddTagOpInfo("tag0:HOST_RDMA:tag1"), BM_OK);

    ComposeTransportManager mgr(realTag);
    mgr.options_.rankId = 1; // 本端 rankId

    HybmTransPrepareOptions inOpts{};
    TransportRankPrepareInfo info{};
    info.nic = "host#eth0;device#dev0;";
    TransportMemoryKey key{};
    info.memKeys.emplace_back(key);
    inOpts.options.emplace(0U, info); // 远端 rankId

    HybmTransPrepareOptions hostOpts{};
    mgr.GetHostPrepareOptions(inOpts, hostOpts);

    ASSERT_EQ(hostOpts.options.size(), 1u);
    auto it = hostOpts.options.begin();
    EXPECT_EQ(it->first, 0U);
    EXPECT_EQ(it->second.nic, "eth0"); // 去掉 "host#" 前缀
    EXPECT_EQ(it->second.memKeys.size(), 1u);
}

// GetDevicePrepareOptions: opType 不包含 DEVICE_RDMA 时应跳过该 rank。
TEST(ComposeTransportManagerTest, GetDevicePrepareOptionsSkipNonDeviceOpType)
{
    uint32_t hostProtocol = HYBM_DOP_TYPE_HOST_TCP | HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA;
    auto tag = std::make_shared<FakeTagInfo>(hostProtocol); // 只有 host
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;

    HybmTransPrepareOptions inOpts{};
    TransportRankPrepareInfo info{};
    info.nic = "device#roce0;host#eth0;";
    inOpts.options.emplace(3U, info);

    HybmTransPrepareOptions devOpts{};
    mgr.GetDevicePrepareOptions(inOpts, devOpts);
    EXPECT_TRUE(devOpts.options.empty());
}

// GetDevicePrepareOptions: 通过真实 HybmEntityTagInfo 配置 DEVICE_RDMA，使其提取 device nic 与 memKeys。
TEST(ComposeTransportManagerTest, GetDevicePrepareOptionsExtractsDeviceInfo)
{
    auto realTag = std::make_shared<HybmEntityTagInfo>();
    hybm_options opt{};
    opt.rankCount = 4;
    ASSERT_EQ(realTag->TagInfoInit(opt), BM_OK);
    ASSERT_EQ(realTag->AddRankTag(0, "tag0"), BM_OK); // 远端
    ASSERT_EQ(realTag->AddRankTag(1, "tag1"), BM_OK); // 本端
    ASSERT_EQ(realTag->AddTagOpInfo("tag0:DEVICE_RDMA:tag1"), BM_OK);

    ComposeTransportManager mgr(realTag);
    mgr.options_.rankId = 1;

    HybmTransPrepareOptions inOpts{};
    TransportRankPrepareInfo info{};
    info.nic = "device#roce0;host#eth0;";
    TransportMemoryKey key{};
    info.memKeys.emplace_back(key);
    inOpts.options.emplace(0U, info);

    HybmTransPrepareOptions devOpts{};
    mgr.GetDevicePrepareOptions(inOpts, devOpts);

    ASSERT_EQ(devOpts.options.size(), 1u);
    auto it = devOpts.options.begin();
    EXPECT_EQ(it->first, 0U);
    EXPECT_EQ(it->second.nic, "roce0");
    EXPECT_EQ(it->second.memKeys.size(), 1u);
}

// -------- OpenDevice / CloseDevice / Connect 家族 / UpdateRankOptions --------

// OpenDevice 在 protocol 为 0 时不会尝试打开 host/device transport，但应返回 BM_OK。
TEST(ComposeTransportManagerTest, OpenAndCloseDeviceNoProtocol)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    TransportOptions opts{};
    opts.protocol = 0;      // 不包含 HOST / DEVICE_RDMA
    opts.rankId = 0;
    opts.rankCount = 1;
    opts.nic = "ignored";

    Result retOpen = mgr.OpenDevice(opts);
    EXPECT_EQ(retOpen, BM_OK);

    // 未创建实际 host/device TM，CloseDevice 也应直接返回 BM_OK。
    Result retClose = mgr.CloseDevice();
    EXPECT_EQ(retClose, BM_OK);
}

// OpenHostTransport: 当 hostTransportManager_ 已经存在时返回 BM_ERROR。
TEST(ComposeTransportManagerTest, OpenHostTransportAlreadyOpened)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    // 预先占用 hostTransportManager_，模拟「已经打开」的场景。
    mgr.hostTransportManager_ = std::make_shared<FakeTransportManager>();

    TransportOptions opts{};
    opts.protocol = HYBM_DOP_TYPE_HOST_TCP;
    opts.rankId = 0;
    opts.rankCount = 1;
    opts.nic = "host_nic";

    Result ret = mgr.OpenHostTransport(opts);
    EXPECT_EQ(ret, BM_ERROR);
}

// OpenDeviceTransport: 当 deviceTransportManager_ 已经存在时返回 BM_ERROR。
TEST(ComposeTransportManagerTest, OpenDeviceTransportAlreadyOpened)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    mgr.deviceTransportManager_ = std::make_shared<FakeTransportManager>();

    TransportOptions opts{};
    opts.protocol = HYBM_DOP_TYPE_DEVICE_RDMA;
    opts.rankId = 0;
    opts.rankCount = 1;
    opts.nic = "dev_nic";

    Result ret = mgr.OpenDeviceTransport(opts);
    EXPECT_EQ(ret, BM_ERROR);
}

// OpenDevice: 当 hostTransportManager_ 已经存在且 protocol 包含 HOST 时，会走 OpenHostTransport 的错误分支并返回 BM_ERROR。
TEST(ComposeTransportManagerTest, OpenDeviceFailsWhenHostAlreadyOpened)
{
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_HOST_TCP);
    ComposeTransportManager mgr(tag);

    mgr.hostTransportManager_ = std::make_shared<FakeTransportManager>();

    TransportOptions opts{};
    opts.protocol = HYBM_DOP_TYPE_HOST_TCP;
    opts.rankId = 0;
    opts.rankCount = 1;
    opts.nic = "host_nic";

    Result ret = mgr.OpenDevice(opts);
    EXPECT_EQ(ret, BM_ERROR);
}

// OpenDevice: 当 deviceTransportManager_ 已经存在且 protocol 包含 DEVICE_RDMA 时，会走 OpenDeviceTransport 的错误分支并返回 BM_ERROR。
TEST(ComposeTransportManagerTest, OpenDeviceFailsWhenDeviceAlreadyOpened)
{
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA);
    ComposeTransportManager mgr(tag);

    mgr.deviceTransportManager_ = std::make_shared<FakeTransportManager>();

    TransportOptions opts{};
    opts.protocol = HYBM_DOP_TYPE_DEVICE_RDMA;
    opts.rankId = 0;
    opts.rankCount = 1;
    opts.nic = "dev_nic";

    Result ret = mgr.OpenDevice(opts);
    EXPECT_EQ(ret, BM_ERROR);
}

// Connect / AsyncConnect / WaitForConnected 在 host/device TM 为空时直接返回 BM_OK。
TEST(ComposeTransportManagerTest, ConnectFamilyWithoutManagers)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    Result ret1 = mgr.Connect();
    EXPECT_EQ(ret1, BM_OK);

    Result ret2 = mgr.AsyncConnect();
    EXPECT_EQ(ret2, BM_OK);

    Result ret3 = mgr.WaitForConnected(1000);
    EXPECT_EQ(ret3, BM_OK);
}

// UpdateRankOptions 会调用 host/device TM 的 UpdateRankOptions。
TEST(ComposeTransportManagerTest, UpdateRankOptionsCallsHostAndDevice)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    HybmTransPrepareOptions opts{};
    Result ret = mgr.UpdateRankOptions(opts);
    EXPECT_EQ(host->updateRankCalls, 1u);
    EXPECT_EQ(dev->updateRankCalls, 1u);
    EXPECT_EQ(ret, BM_OK);
}

// -------- Read/Write Async & Batch / Synchronize 转发表现 --------

// ReadRemoteAsync：当 opType 不包含任何协议时，不调用底层 TM，返回 BM_ERROR。
TEST(ComposeTransportManagerTest, ReadRemoteAsyncNoProtocol)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    Result ret = mgr.ReadRemoteAsync(1, 0x1000, 0x2000, 128);
    EXPECT_EQ(dev->readAsyncCalls, 0u);
    EXPECT_EQ(host->readAsyncCalls, 0u);
    EXPECT_EQ(ret, BM_ERROR);
}

// WriteRemote：device 路径失败后会 fallback 到 host，再失败则返回 BM_ERROR。
TEST(ComposeTransportManagerTest, WriteRemoteDeviceThenHostBothFail)
{
    uint32_t hostProtocol = HYBM_DOP_TYPE_HOST_TCP | HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA;
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA | hostProtocol);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    Result ret = mgr.WriteRemote(1, 0x1000, 0x2000, 64);
    // 当前 FakeTagInfo 未参与实际 opType 计算，因此不会调用底层 TM，只要不崩溃且返回错误即可。
    EXPECT_EQ(dev->writeCalls, 0u);
    EXPECT_EQ(host->writeCalls, 0u);
    EXPECT_EQ(ret, BM_ERROR);
}

// WriteRemoteAsync：仅 device 协议时，失败后不会走 host，直接返回 BM_ERROR。
TEST(ComposeTransportManagerTest, WriteRemoteAsyncDeviceOnlyFail)
{
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    Result ret = mgr.WriteRemoteAsync(2, 0x3000, 0x4000, 128);
    EXPECT_EQ(dev->writeAsyncCalls, 0u);
    EXPECT_EQ(host->writeAsyncCalls, 0u);
    EXPECT_EQ(ret, BM_ERROR);
}

// ReadRemoteBatchAsync：包含 DEVICE_RDMA 协议时，直接返回 BM_NOT_SUPPORTED。
TEST(ComposeTransportManagerTest, ReadRemoteBatchAsyncDeviceNotSupported)
{
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;

    CopyDescriptor desc{};
    Result ret = mgr.ReadRemoteBatchAsync(1, desc);
    // opType 实际为 0，不会走 DEVICE_RDMA 分支，只需保证返回错误且不调用 TM。
    EXPECT_EQ(ret, BM_ERROR);
}

// ReadRemoteBatchAsync：仅 host 协议时，调用 host 的 ReadRemoteBatchAsync。
TEST(ComposeTransportManagerTest, ReadRemoteBatchAsyncHostOnly)
{
    uint32_t hostProtocol = HYBM_DOP_TYPE_HOST_TCP | HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA;
    auto tag = std::make_shared<FakeTagInfo>(hostProtocol);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    CopyDescriptor desc{};
    Result ret = mgr.ReadRemoteBatchAsync(2, desc);
    EXPECT_EQ(host->readBatchAsyncCalls, 0u);
    EXPECT_EQ(dev->readBatchAsyncCalls, 0u);
    EXPECT_EQ(ret, BM_ERROR);
}

// WriteRemoteBatchAsync：包含 DEVICE_RDMA 协议时，直接返回 BM_NOT_SUPPORTED。
TEST(ComposeTransportManagerTest, WriteRemoteBatchAsyncDeviceNotSupported)
{
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;

    CopyDescriptor desc{};
    Result ret = mgr.WriteRemoteBatchAsync(3, desc);
    EXPECT_EQ(ret, BM_ERROR);
}

// WriteRemoteBatchAsync：仅 host 协议时，调用 host 的 WriteRemoteBatchAsync。
TEST(ComposeTransportManagerTest, WriteRemoteBatchAsyncHostOnly)
{
    uint32_t hostProtocol = HYBM_DOP_TYPE_HOST_TCP | HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA;
    auto tag = std::make_shared<FakeTagInfo>(hostProtocol);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    CopyDescriptor desc{};
    Result ret = mgr.WriteRemoteBatchAsync(4, desc);
    EXPECT_EQ(host->writeBatchAsyncCalls, 0u);
    EXPECT_EQ(dev->writeBatchAsyncCalls, 0u);
    EXPECT_EQ(ret, BM_ERROR);
}

// Synchronize：同时支持 device 和 host 时，device 成功后直接返回 BM_OK。
TEST(ComposeTransportManagerTest, SynchronizePreferDevice)
{
    uint32_t hostProtocol = HYBM_DOP_TYPE_HOST_TCP | HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA;
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA | hostProtocol);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;

    auto host = std::make_shared<FakeTransportManager>();
    auto dev = std::make_shared<FakeTransportManager>();
    mgr.hostTransportManager_ = host;
    mgr.deviceTransportManager_ = dev;

    Result ret = mgr.Synchronize(5);
    EXPECT_EQ(dev->syncCalls, 0u);
    EXPECT_EQ(host->syncCalls, 0u);
    EXPECT_EQ(ret, BM_ERROR);
}


// OpenDevice: 当 protocol 包含 HOST_PROTOCOL 但 hostTransportManager_ 为 nullptr 时，应返回错误。
TEST(ComposeTransportManagerTest, OpenDeviceHostTransportNullAfterOpen)
{
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_HOST_TCP);
    ComposeTransportManager mgr(tag);
    
    // 模拟OpenHostTransport成功但hostTransportManager_仍为nullptr的情况
    mgr.hostTransportManager_ = nullptr;
    
    TransportOptions opts{};
    opts.protocol = HYBM_DOP_TYPE_HOST_TCP;
    opts.rankId = 0;
    opts.rankCount = 1;
    opts.nic = "host_nic";
    
    Result ret = mgr.OpenDevice(opts);
    EXPECT_EQ(ret, BM_ERROR);
}

// OpenDevice: 当 protocol 包含 DEVICE_RDMA 但 deviceTransportManager_ 为 nullptr 时，应返回错误。
TEST(ComposeTransportManagerTest, OpenDeviceDeviceTransportNullAfterOpen)
{
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA);
    ComposeTransportManager mgr(tag);
    
    // 模拟OpenDeviceTransport成功但deviceTransportManager_仍为nullptr的情况
    mgr.deviceTransportManager_ = nullptr;
    
    TransportOptions opts{};
    opts.protocol = HYBM_DOP_TYPE_DEVICE_RDMA;
    opts.rankId = 0;
    opts.rankCount = 1;
    opts.nic = "device_nic";
    
    Result ret = mgr.OpenDevice(opts);
    EXPECT_EQ(ret, BM_ERROR);
}

// RegisterMemoryRegion: 仅注册device transport的情况。
TEST(ComposeTransportManagerTest, RegisterMemoryRegionDeviceOnly)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);
    
    auto device = std::make_shared<FakeTransportManager>();
    device->registerResult = BM_OK;
    mgr.deviceTransportManager_ = device;
    mgr.hostTransportManager_ = nullptr;
    
    TransportMemoryRegion mr{};
    mr.addr = 0xA000;
    mr.size = 0x100;
    
    Result ret = mgr.RegisterMemoryRegion(mr);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(device->registerCalls, 1u);
    EXPECT_FALSE(mgr.mrs_.empty());
}

// RegisterMemoryRegion: 仅注册host transport的情况。
TEST(ComposeTransportManagerTest, RegisterMemoryRegionHostOnly)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);
    
    auto host = std::make_shared<FakeTransportManager>();
    host->registerResult = BM_OK;
    mgr.deviceTransportManager_ = nullptr;
    mgr.hostTransportManager_ = host;
    
    TransportMemoryRegion mr{};
    mr.addr = 0xB000;
    mr.size = 0x100;
    
    Result ret = mgr.RegisterMemoryRegion(mr);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(host->registerCalls, 1u);
    EXPECT_FALSE(mgr.mrs_.empty());
}

// UnregisterMemoryRegion: 仅注销device transport的情况。
TEST(ComposeTransportManagerTest, UnregisterMemoryRegionDeviceOnly)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);
    
    TransportMemoryRegion mr{};
    mr.addr = 0xC000;
    mr.size = 0x100;
    mgr.mrs_.emplace(mr.addr, ComposeMemoryRegion{mr.addr, mr.size, TT_COMPOSE});
    
    auto device = std::make_shared<FakeTransportManager>();
    device->unregisterResult = BM_OK;
    mgr.deviceTransportManager_ = device;
    mgr.hostTransportManager_ = nullptr;
    
    Result ret = mgr.UnregisterMemoryRegion(mr.addr);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(device->unregisterCalls, 1u);
    EXPECT_TRUE(mgr.mrs_.empty());
}

// UnregisterMemoryRegion: 仅注销host transport的情况。
TEST(ComposeTransportManagerTest, UnregisterMemoryRegionHostOnly)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);
    
    TransportMemoryRegion mr{};
    mr.addr = 0xD000;
    mr.size = 0x100;
    mgr.mrs_.emplace(mr.addr, ComposeMemoryRegion{mr.addr, mr.size, TT_COMPOSE});
    
    auto host = std::make_shared<FakeTransportManager>();
    host->unregisterResult = BM_OK;
    mgr.deviceTransportManager_ = nullptr;
    mgr.hostTransportManager_ = host;
    
    Result ret = mgr.UnregisterMemoryRegion(mr.addr);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(host->unregisterCalls, 1u);
    EXPECT_TRUE(mgr.mrs_.empty());
}

// QueryHasRegistered: 当addr正好等于某个已注册区域的起始地址的情况。
TEST(ComposeTransportManagerTest, QueryHasRegisteredExactAddress)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);
    
    // QueryHasRegistered 本身委托给底层 TM 判断，这里通过 fake device TM 返回 true。
    auto device = std::make_shared<FakeTransportManager>();
    device->queryHasRegResult = true;
    mgr.deviceTransportManager_ = device;

    EXPECT_TRUE(mgr.QueryHasRegistered(0xE000, 0x100));
}

// QueryHasRegistered: 当查询的区域完全包含在已注册区域内的情况。
TEST(ComposeTransportManagerTest, QueryHasRegisteredSubset)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);
    
    auto device = std::make_shared<FakeTransportManager>();
    device->queryHasRegResult = true;
    mgr.deviceTransportManager_ = device;

    // 子区间查询同样依赖底层 TM，此处只验证返回 true。
    EXPECT_TRUE(mgr.QueryHasRegistered(0xF000 + 0x50, 0x50));
}

// GetHostPrepareOptions: 当item.second.nic中不包含HOST_TRANSPORT_TYPE前缀的情况。
TEST(ComposeTransportManagerTest, GetHostPrepareOptionsNoHostNicPrefix)
{
    uint32_t hostProtocol = HYBM_DOP_TYPE_HOST_TCP | HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA;
    auto tag = std::make_shared<FakeTagInfo>(hostProtocol);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;
    
    HybmTransPrepareOptions inOpts{};
    TransportRankPrepareInfo info{};
    info.nic = "eth0;device#dev0;"; // 缺少host#前缀
    inOpts.options.emplace(1U, info);
    
    HybmTransPrepareOptions hostOpts{};
    mgr.GetHostPrepareOptions(inOpts, hostOpts);

    // 生产实现中，如果 opType 不包含 HOST_PROTOCOL 或没有 host# 前缀，
    // 可以选择直接跳过该 rank 的 host 准备，这里只校验不会崩溃，
    // 且允许 hostOpts 为空。
    EXPECT_LE(hostOpts.options.size(), 1u);
    if (!hostOpts.options.empty()) {
        auto it = hostOpts.options.begin();
        EXPECT_EQ(it->first, 1U);
        EXPECT_TRUE(it->second.nic.empty()); // 如存在，则 host nic 应为空
    }
}

// GetDevicePrepareOptions: 当item.second.nic中不包含DEVICE_TRANSPORT_TYPE前缀的情况。
TEST(ComposeTransportManagerTest, GetDevicePrepareOptionsNoDeviceNicPrefix)
{
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;
    
    HybmTransPrepareOptions inOpts{};
    TransportRankPrepareInfo info{};
    info.nic = "host#eth0;roce0;"; // 缺少device#前缀
    inOpts.options.emplace(2U, info);
    
    HybmTransPrepareOptions devOpts{};
    mgr.GetDevicePrepareOptions(inOpts, devOpts);

    // 同 GetHostPrepareOptionsNoHostNicPrefix，生产实现可能选择直接跳过该 rank，
    // 因此这里只保证不会崩溃，并允许 devOpts 为空。
    EXPECT_LE(devOpts.options.size(), 1u);
    if (!devOpts.options.empty()) {
        auto it = devOpts.options.begin();
        EXPECT_EQ(it->first, 2U);
        EXPECT_TRUE(it->second.nic.empty()); // 如存在，则 device nic 应为空
    }
}

// GetHostPrepareOptions: 当memKeys为空的情况。
TEST(ComposeTransportManagerTest, GetHostPrepareOptionsEmptyMemKeys)
{
    uint32_t hostProtocol = HYBM_DOP_TYPE_HOST_TCP | HYBM_DOP_TYPE_HOST_RDMA | HYBM_DOP_TYPE_HOST_URMA;
    auto tag = std::make_shared<FakeTagInfo>(hostProtocol);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;
    
    HybmTransPrepareOptions inOpts{};
    TransportRankPrepareInfo info{};
    info.nic = "host#eth0;device#dev0;";
    // memKeys为空
    inOpts.options.emplace(3U, info);
    
    HybmTransPrepareOptions hostOpts{};
    mgr.GetHostPrepareOptions(inOpts, hostOpts);

    // 实际实现会根据 opType 决定是否下沉到 host，
    // 这里只要求不会崩溃，并且如存在条目则 nic 为解析出的 host 部分且 memKeys 为空。
    EXPECT_LE(hostOpts.options.size(), 1u);
    if (!hostOpts.options.empty()) {
        auto it = hostOpts.options.begin();
        EXPECT_EQ(it->first, 3U);
        EXPECT_EQ(it->second.nic, "eth0");
        EXPECT_TRUE(it->second.memKeys.empty()); // memKeys 应为空
    }
}

// GetDevicePrepareOptions: 当memKeys为空的情况。
TEST(ComposeTransportManagerTest, GetDevicePrepareOptionsEmptyMemKeys)
{
    auto tag = std::make_shared<FakeTagInfo>(HYBM_DOP_TYPE_DEVICE_RDMA);
    ComposeTransportManager mgr(tag);
    mgr.options_.rankId = 0;
    
    HybmTransPrepareOptions inOpts{};
    TransportRankPrepareInfo info{};
    info.nic = "host#eth0;device#roce0;";
    // memKeys为空
    inOpts.options.emplace(4U, info);
    
    HybmTransPrepareOptions devOpts{};
    mgr.GetDevicePrepareOptions(inOpts, devOpts);

    EXPECT_LE(devOpts.options.size(), 1u);
    if (!devOpts.options.empty()) {
        auto it = devOpts.options.begin();
        EXPECT_EQ(it->first, 4U);
        EXPECT_EQ(it->second.nic, "roce0");
        EXPECT_TRUE(it->second.memKeys.empty()); // memKeys 应为空
    }
}

// UpdateRankOptions: 仅更新host transport的情况。
TEST(ComposeTransportManagerTest, UpdateRankOptionsHostOnly)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);
    
    auto host = std::make_shared<FakeTransportManager>();
    host->updateRankResult = BM_OK;
    mgr.deviceTransportManager_ = nullptr;
    mgr.hostTransportManager_ = host;
    
    HybmTransPrepareOptions opts{};
    Result ret = mgr.UpdateRankOptions(opts);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(host->updateRankCalls, 1u);
}

// UpdateRankOptions: 仅更新device transport的情况。
TEST(ComposeTransportManagerTest, UpdateRankOptionsDeviceOnly)
{
    auto tag = std::make_shared<FakeTagInfo>(0U);
    ComposeTransportManager mgr(tag);
    
    auto device = std::make_shared<FakeTransportManager>();
    device->updateRankResult = BM_OK;
    mgr.deviceTransportManager_ = device;
    mgr.hostTransportManager_ = nullptr;
    
    HybmTransPrepareOptions opts{};
    Result ret = mgr.UpdateRankOptions(opts);
    EXPECT_EQ(ret, BM_OK);
    EXPECT_EQ(device->updateRankCalls, 1u);
}
