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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cassert>
#include "dl_acl_api.h"
#include "dl_hcomm_api.h"
#include "hybm_batch_transfer.h"
#include "hybm_logger.h"
#include "hybm_stream_manager.h"
#include "device_urma_eid_reader.h"
#include "hybm_va_manager.h"
#include "device_urma_transport_manager.h"

namespace ock {
namespace mf {
namespace transport {
namespace device {

namespace {
constexpr uint32_t DEVICE_URMA_MAX_EXPORT_KEY_LENGTH = KEY_SIZE * 4;
constexpr uint32_t DEVICE_URMA_EXPORT_KEY_HEADER_SLOTS = 4;
constexpr uint32_t DEVICE_URMA_EXPORT_KEY_DATA_BYTES =
    (DEVICE_URMA_MAX_EXPORT_KEY_LENGTH - DEVICE_URMA_EXPORT_KEY_HEADER_SLOTS) * sizeof(uint64_t);
constexpr HcommMemHandle INVALID_MEM_HANDLE = nullptr;
constexpr uint32_t HCOMM_NORMAL_NOTIFY_NUM = 0;
constexpr const char *HYBM_DEVICE_FUNC_READ = "HybmBatchRead";
constexpr const char *HYBM_DEVICE_FUNC_WRITE = "HybmBatchWrite";
constexpr uint32_t HYBM_DEVICE_KERNEL_BLOCK_DIM = 1U;
constexpr uint16_t HYBM_DEVICE_KERNEL_TIMEOUT_MS = 60000U;
static_assert(std::is_trivially_copyable<UrmaExportDesc>::value, "UrmaExportDesc must be binary serializable");
static_assert(std::is_trivially_copyable<UrmaEndpointDesc>::value,
              "UrmaEndpointDesc must be trivially copyable for memcpy serialization");

// Must NOT conflict with URMA_EXPORT_DESC_MAGIC (0xA5FAB001).
constexpr uint32_t URMA_PRIVATE_DATA_MAGIC = 0xA5FAC003U;
constexpr uint16_t URMA_PRIVATE_DATA_VERSION = 1U;

struct UrmaPrivateDataDesc {
    uint32_t magic{URMA_PRIVATE_DATA_MAGIC};
    uint16_t version{URMA_PRIVATE_DATA_VERSION};
    uint16_t payloadLen{0};
};

static_assert(sizeof(UrmaPrivateDataDesc) + sizeof(UrmaEndpointDesc) <= sizeof(TransportPrivateData{}.key.keys),
              "UrmaEndpointDesc cannot fit into TransportPrivateData.key");

struct MemEntry {
    HcommMemHandle handle{INVALID_MEM_HANDLE};
    UrmaMemTag memTag{0};
    UrmaCommMem mem{};
    UrmaLocalMr mr{};
    uint32_t refCount{0};
    bool exportCacheValid{false};
    std::vector<uint8_t> exportCache{};
};

bool GetRangeEnd(const UrmaCommMem &mem, uint64_t &end)
{
    if (mem.addr == 0 || mem.size == 0) {
        return false;
    }
    if (std::numeric_limits<uint64_t>::max() - mem.addr < mem.size) {
        return false;
    }
    end = mem.addr + mem.size;
    return true;
}

bool IsValidMem(const UrmaCommMem &mem)
{
    uint64_t end = 0;
    return (mem.type == UrmaMemoryType::HOST_DRAM || mem.type == UrmaMemoryType::DEVICE_HBM) && GetRangeEnd(mem, end);
}

UrmaMemoryType ToUrmaMemoryType(uint32_t flags)
{
    if (flags & REG_MR_FLAG_HBM) {
        return UrmaMemoryType::DEVICE_HBM;
    }
    return UrmaMemoryType::HOST_DRAM;
}

UrmaCommMem ToUrmaMem(const TransportMemoryRegion &mr)
{
    return UrmaCommMem{mr.addr, mr.size, ToUrmaMemoryType(mr.flags)};
}

bool IsSupportedMemoryFlags(uint32_t flags)
{
    const bool hasDram = (flags & (REG_MR_FLAG_DRAM | REG_MR_FLAG_ACL_DRAM)) != 0;
    const bool hasHbm = (flags & REG_MR_FLAG_HBM) != 0;
    return !(hasDram && hasHbm);
}

CommProtocol ToHcommProtocol(UrmaProtocol protocol)
{
    // 目前支持的通信协议包括：RoCE、UBC_TP、UBC_CTP、UBoE。
    if (protocol == UrmaProtocol::ROCE) {
        return COMM_PROTOCOL_ROCE;
    }
    if (protocol == UrmaProtocol::UBC_TP) {
        return COMM_PROTOCOL_UBC_TP;
    }
    if (protocol == UrmaProtocol::UBC_CTP) {
        return COMM_PROTOCOL_UBC_CTP;
    }
    if (protocol == UrmaProtocol::UBOE) {
        return COMM_PROTOCOL_UBOE;
    }
    return COMM_PROTOCOL_RESERVED;
}

UrmaProtocol ToUrmaProtocol(CommProtocol protocol)
{
    if (protocol == COMM_PROTOCOL_ROCE) {
        return UrmaProtocol::ROCE;
    }
    if (protocol == COMM_PROTOCOL_UBC_TP) {
        return UrmaProtocol::UBC_TP;
    }
    if (protocol == COMM_PROTOCOL_UBC_CTP) {
        return UrmaProtocol::UBC_CTP;
    }
    if (protocol == COMM_PROTOCOL_UBOE) {
        return UrmaProtocol::UBOE;
    }
    return UrmaProtocol::RESERVED;
}

bool ToUrmaEndpointDesc(const EndpointDesc &hcommDesc, UrmaEndpointDesc &urmaDesc)
{
    if (hcommDesc.commAddr.type != COMM_ADDR_TYPE_EID && hcommDesc.commAddr.type != COMM_ADDR_TYPE_IP_V6 &&
        hcommDesc.commAddr.type != COMM_ADDR_TYPE_IP_V4) {
        BM_LOG_ERROR("device_urma topo endpoint must use EID/IP address, addr type: " << hcommDesc.commAddr.type);
        return false;
    }

    auto protocol = ToUrmaProtocol(hcommDesc.protocol);
    if (protocol == UrmaProtocol::RESERVED) {
        BM_LOG_ERROR("device_urma unsupported topo endpoint protocol: " << hcommDesc.protocol);
        return false;
    }

    UrmaEndpointDesc desc{};
    desc.protocol = protocol;
    desc.devPhyId = hcommDesc.loc.device.devPhyId;
    desc.superDevId = hcommDesc.loc.device.superDevId;
    desc.serverIdx = hcommDesc.loc.device.serverIdx;
    desc.superPodIdx = hcommDesc.loc.device.superPodIdx;
    desc.type = hcommDesc.commAddr.type;
    std::memcpy(desc.raws, hcommDesc.commAddr.raws, sizeof(desc.raws));
    urmaDesc = desc;
    return true;
}

EndpointDesc ToHcommEndpointDesc(const UrmaEndpointDesc &desc)
{
    EndpointDesc endpoint{};
    endpoint.protocol = ToHcommProtocol(desc.protocol);
    endpoint.commAddr.type = desc.type;
    std::memcpy(endpoint.commAddr.raws, desc.raws, sizeof(endpoint.commAddr.raws));

    endpoint.loc.locType = ENDPOINT_LOC_TYPE_DEVICE; // 暂时只支持DEVICE，后续RoCE再做HOST
    endpoint.loc.device.devPhyId = desc.devPhyId;
    endpoint.loc.device.superDevId = desc.superDevId;
    endpoint.loc.device.serverIdx = desc.serverIdx;
    endpoint.loc.device.superPodIdx = desc.superPodIdx;
    return endpoint;
}

HcommCommMem ToHcommMem(const UrmaCommMem &mem)
{
    HcommCommMem hcommMem{};
    hcommMem.type = COMM_MEM_TYPE_HOST;
    if (mem.type == UrmaMemoryType::DEVICE_HBM) {
        hcommMem.type = COMM_MEM_TYPE_DEVICE;
    }
    hcommMem.addr = reinterpret_cast<void *>(mem.addr);
    hcommMem.size = mem.size;
    return hcommMem;
}

std::string MakeMemTag(UrmaMemTag memTag)
{
    return std::to_string(memTag);
}

bool SameMem(const UrmaCommMem &left, const UrmaCommMem &right)
{
    return left.addr == right.addr && left.size == right.size && left.type == right.type;
}

bool ContainsAddressRange(uint64_t outerAddr, uint64_t outerSize, uint64_t innerAddr, uint64_t innerSize)
{
    uint64_t outerEnd = 0;
    uint64_t innerEnd = 0;
    const UrmaCommMem outer{outerAddr, outerSize, UrmaMemoryType::HOST_DRAM};
    const UrmaCommMem inner{innerAddr, innerSize, UrmaMemoryType::HOST_DRAM};
    return GetRangeEnd(outer, outerEnd) && GetRangeEnd(inner, innerEnd) && outerAddr <= innerAddr &&
           outerEnd >= innerEnd;
}

bool Overlaps(const UrmaCommMem &left, const UrmaCommMem &right)
{
    uint64_t leftEnd = 0;
    uint64_t rightEnd = 0;
    return left.type == right.type && GetRangeEnd(left, leftEnd) && GetRangeEnd(right, rightEnd) &&
           left.addr < rightEnd && right.addr < leftEnd;
}

bool DeserializeExportDesc(const uint8_t *memDesc, uint32_t descLen, UrmaExportDesc &desc, const uint8_t **hcommDesc,
                           uint32_t *hcommDescLen)
{
    if (memDesc == nullptr || hcommDesc == nullptr || hcommDescLen == nullptr || descLen < sizeof(UrmaExportDesc)) {
        return false;
    }
    std::memcpy(&desc, memDesc, sizeof(desc));
    const UrmaCommMem mem{desc.addr, desc.size, desc.memoryType};
    if (desc.magic != URMA_EXPORT_DESC_MAGIC || desc.version != URMA_EXPORT_DESC_VERSION ||
        desc.headerSize != sizeof(UrmaExportDesc) || !IsValidMem(mem)) {
        return false;
    }
    if (desc.hcommDescLen == 0 || descLen != sizeof(UrmaExportDesc) + desc.hcommDescLen) {
        return false;
    }
    *hcommDesc = memDesc + sizeof(UrmaExportDesc);
    *hcommDescLen = desc.hcommDescLen;
    return true;
}

// Returns BM_INVALID_PARAM on magic/version/payloadLen/capacity mismatch.
Result ParsePrivateDataToEndpointDesc(const TransportPrivateData &privateData, UrmaEndpointDesc &outDesc)
{
    constexpr size_t headerSize = sizeof(UrmaPrivateDataDesc);
    const uint8_t *raw = reinterpret_cast<const uint8_t *>(privateData.key.keys);

    UrmaPrivateDataDesc header{};
    std::memcpy(&header, raw, headerSize);
    if (header.magic != URMA_PRIVATE_DATA_MAGIC) {
        BM_LOG_ERROR("device_urma ParsePrivateDataToEndpointDesc invalid magic: 0x"
                     << std::hex << header.magic << " expected: 0x" << URMA_PRIVATE_DATA_MAGIC);
        return BM_INVALID_PARAM;
    }
    if (header.version != URMA_PRIVATE_DATA_VERSION) {
        BM_LOG_ERROR("device_urma ParsePrivateDataToEndpointDesc unsupported version: "
                     << header.version << " expected: " << URMA_PRIVATE_DATA_VERSION);
        return BM_INVALID_PARAM;
    }
    if (header.payloadLen != sizeof(UrmaEndpointDesc)) {
        BM_LOG_ERROR("device_urma ParsePrivateDataToEndpointDesc payloadLen mismatch: "
                     << header.payloadLen << " expected: " << sizeof(UrmaEndpointDesc));
        return BM_INVALID_PARAM;
    }
    if (headerSize + header.payloadLen > sizeof(privateData.key.keys)) {
        BM_LOG_ERROR("device_urma ParsePrivateDataToEndpointDesc payload exceeds key capacity, payloadLen: "
                     << header.payloadLen);
        return BM_INVALID_PARAM;
    }
    std::memcpy(&outDesc, raw + headerSize, sizeof(UrmaEndpointDesc));
    return BM_OK;
}

} // namespace

struct UrmaEndpointEntity {
    HcommEndpointHandle hcommEndpoint{nullptr};
    UrmaEndpointDesc desc{};
    mutable std::mutex mutex{};
    uint64_t memRef{0};
    std::unordered_map<UrmaMemTag, HcommMemHandle> tagIndex{};
    std::unordered_map<HcommMemHandle, std::shared_ptr<MemEntry>> memEntries{};
};

static Result HcomUrmaDestroyEndpoint(HcommEndpointHandle endpoint)
{
    if (endpoint == nullptr) {
        return BM_OK;
    }
    const auto ret = DlHcommApi::HcommEndpointDestroy(endpoint);
    if (ret != 0) {
        BM_LOG_ERROR("device_urma HcommEndpointDestroy failed, ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

UrmaEndpointHandle UrmaManagerTransport::CreateEndpoint(const UrmaEndpointDesc &desc) const
{
    auto endpoint = std::make_shared<UrmaEndpointEntity>();
    endpoint->desc = desc;
    auto hcommDesc = ToHcommEndpointDesc(desc);
    EndpointHandle handle = nullptr;
    const auto ret = DlHcommApi::HcommEndpointCreate(&hcommDesc, &handle);
    if (ret != 0) {
        BM_LOG_ERROR("device_urma HcommEndpointCreate failed, ret: " << ret);
        return nullptr;
    }
    endpoint->hcommEndpoint = handle;
    return endpoint;
}

Result UrmaManagerTransport::HcommMemReg(const UrmaEndpointHandle &endpoint, UrmaMemTag memTag, const UrmaCommMem &mem,
                                         HcommMemHandle *memHandle)
{
    if (endpoint == nullptr || memHandle == nullptr) {
        BM_LOG_ERROR("device_urma HcommMemReg: endpoint or memHandle is null");
        return BM_INVALID_PARAM;
    }
    if (!IsValidMem(mem)) {
        BM_LOG_ERROR("device_urma HcommMemReg: invalid memory");
        return BM_INVALID_PARAM;
    }

    std::lock_guard<std::mutex> guard(endpoint->mutex);
    auto tagIt = endpoint->tagIndex.find(memTag);
    if (tagIt != endpoint->tagIndex.end()) {
        auto entryIt = endpoint->memEntries.find(tagIt->second);
        if (entryIt == endpoint->memEntries.end()) {
            BM_LOG_ERROR("device_urma HcommMemReg: tag index points to non-existent memEntry");
            return BM_ERROR;
        }
        auto &entry = entryIt->second;
        if (!SameMem(entry->mem, mem)) {
            BM_LOG_ERROR("memTag conflict, memTag: " << memTag);
            return BM_ERROR;
        }
        entry->refCount++;
        endpoint->memRef++;
        *memHandle = entry->handle;
        return BM_OK;
    }

    for (const auto &item : endpoint->memEntries) {
        if (item.second != nullptr && Overlaps(item.second->mem, mem)) {
            BM_LOG_ERROR("URMA memory range overlaps an existing MR, addr: " << std::hex << mem.addr
                                                                             << " size: " << mem.size);
            return BM_ERROR;
        }
    }

    auto hcommMem = ToHcommMem(mem);
    HcommMemHandle hcommHandle = nullptr;
    const auto tag = MakeMemTag(memTag);

    BM_LOG_INFO("device_urma try to register mem, addr: " << VaToStr(hcommMem.addr) << " size: " << hcommMem.size
                                                          << " memType: " << hcommMem.type);
    int ret = DlHcommApi::HcommMemReg(endpoint->hcommEndpoint, tag.c_str(), &hcommMem, &hcommHandle);
    if (ret != 0) {
        BM_LOG_ERROR("device_urma HcommMemReg failed, addr: " << std::hex << mem.addr << " size: " << mem.size
                                                              << " ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    UrmaLocalMr localMr{};
    localMr.mem = mem;
    localMr.hcommMem = hcommHandle;

    try {
        auto entry = std::make_shared<MemEntry>();
        entry->handle = localMr.hcommMem;
        entry->memTag = memTag;
        entry->mem = mem;
        entry->mr = localMr;
        entry->refCount = 1;

        auto entryInserted = endpoint->memEntries.emplace(entry->handle, entry);
        auto tagInserted = endpoint->tagIndex.emplace(memTag, entry->handle);
        if (!entryInserted.second || !tagInserted.second) {
            endpoint->memEntries.erase(entry->handle);
            endpoint->tagIndex.erase(memTag);
            int deregRet = DlHcommApi::HcommMemUnreg(endpoint->hcommEndpoint, localMr.hcommMem);
            if (deregRet != 0) {
                BM_LOG_ERROR("device_urma HcommMemUnreg rollback failed, ret: " << deregRet);
            }
            return BM_ERROR;
        }

        endpoint->memRef++;
        *memHandle = entry->handle;
        return BM_OK;
    } catch (...) {
        int deregRet = DlHcommApi::HcommMemUnreg(endpoint->hcommEndpoint, localMr.hcommMem);
        if (deregRet != 0) {
            BM_LOG_ERROR("device_urma HcommMemUnreg rollback failed, ret: " << deregRet);
        }
        return BM_MALLOC_FAILED;
    }
}

Result UrmaManagerTransport::HcommMemUnreg(const UrmaEndpointHandle &endpoint, HcommMemHandle memHandle)
{
    if (endpoint == nullptr || memHandle == INVALID_MEM_HANDLE) {
        BM_LOG_ERROR("device_urma HcommMemUnreg: endpoint is null or memHandle is invalid");
        return BM_INVALID_PARAM;
    }

    std::lock_guard<std::mutex> guard(endpoint->mutex);
    auto entryIt = endpoint->memEntries.find(memHandle);
    if (entryIt == endpoint->memEntries.end() || entryIt->second == nullptr) {
        return BM_INVALID_PARAM;
    }

    auto entry = entryIt->second;
    if (entry->refCount > 1) {
        entry->refCount--;
        if (endpoint->memRef > 0) {
            endpoint->memRef--;
        }
        return BM_OK;
    }

    const auto ret = DlHcommApi::HcommMemUnreg(endpoint->hcommEndpoint, entry->mr.hcommMem);
    if (ret != 0) {
        BM_LOG_ERROR("device_urma HcommMemUnreg failed, ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    endpoint->tagIndex.erase(entry->memTag);
    endpoint->memEntries.erase(entryIt);
    if (endpoint->memRef > 0) {
        endpoint->memRef--;
    }
    return BM_OK;
}

Result UrmaManagerTransport::HcommMemExport(const UrmaEndpointHandle &endpoint, HcommMemHandle memHandle,
                                            const uint8_t **memDesc, uint32_t *memDescLen)
{
    if (endpoint == nullptr || memDesc == nullptr || memDescLen == nullptr) {
        BM_LOG_ERROR("device_urma HcommMemExport: endpoint, memDesc, or memDescLen is null");
        return BM_INVALID_PARAM;
    }
    std::lock_guard<std::mutex> guard(endpoint->mutex);
    auto entryIt = endpoint->memEntries.find(memHandle);
    if (entryIt == endpoint->memEntries.end() || entryIt->second == nullptr) {
        return BM_INVALID_PARAM;
    }

    auto entry = entryIt->second;
    if (!entry->exportCacheValid) {
        void *hcommDesc = nullptr;
        uint32_t hcommDescLen = 0;
        BM_LOG_INFO("device_urma try to export memory addr: " << VaToStr(entry->mem.addr)
                                                              << " size: " << entry->mem.size);
        auto ret = DlHcommApi::HcommMemExport(endpoint->hcommEndpoint, entry->mr.hcommMem, &hcommDesc, &hcommDescLen);
        if (ret != BM_OK) {
            return ret;
        }
        if (hcommDesc == nullptr || hcommDescLen == 0) {
            BM_LOG_ERROR("device_urma HcommMemExport: hcommDesc is null or hcommDescLen is 0 after HcommMemExport");
            return BM_ERROR;
        }

        try {
            UrmaExportDesc desc{};
            desc.magic = URMA_EXPORT_DESC_MAGIC;
            desc.version = URMA_EXPORT_DESC_VERSION;
            desc.headerSize = sizeof(UrmaExportDesc);
            desc.memoryType = entry->mem.type;
            desc.memTag = entry->memTag;
            desc.addr = entry->mem.addr;
            desc.size = entry->mem.size;
            desc.hcommDescLen = hcommDescLen;

            std::vector<uint8_t> bytes(sizeof(desc) + hcommDescLen);
            std::memcpy(bytes.data(), &desc, sizeof(desc));
            std::memcpy(bytes.data() + sizeof(desc), hcommDesc, hcommDescLen);
            entry->exportCache.swap(bytes);
            entry->exportCacheValid = true;
        } catch (...) {
            return BM_MALLOC_FAILED;
        }
    }

    *memDesc = entry->exportCache.data();
    *memDescLen = static_cast<uint32_t>(entry->exportCache.size());
    return BM_OK;
}

Result UrmaManagerTransport::HcommMemImport(const UrmaEndpointHandle &endpoint, const uint8_t *memDesc,
                                            uint32_t descLen, UrmaCommMem *commMem)
{
    if (endpoint == nullptr || memDesc == nullptr || commMem == nullptr) {
        BM_LOG_ERROR("device_urma HcommMemImport: endpoint, memDesc, or commMem is null");
        return BM_INVALID_PARAM;
    }
    UrmaExportDesc desc{};
    const uint8_t *hcommDesc = nullptr;
    uint32_t hcommDescLen = 0;
    if (!DeserializeExportDesc(memDesc, descLen, desc, &hcommDesc, &hcommDescLen)) {
        return BM_INVALID_PARAM;
    }

    HcommCommMem outMem{};
    {
        std::lock_guard<std::mutex> guard(endpoint->mutex);
        const auto ret = DlHcommApi::HcommMemImport(endpoint->hcommEndpoint, hcommDesc, hcommDescLen, &outMem);
        if (ret != 0) {
            BM_LOG_ERROR("device_urma HcommMemImport failed, ret: " << ret);
            return BM_DL_FUNCTION_FAILED;
        }
        if (outMem.type == COMM_MEM_TYPE_INVALID) {
            BM_LOG_ERROR("device_urma HcommMemImport invalid outMem type (COMM_MEM_TYPE_INVALID)");
            return BM_INVALID_PARAM;
        }
    }

    BM_LOG_INFO("device_urma import memory returned outMem (addr=" << VaToStr(outMem.addr) << " size=" << outMem.size
                                                                   << " type=" << outMem.type << ")");
    UrmaCommMem view{reinterpret_cast<uint64_t>(outMem.addr), outMem.size,
                     outMem.type == COMM_MEM_TYPE_DEVICE ? UrmaMemoryType::DEVICE_HBM : UrmaMemoryType::HOST_DRAM};
    if (!IsValidMem(view)) {
        BM_LOG_ERROR("device_urma HcommMemImport returned invalid view (addr="
                     << std::hex << view.addr << " size=" << view.size << " type=" << view.type << std::dec << ")");
        return BM_DL_FUNCTION_FAILED;
    }
    *commMem = view;
    return BM_OK;
}

Result UrmaManagerTransport::HcommMemUnimport(const UrmaEndpointHandle &endpoint, const uint8_t *memDesc,
                                              uint32_t descLen)
{
    if (endpoint == nullptr || memDesc == nullptr) {
        BM_LOG_ERROR("device_urma HcommMemUnimport: endpoint or memDesc is null");
        return BM_INVALID_PARAM;
    }
    UrmaExportDesc desc{};
    const uint8_t *hcommDesc = nullptr;
    uint32_t hcommDescLen = 0;
    if (!DeserializeExportDesc(memDesc, descLen, desc, &hcommDesc, &hcommDescLen)) {
        return BM_INVALID_PARAM;
    }

    std::lock_guard<std::mutex> guard(endpoint->mutex);
    const auto ret = DlHcommApi::HcommMemUnimport(endpoint->hcommEndpoint, hcommDesc, hcommDescLen);
    if (ret != 0) {
        BM_LOG_ERROR("device_urma HcommMemUnimport failed, ret: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    return BM_OK;
}

DeviceUrmaTransportManager::~DeviceUrmaTransportManager()
{
    (void)CloseDevice();
}

Result DeviceUrmaTransportManager::EnsureDeviceKernelLoadedLocked()
{
    if (deviceKernelLoaded_) {
        return BM_OK;
    }

    auto ret = LoadDeviceKernelAndGetHandles(HYBM_DEVICE_FUNC_READ, HYBM_DEVICE_FUNC_WRITE, deviceKernelHandle_,
                                             deviceFuncHandles_);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma LoadDeviceKernelAndGetHandles failed, ret: " << ret);
        return ret;
    }
    if (deviceFuncHandles_.batchRead == nullptr || deviceFuncHandles_.batchWrite == nullptr) {
        BM_LOG_ERROR("device_urma invalid device kernel function handles, read: "
                     << deviceFuncHandles_.batchRead << " write: " << deviceFuncHandles_.batchWrite);
        return BM_DL_FUNCTION_FAILED;
    }
    deviceKernelLoaded_ = true;
    return BM_OK;
}

aclrtFuncHandle DeviceUrmaTransportManager::GetDeviceKernelFunc(bool isRead) const
{
    return isRead ? deviceFuncHandles_.batchRead : deviceFuncHandles_.batchWrite;
}

void DeviceUrmaTransportManager::ReleaseDeviceTransferBuffers(DeviceTransferBuffers &buffers)
{
    if (buffers.dstList != nullptr) {
        (void)DlAclApi::AclrtFree(buffers.dstList);
        buffers.dstList = nullptr;
    }
    if (buffers.srcList != nullptr) {
        (void)DlAclApi::AclrtFree(buffers.srcList);
        buffers.srcList = nullptr;
    }
    if (buffers.lenList != nullptr) {
        (void)DlAclApi::AclrtFree(buffers.lenList);
        buffers.lenList = nullptr;
    }
}

Result DeviceUrmaTransportManager::AddBatchPendingDeviceBuffers(DeviceTransferBuffers &buffers)
{
    std::lock_guard<std::mutex> guard(pendingBatchMutex_);
    try {
        pendingBatchBuffers_.push_back(buffers);
    } catch (...) {
        BM_LOG_ERROR("device_urma save batch pending device buffers failed");
        return BM_ERROR;
    }
    buffers = DeviceTransferBuffers{};
    return BM_OK;
}

void DeviceUrmaTransportManager::FreeBatchPendingDeviceBuffers()
{
    std::lock_guard<std::mutex> guard(pendingBatchMutex_);
    for (auto &buffers : pendingBatchBuffers_) {
        ReleaseDeviceTransferBuffers(buffers);
    }
    pendingBatchBuffers_.clear();
}

Result DeviceUrmaTransportManager::SynchronizeDeviceKernelStream()
{
    {
        std::lock_guard<std::mutex> guard(pendingBatchMutex_);
        if (pendingBatchBuffers_.empty()) {
            return BM_OK;
        }
    }

    void *stream = HybmStreamManager::GetThreadAclStream();
    if (stream == nullptr) {
        BM_LOG_ERROR("device_urma GetThreadAclStream failed while synchronizing device kernel");
        FreeBatchPendingDeviceBuffers();
        return BM_DL_FUNCTION_FAILED;
    }

    const auto ret = DlAclApi::AclrtSynchronizeStream(stream);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma AclrtSynchronizeStream failed, ret: " << ret);
        FreeBatchPendingDeviceBuffers();
        return ret;
    }
    FreeBatchPendingDeviceBuffers();
    return BM_OK;
}

Result DeviceUrmaTransportManager::LaunchDeviceKernelBatch(HcommThreadHandle thread, bool isRead,
                                                           HcommChannelHandle channel,
                                                           const std::vector<uint64_t> &localAddrs,
                                                           const std::vector<uint64_t> &remoteAddrs,
                                                           const std::vector<uint64_t> &sizes)
{
    const auto batchSize = localAddrs.size();
    if (batchSize == 0 || batchSize != remoteAddrs.size() || batchSize != sizes.size()) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch: invalid batchSize=" << batchSize);
        return BM_INVALID_PARAM;
    }
    if (channel == 0) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch: channel is 0, not connected");
        return BM_NOT_CONNECTED;
    }
    if (thread == 0) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch: thread is 0, not connected");
        return BM_NOT_CONNECTED;
    }

    std::lock_guard<std::mutex> guard(deviceKernelMutex_);
    auto ret = EnsureDeviceKernelLoadedLocked();
    if (ret != BM_OK) {
        return ret;
    }

    DeviceTransferBuffers buffers{};
    // Batch buffers are variable-size; allocate fresh (not from free-list).
    ret = DlAclApi::AclrtMalloc(&buffers.dstList, batchSize * sizeof(void *), 0);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch alloc dst list failed, ret: " << ret);
        return ret;
    }
    ret = DlAclApi::AclrtMalloc(&buffers.srcList, batchSize * sizeof(void *), 0);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch alloc src list failed, ret: " << ret);
        (void)DlAclApi::AclrtFree(buffers.dstList);
        buffers.dstList = nullptr;
        return ret;
    }
    ret = DlAclApi::AclrtMalloc(&buffers.lenList, batchSize * sizeof(uint64_t), 0);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch alloc len list failed, ret: " << ret);
        (void)DlAclApi::AclrtFree(buffers.dstList);
        (void)DlAclApi::AclrtFree(buffers.srcList);
        buffers.dstList = nullptr;
        buffers.srcList = nullptr;
        return ret;
    }
    // Build host-side arrays then H2D copy
    std::vector<void *> dstHost(batchSize);
    std::vector<void *> srcHost(batchSize);
    std::vector<uint64_t> lenHost(batchSize);
    for (size_t i = 0; i < batchSize; ++i) {
        dstHost[i] = reinterpret_cast<void *>(isRead ? localAddrs[i] : remoteAddrs[i]);
        srcHost[i] = reinterpret_cast<void *>(isRead ? remoteAddrs[i] : localAddrs[i]);
        lenHost[i] = sizes[i];
    }

    ret = DlAclApi::AclrtMemcpy(buffers.dstList, batchSize * sizeof(void *), dstHost.data(), batchSize * sizeof(void *),
                                ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch copy dst list failed, ret: " << ret);
        ReleaseDeviceTransferBuffers(buffers);
        return ret;
    }
    ret = DlAclApi::AclrtMemcpy(buffers.srcList, batchSize * sizeof(void *), srcHost.data(), batchSize * sizeof(void *),
                                ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch copy src list failed, ret: " << ret);
        ReleaseDeviceTransferBuffers(buffers);
        return ret;
    }
    ret = DlAclApi::AclrtMemcpy(buffers.lenList, batchSize * sizeof(uint64_t), lenHost.data(),
                                batchSize * sizeof(uint64_t), ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch copy len list failed, ret: " << ret);
        ReleaseDeviceTransferBuffers(buffers);
        return ret;
    }
    HybmOneSideOpParam args{};
    args.thread = thread;
    args.channel = channel;
    args.list_num = static_cast<uint32_t>(batchSize);
    args.dst_buf_addr_list = static_cast<void **>(buffers.dstList);
    args.src_buf_addr_list = static_cast<void **>(buffers.srcList);
    args.len_list = static_cast<uint64_t *>(buffers.lenList);
    args.remote_flag_addr = 0;
    args.local_flag_addr = 0;
    args.flag_size = 0;

    aclrtArgsHandle argsHandle = nullptr;
    auto funcHandle = GetDeviceKernelFunc(isRead);
    ret = DlAclApi::AclrtKernelArgsInit(funcHandle, &argsHandle);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch AclrtKernelArgsInit failed, ret: " << ret);
        ReleaseDeviceTransferBuffers(buffers);
        return ret;
    }
    aclrtParamHandle paramHandle = nullptr;
    ret = DlAclApi::AclrtKernelArgsAppend(argsHandle, &args, sizeof(args), &paramHandle);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch AclrtKernelArgsAppend failed, ret: " << ret);
        ReleaseDeviceTransferBuffers(buffers);
        return ret;
    }
    ret = DlAclApi::AclrtKernelArgsFinalize(argsHandle);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch AclrtKernelArgsFinalize failed, ret: " << ret);
        ReleaseDeviceTransferBuffers(buffers);
        return ret;
    }
    void *stream = HybmStreamManager::GetThreadAclStream();
    if (stream == nullptr) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch GetThreadAclStream failed");
        ReleaseDeviceTransferBuffers(buffers);
        return BM_DL_FUNCTION_FAILED;
    }

