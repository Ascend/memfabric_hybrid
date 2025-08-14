/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 */
#include <cstring>
#include <climits>
#include <fstream>
#include "dl_api.h"
#include "dl_hal_api.h"
#include "dl_acl_api.h"
#include "devmm_svm_gva.h"
#include "hybm_logger.h"
#include "hybm_networks_common.h"
#include "hybm_ex_info_transfer.h"
#include "hybm_devide_mem_segment.h"
#include "hybm_cmd.h"

namespace ock {
namespace mf {
static constexpr uint32_t invalidSuperPodId = 0xFFFFFFFFU;
static constexpr uint32_t invalidServerId = 0x3FFU;

int MemSegmentDevice::deviceId_{-1};
int MemSegmentDevice::pid_{-1};
uint32_t MemSegmentDevice::sdid_{0};
uint32_t MemSegmentDevice::serverId_{0};
uint32_t MemSegmentDevice::superPodId_{0};
const std::string LEAST_DRIVER_VER = "V100R001C21B035";

Result MemSegmentDevice::ValidateOptions() noexcept
{
    if (options_.segType != HYBM_MST_HBM || options_.size == 0 || (options_.size % DEVICE_LARGE_PAGE_SIZE) != 0) {
        return BM_INVALID_PARAM;
    }

    return BM_OK;
}

static std::string GetDriverVersionPath(const std::string &driverEnvStr, const std::string &keyStr)
{
    std::string driverVersionPath;
    std::string tempPath; // 存放临时路径
    // 查找driver安装路径
    for (uint32_t i = 0; i < driverEnvStr.length(); ++i) {
        // 环境变量中存放的每段路径之间以':'隔开
        if (driverEnvStr[i] != ':') {
            tempPath += driverEnvStr[i];
        }
        // 对存放driver版本文件的路径进行搜索
        if (driverEnvStr[i] == ':' || i == driverEnvStr.length() - 1) {
            auto found = tempPath.find(keyStr);
            if (found == std::string::npos) {
                tempPath.clear();
                continue;
            }
            // 确保不是部分匹配
            if (tempPath.length() <= found + keyStr.length() || tempPath[found + keyStr.length()] == '/') {
                driverVersionPath = tempPath.substr(0, found);
                break;
            }
            tempPath.clear();
        }
    }
    return driverVersionPath;
}

static std::string LoadDriverVersionInfoFile(const std::string &realName, const std::string &keyStr)
{
    std::string driverVersion;
    // 打开该文件前，判断该文件路径是否有效、规范
    char realFile[PATH_MAX] = {0};
    if (realpath(realName.c_str(), realFile) == nullptr) {
        BM_LOG_WARN("driver version path " << realName << " is not a valid real path");
        return "";
    }

    // realFile转str,然后open这个str
    std::ifstream infile(realFile, std::ifstream::in);
    if (!infile.is_open()) {
        BM_LOG_WARN("driver version file " << realFile << " does not exist");
        return "";
    }

    // 逐行读取，结果放在line中，寻找带有keyStr的字符串
    std::string line;
    int32_t maxRows = 100; // 在文件中读取的最长行数为100，避免超大文件长时间读取
    while (getline(infile, line)) {
        --maxRows;
        if (maxRows < 0) {
            BM_LOG_WARN("driver version file content is too long.");
            return "";
        }
        auto found = line.find(keyStr);
        // 刚好匹配前缀
        if (found == 0) {
            uint32_t len = line.length() - keyStr.length(); // 版本字符串长度
            driverVersion = line.substr(keyStr.length(), len); // 从keyStr截断
            break;
        }
    }
    infile.close();
    return driverVersion;
}

static std::string CastDriverVersion(const std::string &driverEnv)
{
    std::string driverVersionPath = GetDriverVersionPath(driverEnv, "/driver/lib64");
    if (!driverVersionPath.empty()) {
        driverVersionPath += "/driver/version.info";
        std::string driverVersion = LoadDriverVersionInfoFile(driverVersionPath, "Innerversion=");
        return driverVersion;
    }
    BM_LOG_WARN("cannot found version file in :" << driverEnv);
    return "";
}

static int32_t GetValueFromVersion(const std::string &ver, std::string key)
{
    int32_t val = 0;
    auto found = ver.find(key);
    if (found == std::string::npos) {
        return -1;
    }

    std::string tmp;
    while (++found < ver.length()) {
        if (std::isdigit(ver[found])) {
            tmp += ver[found];
        } else {
            break;
        }
    }

    try {
        val = std::stoi(tmp);
    } catch (...) {
        val = -1;
    }
    return val;
}

static bool DriverVersionCheck(const std::string &ver)
{
    auto libPath = std::getenv("LD_LIBRARY_PATH");
    if (libPath == nullptr) {
        BM_LOG_ERROR("check driver version failed, Environment LD_LIBRARY_PATH not set.");
        return false;
    }

    std::string readVer = CastDriverVersion(libPath);
    if (readVer.empty()) {
        BM_LOG_ERROR("check driver version failed, read version is empty.");
        return false;
    }

    int32_t baseVal = GetValueFromVersion(ver, "V");
    int32_t readVal = GetValueFromVersion(readVer, "V");
    if (baseVal == -1 || readVal == -1 || baseVal != readVal) {
        BM_LOG_ERROR("check driver version failed, Version not equal, limit:" << ver << " read:" << readVer);
        return false;
    }

    baseVal = GetValueFromVersion(ver, "R");
    readVal = GetValueFromVersion(readVer, "R");
    if (baseVal == -1 || readVal == -1 || baseVal != readVal) {
        BM_LOG_ERROR("check driver version failed, Release not equal, limit:" << ver << " read:" << readVer);
        return false;
    }

    baseVal = GetValueFromVersion(ver, "C");
    readVal = GetValueFromVersion(readVer, "C");
    if (baseVal == -1 || readVal == -1 || readVal < baseVal) {
        BM_LOG_ERROR("check driver version failed, Customer is too low, limit:" << ver << " read:" << readVer);
        return false;
    }
    if (readVal > baseVal) {
        return true;
    }

    baseVal = GetValueFromVersion(ver, "B");
    readVal = GetValueFromVersion(readVer, "B");
    if (baseVal == -1 || readVal == -1 || readVal < baseVal) {
        BM_LOG_ERROR("check driver version failed, Build is too low, input:" << ver << " read:" << readVer);
        return false;
    }
    return true;
}

int32_t HalGvaPrecheck(void)
{
    if (DriverVersionCheck(LEAST_DRIVER_VER)) {
        return BM_OK;
    }
    return BM_ERROR;
}

Result MemSegmentDevice::ReserveMemorySpace(void **address) noexcept
{
    if (globalVirtualAddress_ != nullptr) {
        BM_LOG_ERROR("already prepare virtual memory.");
        return BM_ERROR;
    }

    if (HalGvaPrecheck() != BM_OK) {
        BM_LOG_ERROR("the current version of ascend driver does not support global virtual address!");
        return BM_ERROR;
    }

    void *globalMemoryBase = nullptr;
    size_t allocSize = HYBM_DEVICE_INFO_SIZE;  // 申请meta空间
    drv::HybmInitialize(deviceId_, DlHalApi::GetDevmmFd());
    auto ret = drv::HalGvaReserveMemory((uint64_t *)&globalMemoryBase, allocSize, (int32_t)deviceId_, 0);
    if (ret != 0) {
        BM_LOG_ERROR("initialize mete memory with size: " << allocSize << " failed: " << ret);
        return -1;
    }

    ret = drv::HalGvaAlloc(HYBM_DEVICE_META_ADDR, HYBM_DEVICE_INFO_SIZE, 0);
    if (ret != BM_OK) {
        (void)drv::HalGvaUnreserveMemory();
        BM_LOG_ERROR("HalGvaAlloc hybm meta memory failed: " << ret);
        return BM_MALLOC_FAILED;
    }

    uint64_t base = 0;
    totalVirtualSize_ = options_.rankCnt * options_.size;
    ret = drv::HalGvaReserveMemory(&base, totalVirtualSize_, deviceId_, 0ULL);
    if (ret != 0 || base == 0) {
        drv::HalGvaFree(HYBM_DEVICE_META_ADDR, HYBM_DEVICE_INFO_SIZE);
        BM_LOG_ERROR("prepare virtual memory size(" << totalVirtualSize_ << ") failed. ret: " << ret);
        return BM_MALLOC_FAILED;
    }

    globalVirtualAddress_ = reinterpret_cast<uint8_t *>(base);
    allocatedSize_ = 0UL;
    sliceCount_ = 0;
    *address = globalVirtualAddress_;
    return BM_OK;
}

Result MemSegmentDevice::AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    if ((size % DEVICE_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.size) {
        BM_LOG_ERROR("invalid allocate memory size : " << size << ", now used " << allocatedSize_ << " of "
                                                       << options_.size);
        return BM_INVALID_PARAM;
    }

    auto localVirtualBase = globalVirtualAddress_ + options_.size * options_.rankId;
    auto ret = drv::HalGvaAlloc((uint64_t)(localVirtualBase + allocatedSize_), size, 0);
    if (ret != BM_OK) {
        BM_LOG_ERROR("HalGvaAlloc memory failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    auto sliceAddr = localVirtualBase + allocatedSize_;
    allocatedSize_ += size;
    slice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_DEVICE_HBM, MEM_PT_TYPE_SVM,
                                       (uint64_t)(ptrdiff_t)(void *)sliceAddr, size);
    slices_.emplace(slice->index_, slice);
    BM_LOG_DEBUG("allocate slice(idx:" << slice->index_ << ", size:" << slice->size_ << ").");

    return BM_OK;
}

Result MemSegmentDevice::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

// export不可重入
Result MemSegmentDevice::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
{
    BM_ASSERT_RETURN(slice != nullptr, BM_INVALID_PARAM);

    auto pos = slices_.find(slice->index_);
    if (pos == slices_.end()) {
        BM_LOG_ERROR("input slice(idx:" << slice->index_ << ") not exist.");
        return BM_INVALID_PARAM;
    }

    if (pos->second.slice != slice) {
        BM_LOG_ERROR("input slice(magic:" << std::hex << slice->magic_ << ") not match.");
        return BM_INVALID_PARAM;
    }

    auto exp = exportMap_.find(slice->index_);
    if (exp != exportMap_.end()) {  // RtIpcSetMemoryName不支持重复调用
        exInfo = exp->second;
        return BM_OK;
    }

    HbmExportInfo info{};
    auto ret = DlAclApi::RtIpcSetMemoryName((void *)(ptrdiff_t)slice->vAddress_, slice->size_, info.shmName,
                                            sizeof(info.shmName));
    if (ret != 0) {
        BM_LOG_ERROR("set memory name failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    info.magic = EXPORT_INFO_MAGIC;
    info.version = EXPORT_INFO_VERSION;
    info.mappingOffset =
        slice->vAddress_ - (uint64_t)(ptrdiff_t)(globalVirtualAddress_ + options_.size * options_.rankId);
    info.sliceIndex = static_cast<uint32_t>(slice->index_);
    info.deviceId = deviceId_;
    info.pid = pid_;
    info.rankId = options_.rankId;
    info.size = slice->size_;
    info.entityId = entityId_;
    info.sdid = sdid_;
    info.serverId = serverId_;
    info.superPodId = superPodId_;
    info.pageTblType = MEM_PT_TYPE_SVM;
    info.memSegType = HYBM_MST_HBM;
    info.exchangeType = HYBM_INFO_EXG_IN_NODE;
    ret = LiteralExInfoTranslater<HbmExportInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        BM_LOG_ERROR("export info failed: " << ret);
        DlAclApi::RtIpcDestroyMemoryName(info.shmName);
        return BM_ERROR;
    }

    exportMap_[slice->index_] = exInfo;
    return BM_OK;
}

// import可重入
Result MemSegmentDevice::Import(const std::vector<std::string> &allExInfo) noexcept
{
    std::map<uint16_t, HbmExportInfo> importMap;
    LiteralExInfoTranslater<HbmExportInfo> translator;
    std::vector<HbmExportInfo> deserializedInfos(allExInfo.size());
    for (auto i = 0U; i < allExInfo.size(); i++) {
        auto ret = translator.Deserialize(allExInfo[i], deserializedInfos[i]);
        if (ret != 0) {
            BM_LOG_ERROR("deserialize imported info(" << i << ") failed.");
            return BM_INVALID_PARAM;
        }
        importMap.emplace(deserializedInfos[i].rankId, deserializedInfos[i]);
    }
    importMap_ = std::move(importMap);

    uint32_t localIdx = UINT32_MAX;
    for (auto i = 0U; i < deserializedInfos.size(); i++) {
        if (deserializedInfos[i].magic != EXPORT_INFO_MAGIC) {
            BM_LOG_ERROR("import info(" << i << ") magic(" << deserializedInfos[i].magic << ") invalid.");
            return BM_INVALID_PARAM;
        }

        if (deserializedInfos[i].rankId == options_.rankId) {
            localIdx = i;
        }
    }
    BM_ASSERT_RETURN(localIdx < deserializedInfos.size(), BM_INVALID_PARAM);

    for (auto i = 0U; i < deserializedInfos.size(); i++) {
        if (deserializedInfos[i].rankId == options_.rankId) {
            continue;
        }

        if (deserializedInfos[i].deviceId != deviceId_) {
            auto ret = DlAclApi::AclrtDeviceEnablePeerAccess(deserializedInfos[i].deviceId, 0);
            if (ret != 0) {
                BM_LOG_ERROR("enable device access failed:" << ret << " local_device:" << deviceId_
                                                            << " remote_device:" << (int)deserializedInfos[i].deviceId);
                return BM_DL_FUNCTION_FAILED;
            }
        }

        auto ret = DlAclApi::RtSetIpcMemorySuperPodPid(deserializedInfos[localIdx].shmName, deserializedInfos[i].sdid,
                                                       &deserializedInfos[i].pid, 1);
        if (ret != 0) {
            BM_LOG_ERROR("enable white list for rank(" << deserializedInfos[i].rankId << ") failed: " << ret
                                                       << ", local rank = " << options_.rankId
                                                       << ", shmName=" << deserializedInfos[localIdx].shmName);
            return BM_DL_FUNCTION_FAILED;
        }
    }

    std::copy(deserializedInfos.begin(), deserializedInfos.end(), std::back_inserter(imports_));
    return BM_OK;
}

Result MemSegmentDevice::Mmap() noexcept
{
    if (imports_.empty()) {
        return BM_OK;
    }

    for (auto &im : imports_) {
        if (im.rankId == options_.rankId) {
            continue;
        }

        auto remoteAddress = globalVirtualAddress_ + options_.size * im.rankId + im.mappingOffset;
        if (mappedMem_.find((uint64_t)remoteAddress) != mappedMem_.end()) {
            BM_LOG_INFO("remote slice on rank(" << im.rankId << ") has maped: " << (void *)remoteAddress);
            continue;
        }

        if (!CanMapRemote(im)) {
            BM_LOG_INFO("remote slice on rank(" << im.rankId << ") SDMA cannot reaches.");
            continue;
        }

        BM_LOG_DEBUG("remote slice on rank(" << im.rankId << ") should map to: " << (void *)remoteAddress
                                             << ", size = " << im.size);
        auto ret = drv::HalGvaOpen((uint64_t)remoteAddress, im.shmName, im.size, 0);
        if (ret != BM_OK) {
            BM_LOG_ERROR("HalGvaOpen memory failed:" << ret);
            return -1;
        }
        mappedMem_.insert((uint64_t)remoteAddress);
    }
    imports_.clear();
    return BM_OK;
}

Result MemSegmentDevice::Unmap() noexcept
{
    for (auto va : mappedMem_) {
        (void)drv::HalGvaClose(va, 0);
    }
    mappedMem_.clear();

    // TODO: free local slice memory
    return 0;
}

Result MemSegmentDevice::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    for (auto &rank : ranks) {
        if (rank >= options_.rankCnt) {
            BM_LOG_ERROR("input rank is invalid! rank:" << rank << " rankSize:" << options_.rankCnt);
            return BM_INVALID_PARAM;
        }
    }

    for (auto &rank : ranks) {
        uint64_t addr = reinterpret_cast<uint64_t>(globalVirtualAddress_) + options_.size * rank;
        auto it = mappedMem_.lower_bound(addr);
        auto st = it;
        while (it != mappedMem_.end() && (*it) < addr + options_.size) {
            (void)drv::HalGvaClose((*it), 0);
            it++;
        }

        if (st != it) {
            mappedMem_.erase(st, it);
        }
    }
    return 0;
}

std::shared_ptr<MemSlice> MemSegmentDevice::GetMemSlice(hybm_mem_slice_t slice) const noexcept
{
    auto index = MemSlice::GetIndexFrom(slice);
    auto pos = slices_.find(index);
    if (pos == slices_.end()) {
        return nullptr;
    }

    auto target = pos->second.slice;
    if (!target->ValidateId(slice)) {
        return nullptr;
    }

    return target;
}

bool MemSegmentDevice::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    if (begin < globalVirtualAddress_) {
        return false;
    }

    if ((const uint8_t *)begin + size >= globalVirtualAddress_ + totalVirtualSize_) {
        return false;
    }

    return true;
}

void MemSegmentDevice::FreeMemory() noexcept
{
    auto ret = drv::HalGvaUnreserveMemory();
    BM_LOG_INFO("uninitialize GVA memory return: " << ret);
}

int MemSegmentDevice::GetDeviceId(int deviceId) noexcept
{
    if (deviceId < 0) {
        return BM_INVALID_PARAM;
    }

    if (deviceId_ >= 0) {
        if (deviceId == deviceId_) {
            return 0;
        }

        return BM_INVALID_PARAM;
    }

    uint32_t tgid = 0;
    auto ret = DlAclApi::RtDeviceGetBareTgid(&tgid);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get bare tgid failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }

    deviceId_ = deviceId;
    pid_ = static_cast<int>(tgid);
    ret = FillDeviceSuperPodInfo();
    if (ret != BM_OK) {
        BM_LOG_ERROR("FillDeviceSuperPodInfo() failed: " << ret);
        return ret;
    }

    return BM_OK;
}

int MemSegmentDevice::FillDeviceSuperPodInfo() noexcept
{
    int64_t value = 0;

    auto ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, INFO_TYPE_SDID, &value);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get sdid failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    sdid_ = static_cast<uint32_t>(value);

    ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, INFO_TYPE_SERVER_ID, &value);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get server id failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    serverId_ = static_cast<uint32_t>(value);
    BM_LOG_DEBUG("local server id=0x" << std::hex << serverId_);

    ret = DlAclApi::RtGetDeviceInfo(deviceId_, 0, INFO_TYPE_SUPER_POD_ID, &value);
    if (ret != BM_OK) {
        BM_LOG_ERROR("get super pod id failed: " << ret);
        return BM_DL_FUNCTION_FAILED;
    }
    superPodId_ = static_cast<uint32_t>(value);

    if (superPodId_ == invalidSuperPodId && serverId_ == invalidServerId) {
        auto networks = NetworkGetIpAddresses();
        if (networks.empty()) {
            BM_LOG_ERROR("get local host ip address empty.");
            return BM_ERROR;
        }

        serverId_ = networks[0];
    }

    BM_LOG_DEBUG("local sdid=0x" << std::hex << sdid_ << ", local server id=0x" << std::hex << serverId_
                                 << ", spid=" << superPodId_);

    return BM_OK;
}

