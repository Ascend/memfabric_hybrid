/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <cstring>

#include <gtest/gtest.h>

#define private   public
#define protected public
#include "device/urma/device_urma_transport_manager.h"
#include "dl_hccl_api.h"
#include "dl_hcomm_api.h"
#undef private
#undef protected

using namespace ock::mf;
using namespace ock::mf::transport::device;
using ock::mf::transport::HybmTransPrepareOptions;
using ock::mf::transport::REG_MR_FLAG_DRAM;
using ock::mf::transport::REG_MR_FLAG_HBM;
using ock::mf::transport::TransportMemoryKey;
using ock::mf::transport::TransportMemoryRegion;
using ock::mf::transport::TransportPrivateData;
using ock::mf::transport::TransportRankPrepareInfo;

namespace {
const EndpointHandle MOCK_ENDPOINT = reinterpret_cast<EndpointHandle>(0xA501UL);
const HcommMemHandle MOCK_MEM_HANDLE = reinterpret_cast<HcommMemHandle>(0xA502UL);
constexpr ChannelHandle MOCK_CHANNEL = 0xA503UL;
constexpr HcommThreadHandle MOCK_THREAD = 0xA504UL;
constexpr uint64_t MOCK_LOCAL_ADDR = 0x100000UL;
constexpr uint64_t MOCK_REMOTE_ADDR = 0x200000UL;
constexpr uint64_t MOCK_SIZE = 0x1000UL;
constexpr uint64_t MOCK_MEM_TAG = 7UL;
constexpr uint32_t MOCK_HCOMM_DESC_LEN = 4U;

struct DlHcommApiFnGuard {
    hcommEndpointCreateFunc oldEndpointCreate{DlHcommApi::gHcommEndpointCreate};
    hcommEndpointDestroyFunc oldEndpointDestroy{DlHcommApi::gHcommEndpointDestroy};
    hcommMemRegFunc oldMemReg{DlHcommApi::gHcommMemReg};
    hcommMemUnregFunc oldMemUnreg{DlHcommApi::gHcommMemUnreg};
    hcommMemExportFunc oldMemExport{DlHcommApi::gHcommMemExport};
    hcommMemImportFunc oldMemImport{DlHcommApi::gHcommMemImport};
    hcommMemUnimportFunc oldMemUnimport{DlHcommApi::gHcommMemUnimport};
    hcommChannelCreateFunc oldChannelCreate{DlHcommApi::gHcommChannelCreate};
    hcommChannelDestroyFunc oldChannelDestroy{DlHcommApi::gHcommChannelDestroy};
    hcommThreadAllocFunc oldThreadAlloc{DlHcommApi::gHcommThreadAlloc};
    hcommThreadFreeFunc oldThreadFree{DlHcommApi::gHcommThreadFree};
    hcommReadOnThreadFunc oldReadOnThread{DlHcommApi::gHcommReadOnThread};
    hcommWriteOnThreadFunc oldWriteOnThread{DlHcommApi::gHcommWriteOnThread};
    hcommChannelFenceOnThreadFunc oldChannelFenceOnThread{DlHcommApi::gHcommChannelFenceOnThread};