    aclrtLaunchKernelAttr attr{};
    attr.id = aclrtLaunchKernelAttrId::ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
    attr.value.timeout = HYBM_DEVICE_KERNEL_TIMEOUT_MS;
    aclrtLaunchKernelCfg cfg{};
    cfg.attrs = &attr;
    cfg.numAttrs = 1;

    ret = DlAclApi::AclrtLaunchKernelWithConfig(funcHandle, HYBM_DEVICE_KERNEL_BLOCK_DIM, stream, &cfg, argsHandle,
                                                nullptr);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma LaunchDeviceKernelBatch AclrtLaunchKernelWithConfig failed, kernel: "
                     << (isRead ? HYBM_DEVICE_FUNC_READ : HYBM_DEVICE_FUNC_WRITE) << " ret: " << ret);
        ReleaseDeviceTransferBuffers(buffers);
        return ret;
    }

    // Batch pending buffers are tracked separately and freed (not recycled) after sync.
    ret = AddBatchPendingDeviceBuffers(buffers);
    if (ret != BM_OK) {
        ReleaseDeviceTransferBuffers(buffers);
        return ret;
    }
    return BM_OK;
}

Result DeviceUrmaTransportManager::EnsureOpenLocked() const
{
    if (!opened_) {
        BM_LOG_ERROR("device_urma transport is not open");
        return BM_ERROR;
    }
    return BM_OK;
}