bool MemSegmentDevice::CanMapRemote(const HbmExportInfo &rmi) noexcept
{
    if (rmi.serverId == serverId_) {
        BM_LOG_DEBUG("map from rank(" << rmi.rankId << ") on sample host, can map.");
        return true;
    }

    if (rmi.superPodId == invalidSuperPodId || superPodId_ == invalidSuperPodId) {
        BM_LOG_INFO("map from rank(" << rmi.rankId << ") spid: " << rmi.superPodId << ", local: " << superPodId_
                                     << " cannot map.");
        return false;
    }

    return rmi.superPodId == superPodId_;
}

uint32_t MemSegmentDevice::GetRankIdByAddr(const void *addr, uint64_t size) const noexcept
{
    if (!MemoryInRange(addr, size)) {
        return UINT32_MAX;
    }
    return (reinterpret_cast<uint64_t>(addr) - reinterpret_cast<uint64_t>(globalVirtualAddress_)) / options_.size;
}

bool MemSegmentDevice::CheckSmdaReaches(uint32_t rankId) const noexcept
{
    auto pos = importMap_.find(static_cast<uint16_t>(rankId));
    if (pos == importMap_.end()) {
        return false;
    }

    if (pos->second.serverId == serverId_) {
        return true;
    }

    if (pos->second.superPodId == invalidSuperPodId || superPodId_ == invalidSuperPodId) {
        return false;
    }

    return pos->second.superPodId == superPodId_;
}
}
}