    ~DlHcommApiFnGuard()
    {
        DlHcommApi::gHcommEndpointCreate = oldEndpointCreate;
        DlHcommApi::gHcommEndpointDestroy = oldEndpointDestroy;
        DlHcommApi::gHcommMemReg = oldMemReg;
        DlHcommApi::gHcommMemUnreg = oldMemUnreg;
        DlHcommApi::gHcommMemExport = oldMemExport;
        DlHcommApi::gHcommMemImport = oldMemImport;
        DlHcommApi::gHcommMemUnimport = oldMemUnimport;
        DlHcommApi::gHcommChannelCreate = oldChannelCreate;
        DlHcommApi::gHcommChannelDestroy = oldChannelDestroy;
        DlHcommApi::gHcommThreadAlloc = oldThreadAlloc;
        DlHcommApi::gHcommThreadFree = oldThreadFree;
        DlHcommApi::gHcommReadOnThread = oldReadOnThread;
        DlHcommApi::gHcommWriteOnThread = oldWriteOnThread;
        DlHcommApi::gHcommChannelFenceOnThread = oldChannelFenceOnThread;
    }
};

UrmaEndpointDesc MakeEndpointDesc()
{
    UrmaEndpointDesc desc{};
    desc.devPhyId = 2UL;
    desc.superDevId = 2UL;
    desc.serverIdx = 3UL;
    desc.superPodIdx = 4UL;
    desc.protocol = UrmaProtocol::UBC_TP;
    desc.type = COMM_ADDR_TYPE_EID;
    for (uint32_t i = 0; i < COMM_ADDR_EID_LEN; ++i) {
        desc.raws[i] = static_cast<uint8_t>(i + 1);
    }
    return desc;
}

int32_t MockHcommEndpointCreate(const EndpointDesc *endpoint, EndpointHandle *endpointHandle)
{
    EXPECT_NE(endpoint, nullptr);
    EXPECT_NE(endpointHandle, nullptr);
    EXPECT_EQ(endpoint->protocol, COMM_PROTOCOL_UBC_TP);
    EXPECT_EQ(endpoint->commAddr.type, COMM_ADDR_TYPE_EID);
    EXPECT_EQ(endpoint->loc.locType, ENDPOINT_LOC_TYPE_DEVICE);
    EXPECT_EQ(endpoint->loc.device.devPhyId, 2U);
    EXPECT_EQ(endpoint->loc.device.superDevId, 2U);
    EXPECT_EQ(endpoint->loc.device.serverIdx, 3U);
    EXPECT_EQ(endpoint->loc.device.superPodIdx, 4U);
    *endpointHandle = MOCK_ENDPOINT;
    return BM_OK;
}

int32_t MockHcommEndpointDestroy(EndpointHandle endpoint)
{
    EXPECT_EQ(endpoint, MOCK_ENDPOINT);
    return BM_OK;
}

int32_t MockHcommMemReg(EndpointHandle endpoint, const char *memTag, const HcommCommMem *mem, HcommMemHandle *memHandle)
{
    EXPECT_EQ(endpoint, MOCK_ENDPOINT);
    EXPECT_STREQ(memTag, "7");
    EXPECT_NE(mem, nullptr);
    EXPECT_EQ(mem->type, COMM_MEM_TYPE_HOST);
    EXPECT_EQ(mem->addr, reinterpret_cast<void *>(MOCK_LOCAL_ADDR));
    EXPECT_EQ(mem->size, MOCK_SIZE);
    EXPECT_NE(memHandle, nullptr);
    *memHandle = MOCK_MEM_HANDLE;
    return BM_OK;
}

int32_t MockHcommMemUnreg(EndpointHandle endpoint, HcommMemHandle memHandle)
{
    EXPECT_EQ(endpoint, MOCK_ENDPOINT);
    EXPECT_EQ(memHandle, MOCK_MEM_HANDLE);
    return BM_OK;
}

int32_t MockHcommMemExport(EndpointHandle endpoint, HcommMemHandle memHandle, void **memDesc, uint32_t *memDescLen)
{
    static uint8_t desc[] = {0xA5, 0x01, 0x02, 0x03};
    EXPECT_EQ(endpoint, MOCK_ENDPOINT);
    EXPECT_EQ(memHandle, MOCK_MEM_HANDLE);
    EXPECT_NE(memDesc, nullptr);
    EXPECT_NE(memDescLen, nullptr);
    *memDesc = desc;
    *memDescLen = sizeof(desc);
    return BM_OK;
}

int32_t MockHcommMemImport(EndpointHandle endpoint, const void *memDesc, uint32_t descLen, HcommCommMem *commMem)
{
    EXPECT_EQ(endpoint, MOCK_ENDPOINT);
    EXPECT_NE(memDesc, nullptr);
    EXPECT_EQ(descLen, MOCK_HCOMM_DESC_LEN);
    EXPECT_NE(commMem, nullptr);
    commMem->type = COMM_MEM_TYPE_HOST;
    commMem->addr = reinterpret_cast<void *>(MOCK_REMOTE_ADDR);
    commMem->size = MOCK_SIZE;
    return BM_OK;
}

int32_t MockHcommMemUnimport(EndpointHandle endpoint, const void *memDesc, uint32_t descLen)
{
    EXPECT_EQ(endpoint, MOCK_ENDPOINT);
    EXPECT_NE(memDesc, nullptr);
    EXPECT_EQ(descLen, MOCK_HCOMM_DESC_LEN);
    return BM_OK;
}

int32_t MockHcommChannelCreate(EndpointHandle endpoint, CommEngine engine, HcommChannelDesc *channelDescs,
                               uint32_t channelNum, ChannelHandle *channels)
{
    EXPECT_EQ(endpoint, MOCK_ENDPOINT);
    EXPECT_EQ(engine, COMM_ENGINE_AICPU);
    EXPECT_NE(channelDescs, nullptr);
    EXPECT_EQ(channelNum, 1U);
    EXPECT_NE(channels, nullptr);
    EXPECT_EQ(channelDescs->header.version, HCOMM_CHANNEL_VERSION);
    EXPECT_EQ(channelDescs->header.magicWord, HCOMM_CHANNEL_MAGIC_WORD);
    EXPECT_EQ(channelDescs->header.size, sizeof(HcommChannelDesc));
    EXPECT_TRUE(channelDescs->exchangeAllMems);
    EXPECT_EQ(channelDescs->remoteEndpoint.protocol, COMM_PROTOCOL_UBC_TP);
    *channels = MOCK_CHANNEL;
    return BM_OK;
}

TransportPrivateData MakePrivateData(const UrmaEndpointDesc &desc)
{
    TransportPrivateData data{};
    struct Header {
        uint32_t magic;
        uint16_t version;
        uint16_t payloadLen;
    } header{0xA5FAC003U, 1U, static_cast<uint16_t>(sizeof(UrmaEndpointDesc))};
    std::memcpy(data.key.keys, &header, sizeof(header));
    std::memcpy(reinterpret_cast<uint8_t *>(data.key.keys) + sizeof(header), &desc, sizeof(desc));
    return data;
}

TransportMemoryKey MakeImportKey(uint64_t remoteAddr, uint64_t size, uint64_t memTag)
{
    TransportMemoryKey key{};
    key.keys[0] = URMA_EXPORT_DESC_MAGIC;
    key.keys[1] = remoteAddr;
    key.keys[2] = size;
    key.keys[3] = memTag;
    UrmaExportDesc exportDesc{};
    exportDesc.headerSize = sizeof(UrmaExportDesc);
    exportDesc.memoryType = UrmaMemoryType::HOST_DRAM;
    exportDesc.addr = remoteAddr;
    exportDesc.size = size;
    exportDesc.memTag = memTag;
    exportDesc.hcommDescLen = MOCK_HCOMM_DESC_LEN;
    std::memcpy(reinterpret_cast<uint8_t *>(&key.keys[4U]), &exportDesc, sizeof(exportDesc));
    std::memset(reinterpret_cast<uint8_t *>(&key.keys[4U]) + sizeof(exportDesc), 0, MOCK_HCOMM_DESC_LEN);
    static_assert(sizeof(UrmaExportDesc) + MOCK_HCOMM_DESC_LEN <= sizeof(key.keys) - 4 * sizeof(uint64_t));
    return key;
}

int32_t MockHcommChannelDestroy(const ChannelHandle *channels, uint32_t channelNum)
{
    EXPECT_NE(channels, nullptr);
    EXPECT_EQ(channelNum, 1U);
    EXPECT_EQ(channels[0], MOCK_CHANNEL);
    return BM_OK;
}

int32_t MockHcommThreadAlloc(CommEngine engine, uint32_t threadNum, const uint32_t *notifyNumPerThread,
                             ThreadHandle *threads)
{
    EXPECT_EQ(engine, COMM_ENGINE_AICPU);
    EXPECT_EQ(threadNum, 1U);
    EXPECT_NE(notifyNumPerThread, nullptr);
    EXPECT_NE(threads, nullptr);
    *threads = MOCK_THREAD;
    return BM_OK;
}

int32_t MockHcommThreadFree(const ThreadHandle *threads, uint32_t threadNum)
{
    EXPECT_NE(threads, nullptr);
    EXPECT_EQ(threadNum, 1U);
    EXPECT_EQ(threads[0], MOCK_THREAD);
    return BM_OK;
}

int32_t MockHcommReadOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    EXPECT_EQ(thread, MOCK_THREAD);
    EXPECT_EQ(channel, MOCK_CHANNEL);
    EXPECT_EQ(dst, reinterpret_cast<void *>(MOCK_LOCAL_ADDR));
    EXPECT_EQ(src, reinterpret_cast<const void *>(MOCK_REMOTE_ADDR));
    EXPECT_EQ(len, MOCK_SIZE);
    return BM_OK;
}