Result DeviceUrmaTransportManager::OpenDevice(const TransportOptions &options)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (opened_) {
        return BM_OK;
    }
    if (options.rankCount == 0 || options.rankId >= options.rankCount) {
        BM_LOG_ERROR("device_urma OpenDevice: invalid rankCount or rankId");
        return BM_INVALID_PARAM;
    }
    if (DlAclApi::GetAscendSocType() != AscendSocType::ASCEND_950) {
        BM_LOG_ERROR("device_urma is only supported on Ascend950 soc, rank: " << options.rankId);
        return BM_NOT_SUPPORTED;
    }

    int32_t userId = -1;
    int32_t logicId = -1;
    auto ret = DlAclApi::AclrtGetDevice(&userId);
    BM_ASSERT_LOG_AND_RETURN(ret == 0 && userId >= 0,
                             "AclrtGetDevice() return=" << ret << ", output deviceId=" << userId,
                             BM_DL_FUNCTION_FAILED);
    ret = DlAclApi::RtGetLogicDevIdByUserDevId(userId, &logicId);
    BM_ASSERT_LOG_AND_RETURN(ret == 0 && logicId >= 0,
                             "RtGetLogicDevIdByUserDevId() return=" << ret << ", output deviceId=" << logicId,
                             BM_DL_FUNCTION_FAILED);

    options_ = options;
    rankId_ = options.rankId;
    rankCount_ = options.rankCount;
    userDeviceId_ = static_cast<uint32_t>(userId);
    logicDeviceId_ = static_cast<uint32_t>(logicId);
    int32_t phyId = 0;
    ret = DlAclApi::AclrtGetPhyDevIdByLogicDevId(logicId, &phyId);
    BM_ASSERT_LOG_AND_RETURN(ret == 0,
                             "aclrtGetPhyDevIdByLogicDevId() return=" << ret << ", logicDeviceId=" << logicDeviceId_
                                                                      << ", userDeviceId=" << userId,
                             BM_DL_FUNCTION_FAILED);
    phyDeviceId_ = static_cast<uint32_t>(phyId);

    // Get device location info
    int64_t infoValue = 0;
    ret = DlAclApi::RtGetDeviceInfo(static_cast<uint32_t>(userId), 0, INFO_TYPE_SDID, &infoValue);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "RtGetDeviceInfo(INFO_TYPE_SDID) return=" << ret, BM_DL_FUNCTION_FAILED);
    sdid_ = static_cast<uint32_t>(infoValue);

    infoValue = 0;
    ret = DlAclApi::RtGetDeviceInfo(static_cast<uint32_t>(userId), 0, INFO_TYPE_SERVER_ID, &infoValue);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "RtGetDeviceInfo(INFO_TYPE_SERVER_ID) return=" << ret, BM_DL_FUNCTION_FAILED);
    serverId_ = static_cast<uint32_t>(infoValue);

    infoValue = 0;
    ret = DlAclApi::RtGetDeviceInfo(static_cast<uint32_t>(userId), 0, INFO_TYPE_SUPER_POD_ID, &infoValue);
    BM_ASSERT_LOG_AND_RETURN(ret == 0, "RtGetDeviceInfo(INFO_TYPE_SUPER_POD_ID) return=" << ret, BM_DL_FUNCTION_FAILED);
    superPodId_ = static_cast<uint32_t>(infoValue);
    BM_LOG_INFO("local device info: userId=" << userId << ", logicId=" << logicId << ", phyId=" << phyId
                                             << " sdid=" << sdid_ << ", server_id=" << serverId_
                                             << ", superpod id=" << superPodId_);

    // Read EID via helper
    std::array<uint8_t, COMM_ADDR_EID_LEN> eidData{};
    const auto retEid = GetDeviceUrmaEid(phyDeviceId_, rankId_, eidData);
    if (retEid != 0) {
        return retEid;
    }

    // Build UrmaEndpointDesc and create local endpoint
    UrmaEndpointDesc localDesc{};
    localDesc.protocol = UrmaProtocol::UBC_CTP;
    localDesc.type = COMM_ADDR_TYPE_IP_V6;
    std::memcpy(localDesc.raws, eidData.data(), COMM_ADDR_EID_LEN);
    localDesc.devPhyId = phyDeviceId_;
    localDesc.superDevId = sdid_;
    localDesc.serverIdx = serverId_;
    localDesc.superPodIdx = superPodId_;
    auto endpoint = manager_.CreateEndpoint(localDesc);
    if (endpoint == nullptr) {
        BM_LOG_ERROR("device_urma CreateEndpoint failed, rankId=" << rankId_);
        return BM_MALLOC_FAILED;
    }
    localEndpoint_ = endpoint;
    localEndpointDesc_ = localDesc;

    opened_ = true;
    BM_LOG_INFO("device_urma OpenDevice success, rank: " << rankId_ << " rankCount: " << rankCount_
                                                         << " devPhyId: " << phyDeviceId_);
    return BM_OK;
}

Result DeviceUrmaTransportManager::CloseDevice()
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (!opened_) {
        return BM_OK;
    }
    Result finalRet = BM_OK;
    for (auto &rankItem : remoteRanks_) {
        auto &state = rankItem.second;
        if (state.pendingOps != 0 && !state.channels.empty()) {
            const auto fenceRet = SynchronizeDeviceKernelStream();
            if (fenceRet != BM_OK && finalRet == BM_OK) {
                finalRet = fenceRet;
            }
            state.pendingOps = 0;
        }
        for (const auto channel : state.channels) {
            if (channel != 0) {
                auto hcommChan = channel;
                const auto ret = DlHcommApi::HcommChannelDestroy(&hcommChan, 1);
                if (ret != 0 && finalRet == BM_OK) {
                    BM_LOG_ERROR("device_urma HcommChannelDestroy failed, channel: " << channel << " ret: " << ret);
                    finalRet = BM_DL_FUNCTION_FAILED;
                }
            }
        }
        state.channels.clear();
        if (state.thread != 0) {
            const auto syncRet = SynchronizeDeviceKernelStream();
            if (syncRet != BM_OK && finalRet == BM_OK) {
                finalRet = syncRet;
            }
            auto hcommThread = state.thread;
            const auto ret = DlHcommApi::HcommThreadFree(&hcommThread, 1);
            if (ret != BM_OK && finalRet == BM_OK) {
                BM_LOG_ERROR("device_urma HcommThreadFree failed, thread: " << state.thread << " ret: " << ret);
                finalRet = ret;
            }
            state.thread = 0;
        }
    }
    // Unimport remote memory from the global localEndpoint_.
    for (auto &rankItem : remoteRanks_) {
        auto &state = rankItem.second;
        for (auto &remote : state.imports) {
            if (!remote.descBytes.empty()) {
                const auto ret = manager_.HcommMemUnimport(localEndpoint_, remote.descBytes.data(),
                                                           static_cast<uint32_t>(remote.descBytes.size()));
                if (ret != BM_OK && finalRet == BM_OK) {
                    finalRet = ret;
                }
            }
        }
        state.imports.clear();
    }

    for (auto &item : localRegistrations_) {
        if (item.second.handle != INVALID_MEM_HANDLE) {
            const auto ret = manager_.HcommMemUnreg(localEndpoint_, item.second.handle);
            if (ret != BM_OK && finalRet == BM_OK) {
                BM_LOG_ERROR("device_urma HcommMemUnreg global handle failed, addr: " << std::hex << item.first
                                                                                      << " ret: " << ret);
                finalRet = ret;
            }
            item.second.handle = INVALID_MEM_HANDLE;
        }
    }

    for (auto &item : localRegistrations_) {
        for (const auto &peerHandle : item.second.peerHandles) {
            auto stateIt = remoteRanks_.find(peerHandle.first);
            if (stateIt == remoteRanks_.end() || stateIt->second.localEndpoint == nullptr ||
                peerHandle.second == INVALID_MEM_HANDLE) {
                continue;
            }
            const auto ret = manager_.HcommMemUnreg(stateIt->second.localEndpoint, peerHandle.second);
            if (ret != BM_OK && finalRet == BM_OK) {
                finalRet = ret;
            }
        }
    }
    localRegistrations_.clear();
    for (auto &rankItem : remoteRanks_) {
        if (rankItem.second.localEndpoint != nullptr) {
            const auto ret = HcomUrmaDestroyEndpoint(rankItem.second.localEndpoint->hcommEndpoint);
            if (ret != BM_OK && finalRet == BM_OK) {
                finalRet = ret;
            }
            rankItem.second.localEndpoint.reset();
        }
    }
    remoteRanks_.clear();
    // Release class-level local endpoint created during OpenDevice
    if (localEndpoint_ != nullptr) {
        const auto ret = HcomUrmaDestroyEndpoint(localEndpoint_->hcommEndpoint);
        if (ret != BM_OK && finalRet == BM_OK) {
            BM_LOG_ERROR("device_urma HcomUrmaDestroyEndpoint failed for localEndpoint_, ret: " << ret);
            finalRet = ret;
        }
        localEndpoint_.reset();
        localEndpointDesc_ = UrmaEndpointDesc{};
    }

    FreeBatchPendingDeviceBuffers();
    opened_ = false;
    connected_ = false;
    return finalRet;
}