int32_t MockHcommWriteOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    EXPECT_EQ(thread, MOCK_THREAD);
    EXPECT_EQ(channel, MOCK_CHANNEL);
    EXPECT_EQ(dst, reinterpret_cast<void *>(MOCK_REMOTE_ADDR));
    EXPECT_EQ(src, reinterpret_cast<const void *>(MOCK_LOCAL_ADDR));
    EXPECT_EQ(len, MOCK_SIZE);
    return BM_OK;
}

int32_t MockHcommChannelFenceOnThread(ThreadHandle thread, ChannelHandle channel)
{
    EXPECT_EQ(thread, MOCK_THREAD);
    EXPECT_EQ(channel, MOCK_CHANNEL);
    return BM_OK;
}

int32_t MockHcommEndpointCreateFail(const EndpointDesc *, EndpointHandle *)
{
    return BM_DL_FUNCTION_FAILED;
}
} // namespace

// ============================================================================
// UrmaManagerTransport tests
// ============================================================================

TEST(DeviceUrmaTransportManagerTest, CreateEndpointCallsHcommEndpointCreate)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;

    UrmaManagerTransport manager;
    const auto endpoint = manager.CreateEndpoint(MakeEndpointDesc());
    EXPECT_NE(endpoint, nullptr);
}

TEST(DeviceUrmaTransportManagerTest, CreateEndpointReturnsNullOnHcommFailure)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreateFail;

    UrmaManagerTransport manager;
    const auto endpoint = manager.CreateEndpoint(MakeEndpointDesc());
    EXPECT_EQ(endpoint, nullptr);
}