Result DeviceUrmaTransportManager::FindLocalRegistrationLocked(uint64_t addr, uint64_t size,
                                                               LocalRegistration *registration) const
{
    if (addr == 0) {
        BM_LOG_ERROR("device_urma FindLocalRegistrationLocked: addr is 0");
        return BM_INVALID_PARAM;
    }
    if (size == 0) {
        return BM_OK;
    }
    // Validate that (addr, size) forms a valid non-wrapping address range.
    if (!ContainsAddressRange(addr, size, addr, size)) {
        BM_LOG_ERROR("device_urma FindLocalRegistrationLocked: address range check failed, addr=0x"
                     << std::hex << addr << " size=0x" << size << std::dec);
        return BM_INVALID_PARAM;
    }
    for (const auto &item : localRegistrations_) {
        if (ContainsAddressRange(item.second.mr.addr, item.second.mr.size, addr, size)) {
            if (registration != nullptr) {
                *registration = item.second;
            }
            return BM_OK;
        }
    }
    return BM_INVALID_PARAM;
}

Result DeviceUrmaTransportManager::FindRemoteRegistrationLocked(uint32_t rankId, uint64_t addr, uint64_t size,
                                                                RemoteRegistration *registration) const
{
    if (rankId >= rankCount_) {
        return BM_INVALID_PARAM;
    }
    if (size == 0) {
        return BM_OK;
    }
    const auto rankIt = remoteRanks_.find(rankId);
    if (rankIt == remoteRanks_.end()) {
        return BM_NOT_CONNECTED;
    }
    // Validate that (addr, size) forms a valid non-wrapping address range.
    if (!ContainsAddressRange(addr, size, addr, size)) {
        BM_LOG_ERROR("device_urma FindRemoteRegistrationLocked: address range check failed, addr=0x"
                     << std::hex << addr << " size=0x" << size << std::dec);
        return BM_INVALID_PARAM;
    }
    for (const auto &remote : rankIt->second.imports) {
        if (ContainsAddressRange(remote.addr, remote.size, addr, size)) {
            if (registration != nullptr) {
                *registration = remote;
            }
            return BM_OK;
        }
    }
    return BM_INVALID_PARAM;
}

Result DeviceUrmaTransportManager::RegisterMemoryRegion(const TransportMemoryRegion &mr)
{
    std::lock_guard<std::mutex> guard(mutex_);
    auto ret = EnsureOpenLocked();
    if (ret != BM_OK) {
        return ret;
    }
    if (mr.addr == 0 || mr.size == 0) {
        BM_LOG_ERROR("device_urma RegisterMemoryRegion: mr.addr or mr.size is 0");
        return BM_INVALID_PARAM;
    }
    if (!IsSupportedMemoryFlags(mr.flags)) {
        BM_LOG_ERROR("device_urma RegisterMemoryRegion: unsupported memory flags: " << mr.flags);
        return BM_INVALID_PARAM;
    }
    const UrmaCommMem mem = ToUrmaMem(mr);
    if (!IsValidMem(mem)) {
        BM_LOG_ERROR("device_urma RegisterMemoryRegion: invalid memory");
        return BM_INVALID_PARAM;
    }

    // localEndpoint_ must be created by OpenDevice before any memory registration
    if (localEndpoint_ == nullptr) {
        BM_LOG_ERROR("device_urma localEndpoint_ is null, cannot register memory");
        return BM_NOT_INITIALIZED;
    }

    auto item = localRegistrations_.find(mr.addr);
    if (item != localRegistrations_.end()) {
        if (item->second.mr.size != mr.size) {
            BM_LOG_ERROR("device_urma register memory conflict, addr: " << std::hex << mr.addr);
            return BM_ERROR;
        }
        item->second.refCount++;
        return BM_OK;
    }

    LocalRegistration registration{};
    registration.mr = mr;
    registration.memTag = mr.addr;
    registration.refCount = 1;

    HcommMemHandle hcommHandle = nullptr;
    ret = manager_.HcommMemReg(localEndpoint_, registration.memTag, mem, &hcommHandle);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma HcommMemReg failed, addr: " << VaToStr(mr.addr) << " size: " << mr.size
                                                              << " ret: " << ret);
        return ret;
    }
    registration.handle = hcommHandle;

    try {
        localRegistrations_.emplace(mr.addr, registration);
    } catch (...) {
        (void)manager_.HcommMemUnreg(localEndpoint_, hcommHandle);
        return BM_MALLOC_FAILED;
    }
    return BM_OK;
}

Result DeviceUrmaTransportManager::UnregisterMemoryRegion(uint64_t addr)
{
    std::lock_guard<std::mutex> guard(mutex_);
    auto ret = EnsureOpenLocked();
    if (ret != BM_OK) {
        return ret;
    }
    auto item = localRegistrations_.find(addr);
    if (item == localRegistrations_.end()) {
        BM_LOG_WARN("device_urma unregister skipped for unknown addr: " << std::hex << addr);
        return BM_OK;
    }
    if (item->second.refCount > 1) {
        item->second.refCount--;
        return BM_OK;
    }

    Result finalRet = BM_OK;
    if (item->second.handle != INVALID_MEM_HANDLE) {
        auto retUnreg = manager_.HcommMemUnreg(localEndpoint_, item->second.handle);
        if (retUnreg != BM_OK) {
            BM_LOG_ERROR("device_urma HcommMemUnreg failed for global handle, addr: " << std::hex << addr
                                                                                      << " ret: " << retUnreg);
            if (finalRet == BM_OK) {
                finalRet = retUnreg;
            }
        }
        item->second.handle = INVALID_MEM_HANDLE;
    }

    for (const auto &peerHandle : item->second.peerHandles) {
        auto stateIt = remoteRanks_.find(peerHandle.first);
        if (stateIt == remoteRanks_.end() || stateIt->second.localEndpoint == nullptr ||
            peerHandle.second == INVALID_MEM_HANDLE) {
            continue;
        }
        auto retPeer = manager_.HcommMemUnreg(stateIt->second.localEndpoint, peerHandle.second);
        if (retPeer != BM_OK) {
            BM_LOG_ERROR("device_urma HcommMemUnreg failed for peer handle, rank: " << peerHandle.first
                                                                                    << " ret: " << retPeer);
            if (finalRet == BM_OK) {
                finalRet = retPeer;
            }
        }
    }
    localRegistrations_.erase(item);
    return finalRet;
}