TEST(DeviceUrmaTransportManagerTest, RegisterLocalMemoryCallsHcommMemReg)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;
    DlHcommApi::gHcommMemReg = MockHcommMemReg;

    UrmaManagerTransport manager;
    const auto endpoint = manager.CreateEndpoint(MakeEndpointDesc());
    ASSERT_NE(endpoint, nullptr);

    HcommMemHandle memHandle = nullptr;
    const UrmaCommMem mem{MOCK_LOCAL_ADDR, MOCK_SIZE, UrmaMemoryType::HOST_DRAM};
    EXPECT_EQ(manager.HcommMemReg(endpoint, 7, mem, &memHandle), BM_OK);
    EXPECT_EQ(memHandle, MOCK_MEM_HANDLE);
}

TEST(DeviceUrmaTransportManagerTest, DeregisterLocalMemoryCallsHcommMemUnreg)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;
    DlHcommApi::gHcommMemReg = MockHcommMemReg;
    DlHcommApi::gHcommMemUnreg = MockHcommMemUnreg;

    UrmaManagerTransport manager;
    const auto endpoint = manager.CreateEndpoint(MakeEndpointDesc());
    ASSERT_NE(endpoint, nullptr);

    HcommMemHandle memHandle = nullptr;
    const UrmaCommMem mem{MOCK_LOCAL_ADDR, MOCK_SIZE, UrmaMemoryType::HOST_DRAM};
    EXPECT_EQ(manager.HcommMemReg(endpoint, 7, mem, &memHandle), BM_OK);
    ASSERT_EQ(memHandle, MOCK_MEM_HANDLE);

    EXPECT_EQ(manager.HcommMemUnreg(endpoint, memHandle), BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, ExportLocalMemoryCallsHcommMemExport)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;
    DlHcommApi::gHcommMemReg = MockHcommMemReg;
    DlHcommApi::gHcommMemExport = MockHcommMemExport;

    UrmaManagerTransport manager;
    const auto endpoint = manager.CreateEndpoint(MakeEndpointDesc());
    ASSERT_NE(endpoint, nullptr);

    HcommMemHandle memHandle = nullptr;
    const UrmaCommMem mem{MOCK_LOCAL_ADDR, MOCK_SIZE, UrmaMemoryType::HOST_DRAM};
    EXPECT_EQ(manager.HcommMemReg(endpoint, 7, mem, &memHandle), BM_OK);
    ASSERT_EQ(memHandle, MOCK_MEM_HANDLE);

    const uint8_t *memDesc = nullptr;
    uint32_t memDescLen = 0;
    EXPECT_EQ(manager.HcommMemExport(endpoint, memHandle, &memDesc, &memDescLen), BM_OK);
    EXPECT_NE(memDesc, nullptr);
    EXPECT_GT(memDescLen, 0U);
}