bool DeviceUrmaTransportManager::QueryHasRegistered(uint64_t addr, uint64_t size)
{
    std::lock_guard<std::mutex> guard(mutex_);
    return FindLocalRegistrationLocked(addr, size, nullptr) == BM_OK;
}

Result DeviceUrmaTransportManager::QueryMemoryKey(uint64_t addr, TransportMemoryKey &key)
{
    std::lock_guard<std::mutex> guard(mutex_);
    auto ret = EnsureOpenLocked();
    if (ret != BM_OK) {
        return ret;
    }
    LocalRegistration registration{};
    ret = FindLocalRegistrationLocked(addr, 1, &registration);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma QueryMemoryKey addr is not registered: " << std::hex << addr);
        return ret;
    }

    if (registration.handle == INVALID_MEM_HANDLE) {
        BM_LOG_ERROR("device_urma QueryMemoryKey addr has no HCOMM handle: " << std::hex << addr);
        return BM_ERROR;
    }

    // Export full memDesc via the global localEndpoint_.
    // manager_.HcommMemExport wraps HcommMemExport + UrmaExportDesc header + cache.
    const uint8_t *memDesc = nullptr;
    uint32_t memDescLen = 0;
    ret = manager_.HcommMemExport(localEndpoint_, registration.handle, &memDesc, &memDescLen);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma HcommMemExport failed for addr: " << std::hex << addr << " ret: " << ret);
        return ret;
    }

    // Pack memDesc into TransportMemoryKey.keys[4..56] (416 bytes available after 4-header slots)
    if (memDescLen > DEVICE_URMA_EXPORT_KEY_DATA_BYTES) {
        BM_LOG_ERROR("device_urma HcommMemExport desc too large: " << memDescLen << " bytes, max supported: "
                                                                   << DEVICE_URMA_EXPORT_KEY_DATA_BYTES);
        return BM_NOT_SUPPORTED;
    }

    std::memset(&key, 0, sizeof(key));
    key.keys[0] = URMA_EXPORT_DESC_MAGIC;
    uint64_t gva = HybmVaManager::GetInstance().TransformVa(registration.mr.addr, HVM_HVA, HVM_GVA);
    key.keys[1] = (gva != 0) ? gva : registration.mr.addr;
    key.keys[2] = registration.mr.size;
    key.keys[3] = registration.memTag;
    std::memcpy(reinterpret_cast<uint8_t *>(&key.keys[DEVICE_URMA_EXPORT_KEY_HEADER_SLOTS]), memDesc, memDescLen);
    return BM_OK;
}

void DeviceUrmaTransportManager::UpdateMemoryKey(TransportMemoryKey &key, void *addr)
{
    if (addr != nullptr) {
        key.keys[1] = reinterpret_cast<uint64_t>(addr);
    }
}

Result DeviceUrmaTransportManager::ImportRemoteMemKeysLocked(uint32_t peerRank, RemoteRankState &state,
                                                             const std::vector<TransportMemoryKey> &memKeys)
{
    if (memKeys.empty()) {
        return BM_OK;
    }
    if (localEndpoint_ == nullptr) {
        BM_LOG_ERROR("device_urma ImportRemoteMemKeysLocked localEndpoint_ is null, peer: " << peerRank);
        return BM_NOT_INITIALIZED;
    }

    // Collect newly imported registrations in a local vector;
    // commit to state.imports only after all keys succeed.
    std::vector<RemoteRegistration> newImports;
    auto rollbackNewImports = [&]() {
        for (const auto &ni : newImports) {
            if (!ni.descBytes.empty()) {
                (void)manager_.HcommMemUnimport(localEndpoint_, ni.descBytes.data(),
                                                static_cast<uint32_t>(ni.descBytes.size()));
            }
        }
        newImports.clear();
    };

    for (const auto &key : memKeys) {
        // --- 1. Validate top-level magic ---
        if (key.keys[0] != URMA_EXPORT_DESC_MAGIC) {
            BM_LOG_ERROR("device_urma ImportRemoteMemKeysLocked invalid key magic 0x"
                         << std::hex << key.keys[0] << ", peer: " << peerRank << ", expected: 0x"
                         << URMA_EXPORT_DESC_MAGIC);
            rollbackNewImports();
            return BM_INVALID_PARAM;
        }

        const uint64_t remoteAddr = key.keys[1];
        const uint64_t remoteSize = key.keys[2];
        const uint64_t memTag = key.keys[3];

        if (remoteAddr == 0 || remoteSize == 0) {
            BM_LOG_ERROR("device_urma ImportRemoteMemKeysLocked zero addr(0x"
                         << std::hex << remoteAddr << ") or size(0x" << remoteSize << ") in key, peer: " << peerRank);
            rollbackNewImports();
            return BM_INVALID_PARAM;
        }

        // --- 2. Parse UrmaExportDesc header from keys[4..56] ---
        const uint8_t *raw = reinterpret_cast<const uint8_t *>(&key.keys[DEVICE_URMA_EXPORT_KEY_HEADER_SLOTS]);
        UrmaExportDesc exportDesc{};
        std::memcpy(&exportDesc, raw, sizeof(UrmaExportDesc));

        if (exportDesc.magic != URMA_EXPORT_DESC_MAGIC || exportDesc.version != URMA_EXPORT_DESC_VERSION) {
            BM_LOG_ERROR("device_urma ImportRemoteMemKeysLocked invalid UrmaExportDesc magic/version in key, peer: "
                         << peerRank);
            rollbackNewImports();
            return BM_INVALID_PARAM;
        }
        if (exportDesc.headerSize != sizeof(UrmaExportDesc)) {
            BM_LOG_ERROR(
                "device_urma ImportRemoteMemKeysLocked UrmaExportDesc headerSize mismatch, peer: " << peerRank);
            rollbackNewImports();
            return BM_INVALID_PARAM;
        }
        if (exportDesc.hcommDescLen == 0) {
            BM_LOG_ERROR("device_urma ImportRemoteMemKeysLocked hcommDescLen is 0, peer: " << peerRank);
            rollbackNewImports();
            return BM_INVALID_PARAM;
        }

        const uint32_t memDescLen = sizeof(UrmaExportDesc) + exportDesc.hcommDescLen;
        if (memDescLen > DEVICE_URMA_EXPORT_KEY_DATA_BYTES) {
            BM_LOG_ERROR("device_urma ImportRemoteMemKeysLocked memDescLen " << memDescLen << " exceeds key capacity "
                                                                             << DEVICE_URMA_EXPORT_KEY_DATA_BYTES
                                                                             << ", peer: " << peerRank);
            rollbackNewImports();
            return BM_INVALID_PARAM;
        }

        // Cross-check memTag consistency (best-effort warning, not a hard error)
        if (exportDesc.memTag != memTag) {
            BM_LOG_DEBUG("device_urma ImportRemoteMemKeysLocked memTag cross-check mismatch, key: "
                         << memTag << " exportDesc: " << exportDesc.memTag << ", peer: " << peerRank);
        }

        // --- 3. Idempotency: skip if already imported (by memTag) ---
        auto it = std::find_if(state.imports.begin(), state.imports.end(),
                               [memTag](const auto &r) { return r.memTag == memTag; });
        if (it != state.imports.end()) {
            BM_LOG_DEBUG("device_urma ImportRemoteMemKeysLocked skip duplicate memTag: " << memTag
                                                                                         << ", peer: " << peerRank);
            continue;
        }

        // --- 4. Protocol compatibility check before import ---
        if (state.remoteEndpointDesc.protocol != localEndpoint_->desc.protocol) {
            BM_LOG_ERROR("device_urma ImportRemoteMemKeysLocked protocol mismatch, peer: "
                         << peerRank << " remote protocol: " << state.remoteEndpointDesc.protocol
                         << " local protocol: " << localEndpoint_->desc.protocol);
            rollbackNewImports();
            return BM_INVALID_PARAM;
        }

        // --- 5. HcommMemImport using global localEndpoint_ ---
        UrmaCommMem view{};
        auto ret = manager_.HcommMemImport(localEndpoint_, raw, memDescLen, &view);
        if (ret != BM_OK) {
            BM_LOG_ERROR("device_urma ImportRemoteMemKeysLocked HcommMemImport failed, memTag: "
                         << memTag << " peer: " << peerRank << " ret: " << ret);
            rollbackNewImports();
            return ret;
        }

        // --- 6. Build RemoteRegistration and collect in local vector ---
        RemoteRegistration remote{};
        remote.addr = remoteAddr;
        remote.size = remoteSize;
        remote.memTag = memTag;
        remote.descBytes.assign(raw, raw + memDescLen);
        remote.view = view;
        newImports.emplace_back(std::move(remote));

        BM_LOG_INFO("device_urma ImportRemoteMemKeysLocked imported mem, peer: "
                    << peerRank << " memTag: " << memTag << " addr: 0x" << std::hex << remoteAddr << " size: 0x"
                    << remoteSize << " view: 0x" << view.addr);
    }

    // --- All succeeded: commit to state ---
    for (auto &ni : newImports) {
        state.imports.emplace_back(std::move(ni));
    }
    BM_LOG_INFO("device_urma ImportRemoteMemKeysLocked success, peer: " << peerRank << " imported " << newImports.size()
                                                                        << " keys");
    return BM_OK;
}