TEST(DeviceUrmaTransportManagerTest, ImportAndUnimportMemoryWorkflow)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;
    DlHcommApi::gHcommMemReg = MockHcommMemReg;
    DlHcommApi::gHcommMemExport = MockHcommMemExport;
    DlHcommApi::gHcommMemImport = MockHcommMemImport;
    DlHcommApi::gHcommMemUnimport = MockHcommMemUnimport;

    UrmaManagerTransport manager;
    const auto endpoint = manager.CreateEndpoint(MakeEndpointDesc());
    ASSERT_NE(endpoint, nullptr);

    HcommMemHandle memHandle = nullptr;
    const UrmaCommMem mem{MOCK_LOCAL_ADDR, MOCK_SIZE, UrmaMemoryType::HOST_DRAM};
    EXPECT_EQ(manager.HcommMemReg(endpoint, 7, mem, &memHandle), BM_OK);
    ASSERT_EQ(memHandle, MOCK_MEM_HANDLE);

    const uint8_t *exportDesc = nullptr;
    uint32_t exportDescLen = 0;
    EXPECT_EQ(manager.HcommMemExport(endpoint, memHandle, &exportDesc, &exportDescLen), BM_OK);
    ASSERT_NE(exportDesc, nullptr);
    ASSERT_GT(exportDescLen, sizeof(UrmaExportDesc));

    UrmaCommMem importedView{};
    EXPECT_EQ(manager.HcommMemImport(endpoint, exportDesc, exportDescLen, &importedView), BM_OK);
    EXPECT_EQ(importedView.addr, MOCK_REMOTE_ADDR);
    EXPECT_EQ(importedView.size, MOCK_SIZE);
    EXPECT_EQ(importedView.type, UrmaMemoryType::HOST_DRAM);

    EXPECT_EQ(manager.HcommMemUnimport(endpoint, exportDesc, exportDescLen), BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, GetPrivateDataEncodesLocalEndpointDesc)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;

    DeviceUrmaTransportManager manager;
    manager.localEndpoint_ = manager.manager_.CreateEndpoint(MakeEndpointDesc());
    ASSERT_NE(manager.localEndpoint_, nullptr);
    manager.localEndpointDesc_ = MakeEndpointDesc();

    const auto data = manager.GetPrivateData();
    const auto *raw = reinterpret_cast<const uint8_t *>(data.key.keys);
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t payloadLen = 0;
    std::memcpy(&magic, raw, sizeof(magic));
    std::memcpy(&version, raw + sizeof(magic), sizeof(version));
    std::memcpy(&payloadLen, raw + sizeof(magic) + sizeof(version), sizeof(payloadLen));
    EXPECT_EQ(magic, 0xA5FAC003U);
    EXPECT_EQ(version, 1U);
    EXPECT_EQ(payloadLen, sizeof(UrmaEndpointDesc));
}

TEST(DeviceUrmaTransportManagerTest, PrepareCreatesThreadChannelAndImportsMemKeys)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;
    DlHcommApi::gHcommThreadAlloc = MockHcommThreadAlloc;
    DlHcommApi::gHcommChannelCreate = MockHcommChannelCreate;
    DlHcommApi::gHcommMemImport = MockHcommMemImport;
    DlHcommApi::gHcommMemUnimport = MockHcommMemUnimport;
    DlHcommApi::gHcommChannelDestroy = MockHcommChannelDestroy;
    DlHcommApi::gHcommThreadFree = MockHcommThreadFree;

    DeviceUrmaTransportManager manager;
    manager.opened_ = true;
    manager.rankId_ = 0;
    manager.rankCount_ = 2;
    manager.localEndpoint_ = manager.manager_.CreateEndpoint(MakeEndpointDesc());
    ASSERT_NE(manager.localEndpoint_, nullptr);
    manager.localEndpointDesc_ = MakeEndpointDesc();

    const auto peerDesc = MakeEndpointDesc();
    auto privateData = MakePrivateData(peerDesc);
    auto key = MakeImportKey(MOCK_REMOTE_ADDR, MOCK_SIZE, MOCK_MEM_TAG);
    HybmTransPrepareOptions options{};
    TransportRankPrepareInfo info{};
    info.nic = "tcp://127.0.0.1:8000";
    info.privateData = privateData;
    info.role = HYBM_ROLE_PEER;
    info.memKeys = {key};
    options.options.emplace(1, std::move(info));

    EXPECT_EQ(manager.Prepare(options), BM_OK);
    auto &state = manager.remoteRanks_[1];
    EXPECT_EQ(state.thread, MOCK_THREAD);
    ASSERT_EQ(state.channels.size(), 1U);
    EXPECT_EQ(state.channels.front(), MOCK_CHANNEL);
    ASSERT_EQ(state.imports.size(), 1U);
    EXPECT_EQ(state.imports.front().memTag, MOCK_MEM_TAG);
    EXPECT_EQ(state.imports.front().addr, MOCK_REMOTE_ADDR);
}

// ============================================================================
// DeviceUrmaTransportManager tests — kernel failure / sync paths
// ============================================================================