Result DeviceUrmaTransportManager::Prepare(const HybmTransPrepareOptions &options)
{
    std::lock_guard<std::mutex> guard(mutex_);
    auto ret = EnsureOpenLocked();
    if (ret != BM_OK) {
        return ret;
    }
    if (localEndpoint_ == nullptr) {
        BM_LOG_ERROR("device_urma Prepare failed: localEndpoint_ is null (OpenDevice may not have completed)");
        return BM_NOT_INITIALIZED;
    }
    for (const auto &item : options.options) {
        const uint32_t peerRank = item.first;
        if (peerRank >= rankCount_) {
            BM_LOG_ERROR("device_urma Prepare invalid peerRank: " << peerRank << " rankCount: " << rankCount_);
            return BM_INVALID_PARAM;
        }
        if (peerRank == rankId_) {
            BM_LOG_WARN("device_urma Prepare skipping self rank: " << peerRank);
            continue;
        }
        auto &state = remoteRanks_[peerRank];

        // 1. Parse peer UrmaEndpointDesc from privateData
        UrmaEndpointDesc peerDesc{};
        ret = ParsePrivateDataToEndpointDesc(item.second.privateData, peerDesc);
        if (ret != BM_OK) {
            BM_LOG_ERROR("device_urma Prepare failed to parse peer endpoint desc, peer: " << peerRank);
            return ret;
        }

        bool resourcesCreatedInThisCall = false;

        // 2. Idempotency: if thread and channels already exist, compare with stored remoteEndpointDesc.
        //    If unchanged → skip resource creation; if changed → reject (no hot-replace).
        //    Either way, fall through to mem import — do NOT continue.
        if (!state.channels.empty() && state.thread != 0) {
            if (std::memcmp(&state.remoteEndpointDesc, &peerDesc, sizeof(UrmaEndpointDesc)) != 0) {
                BM_LOG_ERROR(
                    "device_urma Prepare peer endpoint desc changed but hot-replace not supported, peer: " << peerRank);
                return BM_INVALID_PARAM;
            }
            BM_LOG_INFO("device_urma Prepare reusing existing channel/thread for peer: " << peerRank);
            // resourcesCreatedInThisCall remains false — do not destroy channel/thread on import failure
        } else {
            // 3. Convert peer UrmaEndpointDesc → HCOMM EndpointDesc for HcommChannelDesc.remoteEndpoint
            EndpointDesc hcommRemoteEndpoint = ToHcommEndpointDesc(peerDesc);

            // 4. Build a single HcommChannelDesc for this peer
            HcommChannelDesc channelDesc{};
            HcommChannelDescInit(&channelDesc, 1); // not in DlHcommApi namespace, direct C function
            channelDesc.role = (rankId_ > peerRank) ? HCOMM_SOCKET_ROLE_CLIENT : HCOMM_SOCKET_ROLE_SERVER;
            channelDesc.remoteEndpoint = hcommRemoteEndpoint;
            channelDesc.notifyNum = HCOMM_NORMAL_NOTIFY_NUM;
            channelDesc.exchangeAllMems = true; // 填true, 不用管memHandles了, remoteEndpoint要填对

            // 5. Allocate one thread per peer (use temporary variable for safe rollback)
            HcommThreadHandle threadHandle = 0;
            ret = DlHcommApi::HcommThreadAlloc(COMM_ENGINE_AICPU, 1, &HCOMM_NORMAL_NOTIFY_NUM, &threadHandle);
            if (ret != BM_OK) {
                BM_LOG_ERROR("device_urma Prepare HcommThreadAlloc failed, peer: "
                             << peerRank << " engine: " << COMM_ENGINE_AICPU << " ret: " << ret);
                return ret;
            }
            if (threadHandle == 0) {
                BM_LOG_ERROR("device_urma Prepare HcommThreadAlloc returned invalid thread, peer: " << peerRank);
                return BM_DL_FUNCTION_FAILED;
            }

            // 6. Create one channel per peer (temporary variable for safe rollback)
            HcommChannelHandle channelHandle = 0;
            ret = DlHcommApi::HcommChannelCreate(localEndpoint_->hcommEndpoint, COMM_ENGINE_AICPU, &channelDesc, 1,
                                                 &channelHandle);
            if (ret != 0) {
                BM_LOG_ERROR("device_urma Prepare HcommChannelCreate failed, peer: " << peerRank << " ret: " << ret);
                auto rollbackThread = threadHandle;
                // No stream sync needed: thread was just allocated, no RemoteIO launched during Prepare.
                (void)DlHcommApi::HcommThreadFree(&rollbackThread, 1);
                return BM_DL_FUNCTION_FAILED;
            }
            if (channelHandle == 0) {
                BM_LOG_ERROR("device_urma Prepare HcommChannelCreate returned invalid channel, peer: " << peerRank);
                auto rollbackThread = threadHandle;
                // No stream sync needed: thread was just allocated, no RemoteIO launched during Prepare.
                (void)DlHcommApi::HcommThreadFree(&rollbackThread, 1);
                return BM_DL_FUNCTION_FAILED;
            }

            // 7. Persist new resources to state
            state.remoteEndpointDesc = peerDesc;
            state.hasEndpointDesc = true;
            state.thread = threadHandle;
            state.channelDescs.clear();
            state.channelDescs.emplace_back(channelDesc);
            state.channels.clear();
            state.channels.emplace_back(channelHandle);
            resourcesCreatedInThisCall = true;

            BM_LOG_INFO("device_urma Prepare created channel/thread, peer: " << peerRank << " thread: " << threadHandle
                                                                             << " channel: " << channelHandle);
        }

        // 8. Import remote memory keys (always, even if channel/thread were reused)
        ret = ImportRemoteMemKeysLocked(peerRank, state, item.second.memKeys);
        if (ret != BM_OK) {
            BM_LOG_ERROR("device_urma Prepare ImportRemoteMemKeysLocked failed, peer: " << peerRank);
            if (resourcesCreatedInThisCall) {
                // Rollback the channel/thread that were just created in this call
                BM_LOG_WARN("device_urma Prepare rolling back newly created channel/thread for peer: " << peerRank);
                if (!state.channels.empty()) {
                    auto chanRollback = state.channels.front();
                    // No stream sync needed: channel was just created, no RemoteIO launched during Prepare.
                    (void)DlHcommApi::HcommChannelDestroy(&chanRollback, 1);
                    state.channels.clear();
                }
                if (state.thread != 0) {
                    auto threadRollback = state.thread;
                    (void)DlHcommApi::HcommThreadFree(&threadRollback, 1);
                    state.thread = 0;
                }
                state.channelDescs.clear();
                state.remoteEndpointDesc = UrmaEndpointDesc{};
                state.hasEndpointDesc = false;
            }
            // If resourcesCreatedInThisCall is false, ImportRemoteMemKeysLocked already
            // rolled back its own partial imports; we leave pre-existing channel/thread intact.
            return ret;
        }

        BM_LOG_INFO("device_urma Prepare success, peer: " << peerRank << " thread: " << state.thread << " channel: "
                                                          << (state.channels.empty() ? 0 : state.channels.front())
                                                          << " imports: " << state.imports.size());
    }
    return BM_OK;
}

Result DeviceUrmaTransportManager::RemoveRankLocked(uint32_t rankId)
{
    auto rankIt = remoteRanks_.find(rankId);
    if (rankIt == remoteRanks_.end()) {
        return BM_OK;
    }
    auto &state = rankIt->second;
    Result finalRet = BM_OK;
    if (state.pendingOps != 0 && !state.channels.empty()) {
        finalRet = SynchronizeDeviceKernelStream();
        state.pendingOps = 0;
    }
    for (const auto channel : state.channels) {
        if (channel != 0) {
            auto hcommChan = channel;
            const auto ret = DlHcommApi::HcommChannelDestroy(&hcommChan, 1);
            if (ret != 0 && finalRet == BM_OK) {
                BM_LOG_ERROR("device_urma HcommChannelDestroy failed, channel: " << channel << " ret: " << ret);
                finalRet = BM_DL_FUNCTION_FAILED;
            }
        }
    }
    state.channels.clear();
    if (state.thread != 0) {
        const auto syncRet = SynchronizeDeviceKernelStream();
        if (syncRet != BM_OK && finalRet == BM_OK) {
            finalRet = syncRet;
        }
        auto hcommThread = state.thread;
        const auto ret = DlHcommApi::HcommThreadFree(&hcommThread, 1);
        if (ret != BM_OK && finalRet == BM_OK) {
            BM_LOG_ERROR("device_urma HcommThreadFree failed, thread: " << state.thread << " ret: " << ret);
            finalRet = ret;
        }
        state.thread = 0;
    }

    for (auto &remote : state.imports) {
        if (!remote.descBytes.empty()) {
            const auto ret = manager_.HcommMemUnimport(localEndpoint_, remote.descBytes.data(),
                                                       static_cast<uint32_t>(remote.descBytes.size()));
            if (ret != BM_OK && finalRet == BM_OK) {
                finalRet = ret;
            }
        }
    }
    state.imports.clear();
    for (auto &item : localRegistrations_) {
        auto handleIt = item.second.peerHandles.find(rankId);
        if (handleIt == item.second.peerHandles.end() || handleIt->second == INVALID_MEM_HANDLE) {
            continue;
        }
        const auto ret = manager_.HcommMemUnreg(state.localEndpoint, handleIt->second);
        if (ret != BM_OK && finalRet == BM_OK) {
            finalRet = ret;
        }
        item.second.peerHandles.erase(handleIt);
    }
    if (state.localEndpoint != nullptr) {
        const auto ret = HcomUrmaDestroyEndpoint(state.localEndpoint->hcommEndpoint);
        if (ret != BM_OK && finalRet == BM_OK) {
            finalRet = ret;
        }
        state.localEndpoint.reset();
    }
    remoteRanks_.erase(rankIt);
    return finalRet;
}

Result DeviceUrmaTransportManager::RemoveRanks(const std::vector<uint32_t> &removedRanks)
{
    std::lock_guard<std::mutex> guard(mutex_);
    auto ret = EnsureOpenLocked();
    if (ret != BM_OK) {
        return ret;
    }
    Result finalRet = BM_OK;
    for (auto rankId : removedRanks) {
        ret = RemoveRankLocked(rankId);
        if (ret != BM_OK && finalRet == BM_OK) {
            finalRet = ret;
        }
    }
    return finalRet;
}

Result DeviceUrmaTransportManager::Connect()
{
    connected_ = true;
    return BM_OK;
}

Result DeviceUrmaTransportManager::AsyncConnect()
{
    return BM_OK;
}

Result DeviceUrmaTransportManager::WaitForConnected(int64_t timeoutNs)
{
    (void)timeoutNs;
    return BM_OK;
}

Result DeviceUrmaTransportManager::UpdateRankOptions(const HybmTransPrepareOptions &options)
{
    bool needFallback = false;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto ret = EnsureOpenLocked();
        if (ret != BM_OK) {
            return ret;
        }
        if (localEndpoint_ == nullptr) {
            BM_LOG_ERROR("device_urma UpdateRankOptions failed: localEndpoint_ is null");
            return BM_NOT_INITIALIZED;
        }
        for (const auto &item : options.options) {
            const uint32_t peerRank = item.first;
            if (peerRank >= rankCount_) {
                BM_LOG_ERROR("device_urma UpdateRankOptions invalid peerRank: " << peerRank
                                                                                << " rankCount: " << rankCount_);
                return BM_INVALID_PARAM;
            }
            if (peerRank == rankId_) {
                BM_LOG_WARN("device_urma UpdateRankOptions skipping self rank: " << peerRank);
                continue;
            }
            auto stateIt = remoteRanks_.find(peerRank);
            if (stateIt == remoteRanks_.end()) {
                BM_LOG_WARN("device_urma UpdateRankOptions peer rank " << peerRank
                                                                       << " not prepared yet, fallback to Prepare");
                needFallback = true;
                break;
            }
            auto &state = stateIt->second;
            if (state.channels.empty() || state.thread == 0) {
                BM_LOG_WARN("device_urma UpdateRankOptions peer rank "
                            << peerRank << " has no channels/thread, fallback to Prepare");
                needFallback = true;
                break;
            }
        }
    } // mutex_ released

    if (needFallback) {
        // Fallback to Prepare (without holding mutex_) — it will create resources and import memKeys.
        BM_LOG_INFO("device_urma UpdateRankOptions falling back to Prepare for new dynamic ranks");
        return Prepare(options);
    }

    // Only import remote memory keys without re-creating resources.
    std::lock_guard<std::mutex> guard(mutex_);
    for (const auto &item : options.options) {
        const uint32_t peerRank = item.first;
        if (peerRank >= rankCount_ || peerRank == rankId_) {
            continue; // already validated/skipped above
        }
        auto stateIt = remoteRanks_.find(peerRank);
        if (stateIt == remoteRanks_.end()) {
            continue;
        }
        auto &state = stateIt->second;
        auto ret = ImportRemoteMemKeysLocked(peerRank, state, item.second.memKeys);
        if (ret != BM_OK) {
            BM_LOG_ERROR("device_urma UpdateRankOptions ImportRemoteMemKeysLocked failed, peer: " << peerRank);
            return ret;
        }
        BM_LOG_INFO("device_urma UpdateRankOptions success, peer: " << peerRank);
    }
    return BM_OK;
}

const std::string &DeviceUrmaTransportManager::GetNic() const
{
    static const std::string emptyNic;
    return emptyNic;
}

const TransportPrivateData DeviceUrmaTransportManager::GetPrivateData() const
{
    std::lock_guard<std::mutex> guard(mutex_);
    TransportPrivateData data{};
    if (localEndpoint_ == nullptr) {
        BM_LOG_WARN("device_urma GetPrivateData called before localEndpoint_ is ready, returning empty");
        return data; // empty data, peer's ParsePrivateDataToEndpointDesc will reject with magic mismatch
    }
    UrmaPrivateDataDesc header{};
    header.payloadLen = static_cast<uint16_t>(sizeof(UrmaEndpointDesc));
    uint8_t *raw = reinterpret_cast<uint8_t *>(data.key.keys);
    std::memcpy(raw, &header, sizeof(UrmaPrivateDataDesc));
    std::memcpy(raw + sizeof(UrmaPrivateDataDesc), &localEndpointDesc_, sizeof(UrmaEndpointDesc));
    return data;
}

Result DeviceUrmaTransportManager::RemoteIo(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size, bool write)
{
    if (size == 0) {
        return BM_OK;
    }
    std::lock_guard<std::mutex> guard(mutex_);
    auto ret = EnsureOpenLocked();
    if (ret != BM_OK) {
        return ret;
    }
    RemoteRegistration remote{};
    ret = FindRemoteRegistrationLocked(rankId, rAddr, size, &remote);
    if (ret != BM_OK) {
        BM_LOG_ERROR("device_urma remote address is not prepared, rank: " << rankId << " addr: " << std::hex << rAddr);
        return ret;
    }
    auto &state = remoteRanks_[rankId];
    if (state.channels.empty()) {
        return BM_NOT_CONNECTED;
    }
    const auto translatedRemoteAddr = remote.view.addr + (rAddr - remote.addr);
    const auto channel = state.channels.front();
    if (lAddr == 0 || translatedRemoteAddr == 0) {
        BM_LOG_ERROR("device_urma RemoteIo: localAddr or remoteAddr is 0");
        return BM_INVALID_PARAM;
    }
    std::vector<uint64_t> localAddrs = {lAddr};
    std::vector<uint64_t> remoteAddrs = {translatedRemoteAddr};
    std::vector<uint64_t> sizes = {size};
    ret = LaunchDeviceKernelBatch(state.thread, !write, channel, localAddrs, remoteAddrs, sizes);
    if (ret != BM_OK) {
        return ret;
    }
    state.pendingOps++;
    return BM_OK;
}

Result DeviceUrmaTransportManager::ReadRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    auto ret = ReadRemoteAsync(rankId, lAddr, rAddr, size);
    if (ret != BM_OK) {
        return ret;
    }
    return Synchronize(rankId);
}

Result DeviceUrmaTransportManager::WriteRemote(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    auto ret = WriteRemoteAsync(rankId, lAddr, rAddr, size);
    if (ret != BM_OK) {
        return ret;
    }
    return Synchronize(rankId);
}

Result DeviceUrmaTransportManager::ReadRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    return RemoteIo(rankId, lAddr, rAddr, size, false);
}

Result DeviceUrmaTransportManager::WriteRemoteAsync(uint32_t rankId, uint64_t lAddr, uint64_t rAddr, uint64_t size)
{
    return RemoteIo(rankId, lAddr, rAddr, size, true);
}

Result DeviceUrmaTransportManager::RemoteIoBatch(uint32_t rankId, const CopyDescriptor &descriptor, bool write)
{
    const auto &localAddrs = descriptor.localAddrs;
    const auto &globalAddrs = descriptor.globalAddrs;
    const auto &counts = descriptor.counts;
    const auto batchSize = counts.size();

    if (batchSize == 0) {
        return BM_OK;
    }
    if (localAddrs.size() != batchSize || globalAddrs.size() != batchSize) {
        BM_LOG_ERROR("device_urma RemoteIoBatch: descriptor vector sizes mismatch, local="
                     << localAddrs.size() << " global=" << globalAddrs.size() << " counts=" << batchSize);
        return BM_INVALID_PARAM;
    }

    std::lock_guard<std::mutex> guard(mutex_);
    auto ret = EnsureOpenLocked();
    if (ret != BM_OK) {
        return ret;
    }
    auto rankIt = remoteRanks_.find(rankId);
    if (rankIt == remoteRanks_.end()) {
        BM_LOG_ERROR("device_urma RemoteIoBatch rank not found: " << rankId);
        return BM_NOT_CONNECTED;
    }
    auto &state = rankIt->second;
    if (state.channels.empty()) {
        BM_LOG_ERROR("device_urma RemoteIoBatch no channels for rank: " << rankId);
        return BM_NOT_CONNECTED;
    }

    std::vector<uint64_t> localVec;
    std::vector<uint64_t> remoteVec;
    std::vector<uint64_t> sizeVec;
    localVec.reserve(batchSize);
    remoteVec.reserve(batchSize);
    sizeVec.reserve(batchSize);

    for (uint32_t i = 0; i < batchSize; ++i) {
        const uint64_t lAddr = reinterpret_cast<uint64_t>(localAddrs[i]);
        const uint64_t rAddr = reinterpret_cast<uint64_t>(globalAddrs[i]);
        const uint64_t size = counts[i];
        if (size == 0) {
            continue;
        }

        RemoteRegistration remote{};
        ret = FindRemoteRegistrationLocked(rankId, rAddr, size, &remote);
        if (ret != BM_OK) {
            BM_LOG_ERROR("device_urma RemoteIoBatch remote address not prepared, rank: "
                         << rankId << " addr: 0x" << std::hex << rAddr << " index: " << i);
            return ret;
        }
        const auto translatedRemoteAddr = remote.view.addr + (rAddr - remote.addr);
        localVec.push_back(lAddr);
        remoteVec.push_back(translatedRemoteAddr);
        sizeVec.push_back(size);
    }

    if (localVec.empty()) {
        return BM_OK;
    }

    const auto channel = state.channels.front();
    ret = LaunchDeviceKernelBatch(state.thread, !write, channel, localVec, remoteVec, sizeVec);
    if (ret != BM_OK) {
        return ret;
    }
    state.pendingOps++;
    return BM_OK;
}

Result DeviceUrmaTransportManager::WriteRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor)
{
    return RemoteIoBatch(rankId, descriptor, true);
}

Result DeviceUrmaTransportManager::ReadRemoteBatchAsync(uint32_t rankId, const CopyDescriptor &descriptor)
{
    return RemoteIoBatch(rankId, descriptor, false);
}

Result DeviceUrmaTransportManager::Synchronize(uint32_t rankId)
{
    std::lock_guard<std::mutex> guard(mutex_);
    auto ret = EnsureOpenLocked();
    if (ret != BM_OK) {
        return ret;
    }
    auto rankIt = remoteRanks_.find(rankId);
    if (rankIt == remoteRanks_.end()) {
        return BM_NOT_CONNECTED;
    }
    if (rankIt->second.pendingOps == 0) {
        return BM_OK;
    }
    if (rankIt->second.channels.empty()) {
        return BM_NOT_CONNECTED;
    }
    ret = SynchronizeDeviceKernelStream();
    if (ret != BM_OK) {
        return ret;
    }
    rankIt->second.pendingOps = 0;
    return BM_OK;
}

} // namespace device
} // namespace transport
} // namespace mf
} // namespace ock