TEST(DeviceUrmaTransportManagerTest, ReadRemoteAsyncPropagatesKernelLoadFailure)
{
    DeviceUrmaTransportManager manager;
    // deviceKernelLoaded_ defaults to false -> EnsureDeviceKernelLoadedLocked fails
    const std::vector<uint64_t> localAddrs = {MOCK_LOCAL_ADDR};
    const std::vector<uint64_t> remoteAddrs = {MOCK_REMOTE_ADDR};
    const std::vector<uint64_t> sizes = {MOCK_SIZE};
    const auto ret = manager.LaunchDeviceKernelBatch(MOCK_THREAD, false, MOCK_CHANNEL, localAddrs, remoteAddrs, sizes);
    EXPECT_NE(ret, BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, WriteRemoteAsyncPropagatesKernelLoadFailure)
{
    DeviceUrmaTransportManager manager;
    const std::vector<uint64_t> localAddrs = {MOCK_LOCAL_ADDR};
    const std::vector<uint64_t> remoteAddrs = {MOCK_REMOTE_ADDR};
    const std::vector<uint64_t> sizes = {MOCK_SIZE};
    const auto ret = manager.LaunchDeviceKernelBatch(MOCK_THREAD, true, MOCK_CHANNEL, localAddrs, remoteAddrs, sizes);
    EXPECT_NE(ret, BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, SynchronizeNoOpsSucceeds)
{
    DeviceUrmaTransportManager manager;
    manager.opened_ = true;
    auto &state = manager.remoteRanks_[0];
    state.pendingOps = 0;
    EXPECT_EQ(manager.Synchronize(0), BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, SynchronizeUnknownRankFails)
{
    DeviceUrmaTransportManager manager;
    manager.opened_ = true;
    EXPECT_NE(manager.Synchronize(0), BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, SynchronizeNotOpenedFails)
{
    DeviceUrmaTransportManager manager;
    EXPECT_NE(manager.Synchronize(0), BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, LaunchDeviceKernelBatchRejectsZeroChannel)
{
    DeviceUrmaTransportManager manager;
    const std::vector<uint64_t> localAddrs = {MOCK_LOCAL_ADDR};
    const std::vector<uint64_t> remoteAddrs = {MOCK_REMOTE_ADDR};
    const std::vector<uint64_t> sizes = {MOCK_SIZE};
    EXPECT_EQ(manager.LaunchDeviceKernelBatch(MOCK_THREAD, false, 0, localAddrs, remoteAddrs, sizes), BM_NOT_CONNECTED);
}

TEST(DeviceUrmaTransportManagerTest, LaunchDeviceKernelBatchRejectsZeroThread)
{
    DeviceUrmaTransportManager manager;
    const std::vector<uint64_t> localAddrs = {MOCK_LOCAL_ADDR};
    const std::vector<uint64_t> remoteAddrs = {MOCK_REMOTE_ADDR};
    const std::vector<uint64_t> sizes = {MOCK_SIZE};
    EXPECT_EQ(manager.LaunchDeviceKernelBatch(0, false, MOCK_CHANNEL, localAddrs, remoteAddrs, sizes),
              BM_NOT_CONNECTED);
}

// ============================================================================
// DeviceUrmaTransportManager tests — supplemental validation & edge cases
// ============================================================================

TEST(DeviceUrmaTransportManagerTest, GetPrivateDataBeforeEndpointReadyReturnsEmpty)
{
    DeviceUrmaTransportManager manager;
    // localEndpoint_ is null by default — GetPrivateData must not crash
    const auto data = manager.GetPrivateData();
    // Expect empty key (all zeros) — peer will reject with magic mismatch
    bool allZero = true;
    for (const auto &k : data.key.keys) {
        if (k != 0) {
            allZero = false;
            break;
        }
    }
    EXPECT_TRUE(allZero);
}

TEST(DeviceUrmaTransportManagerTest, GetPrivateDataContainsValidEndpointDesc)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;

    DeviceUrmaTransportManager manager;
    manager.localEndpoint_ = manager.manager_.CreateEndpoint(MakeEndpointDesc());
    ASSERT_NE(manager.localEndpoint_, nullptr);
    const auto expectedDesc = MakeEndpointDesc();
    manager.localEndpointDesc_ = expectedDesc;

    const auto data = manager.GetPrivateData();
    const auto *raw = reinterpret_cast<const uint8_t *>(data.key.keys);
    // Check header (magic=0xA5FAC003, version=1, payloadLen=sizeof(UrmaEndpointDesc))
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t payloadLen = 0;
    std::memcpy(&magic, raw, sizeof(magic));
    std::memcpy(&version, raw + sizeof(magic), sizeof(version));
    std::memcpy(&payloadLen, raw + sizeof(magic) + sizeof(version), sizeof(payloadLen));
    EXPECT_EQ(magic, 0xA5FAC003U);
    EXPECT_EQ(version, 1U);
    EXPECT_EQ(payloadLen, sizeof(UrmaEndpointDesc));

    // Check payload matches expectedDesc
    UrmaEndpointDesc decodedDesc{};
    std::memcpy(&decodedDesc, raw + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t), sizeof(UrmaEndpointDesc));
    EXPECT_EQ(decodedDesc.devPhyId, expectedDesc.devPhyId);
    EXPECT_EQ(decodedDesc.superDevId, expectedDesc.superDevId);
    EXPECT_EQ(decodedDesc.serverIdx, expectedDesc.serverIdx);
    EXPECT_EQ(decodedDesc.superPodIdx, expectedDesc.superPodIdx);
    EXPECT_EQ(decodedDesc.protocol, expectedDesc.protocol);
    EXPECT_EQ(decodedDesc.type, expectedDesc.type);
    EXPECT_EQ(std::memcmp(decodedDesc.raws, expectedDesc.raws, sizeof(decodedDesc.raws)), 0);
}

TEST(DeviceUrmaTransportManagerTest, ReadRemoteNotOpenedFails)
{
    DeviceUrmaTransportManager manager;
    EXPECT_NE(manager.ReadRemote(0, MOCK_LOCAL_ADDR, MOCK_REMOTE_ADDR, MOCK_SIZE), BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, WriteRemoteNotOpenedFails)
{
    DeviceUrmaTransportManager manager;
    EXPECT_NE(manager.WriteRemote(0, MOCK_LOCAL_ADDR, MOCK_REMOTE_ADDR, MOCK_SIZE), BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, RegisterMemoryRegionNotOpenedFails)
{
    DeviceUrmaTransportManager manager;
    TransportMemoryRegion mr{};
    mr.addr = MOCK_LOCAL_ADDR;
    mr.size = MOCK_SIZE;
    mr.flags = REG_MR_FLAG_DRAM;
    EXPECT_NE(manager.RegisterMemoryRegion(mr), BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, UnregisterMemoryRegionUnknownAddrSucceeds)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;

    DeviceUrmaTransportManager manager;
    manager.opened_ = true;
    manager.localEndpoint_ = manager.manager_.CreateEndpoint(MakeEndpointDesc());
    ASSERT_NE(manager.localEndpoint_, nullptr);
    // UnregisterMemoryRegion for unknown addr should return BM_OK (no-op with warning)
    EXPECT_EQ(manager.UnregisterMemoryRegion(MOCK_LOCAL_ADDR), BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, CloseDeviceNotOpenedSucceeds)
{
    DeviceUrmaTransportManager manager;
    EXPECT_EQ(manager.CloseDevice(), BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, PrepareFailsOnBadPrivateDataMagic)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;

    DeviceUrmaTransportManager manager;
    manager.opened_ = true;
    manager.rankId_ = 0;
    manager.rankCount_ = 2;
    manager.localEndpoint_ = manager.manager_.CreateEndpoint(MakeEndpointDesc());
    ASSERT_NE(manager.localEndpoint_, nullptr);
    manager.localEndpointDesc_ = MakeEndpointDesc();

    // Private data with wrong magic
    TransportPrivateData badData{};
    badData.key.keys[0] = 0xDEADBEEFU;
    auto key = MakeImportKey(MOCK_REMOTE_ADDR, MOCK_SIZE, MOCK_MEM_TAG);
    HybmTransPrepareOptions options{};
    TransportRankPrepareInfo info{};
    info.nic = "tcp://127.0.0.1:8000";
    info.privateData = badData;
    info.role = HYBM_ROLE_PEER;
    info.memKeys = {key};
    options.options.emplace(1, std::move(info));

    EXPECT_NE(manager.Prepare(options), BM_OK);
}

TEST(DeviceUrmaTransportManagerTest, RegisterMemoryRegionInvalidFlagsFails)
{
    DlHcommApiFnGuard guard;
    DlHcommApi::gHcommEndpointCreate = MockHcommEndpointCreate;
    DlHcommApi::gHcommEndpointDestroy = MockHcommEndpointDestroy;

    DeviceUrmaTransportManager manager;
    manager.opened_ = true;
    manager.localEndpoint_ = manager.manager_.CreateEndpoint(MakeEndpointDesc());
    ASSERT_NE(manager.localEndpoint_, nullptr);

    // DRAM | HBM flags simultaneously — IsSupportedMemoryFlags returns false
    TransportMemoryRegion mr{};
    mr.addr = MOCK_LOCAL_ADDR;
    mr.size = MOCK_SIZE;
    mr.flags = REG_MR_FLAG_DRAM | REG_MR_FLAG_HBM;
    EXPECT_NE(manager.RegisterMemoryRegion(mr), BM_OK);
}
