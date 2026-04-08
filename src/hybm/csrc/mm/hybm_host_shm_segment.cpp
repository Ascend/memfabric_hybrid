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
#include "hybm_host_shm_segment.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

#include "hybm_ex_info_transfer.h"
#include "hybm_logger.h"

namespace ock {
namespace mf {

uint64_t HybmHostShmSegment::sUsedOffset_ = 0U;

namespace {
constexpr const char *HOST_SHM_HUGEPAGE_DIR = "/dev/hugepages";
constexpr const char *HOST_SHM_FALLBACK_DIR = "/dev/shm";
constexpr unsigned long HOST_SHM_HUGETLBFS_MAGIC = 0x958458f6UL;
constexpr uint32_t HOST_SHM_IMPORT_OPEN_RETRY_TIMES = 50U;
constexpr uint32_t HOST_SHM_IMPORT_OPEN_RETRY_INTERVAL_US = 10000U;

bool IsHugetlbfsMounted() noexcept
{
    struct statfs fsInfo {};
    if (statfs(HOST_SHM_HUGEPAGE_DIR, &fsInfo) != 0) {
        BM_LOG_WARN("Failed to access hugepage dir " << HOST_SHM_HUGEPAGE_DIR << " error:" << errno << ", "
                                                     << SafeStrError(errno) << ", will fallback to /dev/shm");
        return false;
    }
    if (static_cast<unsigned long>(fsInfo.f_type) != HOST_SHM_HUGETLBFS_MAGIC) {
        BM_LOG_WARN("Hugepage dir " << HOST_SHM_HUGEPAGE_DIR << " is not hugetlbfs, fs type:" << std::hex
                                    << static_cast<unsigned long>(fsInfo.f_type) << ", will fallback to /dev/shm");
        return false;
    }
    return true;
}

bool HasAvailableHugePages() noexcept
{
    std::ifstream memInfoFile("/proc/meminfo");
    if (!memInfoFile.is_open()) {
        BM_LOG_WARN("Failed to open /proc/meminfo, will fallback to /dev/shm");
        return false;
    }

    std::string line;
    while (std::getline(memInfoFile, line)) {
        std::istringstream lineStream(line);
        std::string key;
        uint64_t value = 0;

        if (!(lineStream >> key >> value)) {
            continue;
        }

        if (key != "HugePages_Free:") {
            continue;
        }

        if (value == 0) {
            BM_LOG_WARN("HugePages_Free is 0, will fallback to /dev/shm");
            return false;
        }
        return true;
    }

    BM_LOG_WARN("Failed to parse HugePages_Free from /proc/meminfo, will fallback to /dev/shm");
    return false;
}
} // namespace

HybmHostShmSegment::~HybmHostShmSegment()
{
    FreeMemory();
}

Result HybmHostShmSegment::ValidateOptions() noexcept
{
    if (options_.segType != HYBM_MST_DRAM || options_.size == 0 || (options_.size % HYBM_LARGE_PAGE_SIZE) != 0) {
        BM_LOG_ERROR("Validate options error type(" << options_.segType << ") size(" << options_.size);
        return BM_INVALID_PARAM;
    }
    if (UINT64_MAX / options_.size < options_.rankCnt) {
        BM_LOG_ERROR("Validate options error rankCnt(" << options_.rankCnt << ") size(" << options_.size);
        return BM_INVALID_PARAM;
    }
    return BM_OK;
}

Result HybmHostShmSegment::ReserveMemorySpace(void **address) noexcept
{
    BM_ASSERT_LOG_AND_RETURN(ValidateOptions() == BM_OK, "Failed to validate options.", BM_INVALID_PARAM);
    BM_ASSERT_LOG_AND_RETURN(globalVirtualAddress_ == nullptr, "Already prepare virtual memory.", BM_NOT_INITIALIZED);
    BM_ASSERT_LOG_AND_RETURN(address != nullptr, "Invalid param, address is NULL.", BM_INVALID_PARAM);
    void *startAddr = reinterpret_cast<void *>(HYBM_HOST_CONN_START_ADDR + sUsedOffset_);
    uint64_t totalSize = options_.rankCnt * options_.size;
    if (totalSize + sUsedOffset_ > HYBM_HOST_CONN_ADDR_SIZE) {
        BM_LOG_ERROR("Failed to reserve size:" << totalSize << " used:" << sUsedOffset_
                                               << " total:" << HYBM_HOST_CONN_ADDR_SIZE);
        return BM_INVALID_PARAM;
    }
    void *mapped =
        mmap(startAddr, totalSize, PROT_NONE, MAP_FIXED_NOREPLACE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE, -1, 0);
    if (mapped == MAP_FAILED || (uint64_t)mapped != (uint64_t)startAddr) {
        BM_LOG_ERROR("Failed to mmap size:" << totalSize << " addr:" << startAddr << " ret:" << mapped
                                            << " error: " << errno);
        return BM_ERROR;
    }
    globalVirtualAddress_ = (uint8_t *)startAddr;
    totalVirtualSize_ = totalSize;
    localVirtualBase_ = globalVirtualAddress_ + options_.size * options_.rankId;
    allocatedSize_ = 0UL;
    sliceCount_ = 0;
    auto ret = MapLocalShm();
    if (ret != BM_OK) {
        munmap(globalVirtualAddress_, totalVirtualSize_);
        globalVirtualAddress_ = nullptr;
        totalVirtualSize_ = 0;
        localVirtualBase_ = nullptr;
        return ret;
    }
    *address = globalVirtualAddress_;
    sUsedOffset_ += totalSize;
    return BM_OK;
}

Result HybmHostShmSegment::UnReserveMemorySpace() noexcept
{
    BM_LOG_INFO("un-reserve memory space, reclaiming " << totalVirtualSize_ << " bytes.");
    if (totalVirtualSize_ > 0 && sUsedOffset_ >= totalVirtualSize_) {
        sUsedOffset_ -= totalVirtualSize_;
    }
    FreeMemory();
    return BM_OK;
}

Result HybmHostShmSegment::AllocLocalMemory(uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    if ((size % HYBM_LARGE_PAGE_SIZE) != 0UL || size + allocatedSize_ > options_.size) {
        BM_LOG_ERROR("invalid allocate memory size : " << size << ", now used " << allocatedSize_ << " of "
                                                       << options_.size);
        return BM_INVALID_PARAM;
    }
    void *sliceAddr = localVirtualBase_ + allocatedSize_;
    allocatedSize_ += size;
    slice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_HOST_DRAM, MEM_PT_TYPE_SVM,
                                       reinterpret_cast<uint64_t>(sliceAddr), size);
    slices_.emplace(slice->index_, slice);
    BM_LOG_INFO("allocate slice(idx:" << slice->index_ << ", size:" << slice->size_ << " va:" << sliceAddr << ").");
    return BM_OK;
}

Result HybmHostShmSegment::Export(std::string &exInfo) noexcept
{
    return BM_OK;
}

Result HybmHostShmSegment::Export(const std::shared_ptr<MemSlice> &slice, std::string &exInfo) noexcept
{
    if (slice == nullptr) {
        return BM_INVALID_PARAM;
    }
    auto pos = slices_.find(slice->index_);
    if (pos == slices_.end() || pos->second.slice != slice) {
        return BM_INVALID_PARAM;
    }
    auto exp = exportMap_.find(slice->index_);
    if (exp != exportMap_.end()) {
        exInfo = exp->second;
        return BM_OK;
    }
    ShmExportInfo info;
    info.mappingOffset = slice->vAddress_ - (uint64_t)(localVirtualBase_);
    info.sliceIndex = static_cast<uint32_t>(slice->index_);
    info.rankId = options_.rankId;
    info.size = slice->size_;
    info.pageTblType = MEM_PT_TYPE_SVM;
    info.memSegType = HYBM_MST_DRAM;
    info.exchangeType = HYBM_INFO_EXG_IN_NODE;
    info.useHugetlbfs = useHugetlbfs_;
    auto ret = LiteralExInfoTranslater<ShmExportInfo>{}.Serialize(info, exInfo);
    if (ret != BM_OK) {
        return BM_ERROR;
    }
    exportMap_[slice->index_] = exInfo;
    return BM_OK;
}

Result HybmHostShmSegment::Import(const std::vector<std::string> &allExInfo, void *addresses[]) noexcept
{
    LiteralExInfoTranslater<ShmExportInfo> translator;
    std::vector<ShmExportInfo> deserializedInfos{allExInfo.size()};
    for (auto i = 0U; i < allExInfo.size(); i++) {
        if (translator.Deserialize(allExInfo[i], deserializedInfos[i]) != 0) {
            return BM_INVALID_PARAM;
        }
    }
    uint32_t localIdx = UINT32_MAX;
    std::unordered_set<uint32_t> rankIdSet;
    std::vector<bool> uniqueRankFlags(deserializedInfos.size(), false);
    for (auto i = 0U; i < deserializedInfos.size(); i++) {
        if (deserializedInfos[i].magic != DRAM_SLICE_EXPORT_INFO_MAGIC) {
            return BM_INVALID_PARAM;
        }
        if (!rankIdSet.insert(deserializedInfos[i].rankId).second) {
            BM_LOG_WARN("Duplicate rankId in import: " << deserializedInfos[i].rankId);
            continue;
        }
        uniqueRankFlags[i] = true;
        if (deserializedInfos[i].rankId == options_.rankId) {
            localIdx = i;
        }
    }
    BM_ASSERT_RETURN(localIdx < deserializedInfos.size(), BM_INVALID_PARAM);
    try {
        for (auto i = 0U; i < deserializedInfos.size(); ++i) {
            if (uniqueRankFlags[i]) {
                imports_.push_back(deserializedInfos[i]);
                importedHugetlbfsFlags_[deserializedInfos[i].rankId] = deserializedInfos[i].useHugetlbfs;
            }
        }
    } catch (...) {
        return BM_MALLOC_FAILED;
    }
    return BM_OK;
}

Result HybmHostShmSegment::Mmap() noexcept
{
    for (const auto &im : imports_) {
        if (im.rankId == options_.rankId) {
            continue;
        }
        auto ret = MapImportedShm(im.rankId);
        if (ret != BM_OK) {
            return ret;
        }
    }
    imports_.clear();
    return BM_OK;
}

Result HybmHostShmSegment::Unmap() noexcept
{
    auto ret = BM_OK;
    for (auto rankId : mappedRemoteRanks_) {
        if (RemapRemoteAsReserved(rankId) != BM_OK) {
            ret = BM_ERROR;
        }
    }
    CloseImportedShmFds();
    mappedRemoteRanks_.clear();
    return ret;
}

std::shared_ptr<MemSlice> HybmHostShmSegment::GetMemSlice(hybm_mem_slice_t slice) const noexcept
{
    auto index = MemSlice::GetIndexFrom(slice);
    auto pos = slices_.find(index);
    if (pos == slices_.end() || !pos->second.slice->ValidateId(slice)) {
        return nullptr;
    }
    return pos->second.slice;
}

bool HybmHostShmSegment::MemoryInRange(const void *begin, uint64_t size) const noexcept
{
    return !(begin < globalVirtualAddress_ ||
             reinterpret_cast<const uint8_t *>(begin) + size > globalVirtualAddress_ + totalVirtualSize_);
}

bool HybmHostShmSegment::GetRankIdByAddr(const void *addr, uint64_t size, uint32_t &rankId) const noexcept
{
    if (!MemoryInRange(addr, size)) {
        rankId = options_.rankId;
        return false;
    }
    rankId = (reinterpret_cast<uint64_t>(addr) - reinterpret_cast<uint64_t>(globalVirtualAddress_)) / options_.size;
    return true;
}

void HybmHostShmSegment::FreeMemory() noexcept
{
    (void)Unmap();
    if (localShmFd_ >= 0) {
        close(localShmFd_);
        localShmFd_ = -1;
        auto shmPath = GetShmFilePath(options_.rankId);
        if (unlink(shmPath.c_str()) != 0) {
            if (errno != ENOENT) {
                BM_LOG_ERROR("Failed to unlink local shm file " << shmPath << " error:" << errno << " "
                                                                << SafeStrError(errno));
                if (errno == EBUSY || errno == EACCES) {
                    BM_LOG_WARN("File may remain on filesystem, consider manual cleanup");
                }
            }
        }
    }
    localVirtualBase_ = nullptr;
    if (globalVirtualAddress_ != nullptr) {
        (void)munmap(globalVirtualAddress_, totalVirtualSize_);
        globalVirtualAddress_ = nullptr;
    }
}

Result HybmHostShmSegment::RemoveImported(const std::vector<uint32_t> &ranks) noexcept
{
    auto ret = BM_OK;
    for (auto rankId : ranks) {
        if (mappedRemoteRanks_.count(rankId) == 0) {
            continue;
        }
        if (RemapRemoteAsReserved(rankId) != BM_OK) {
            ret = BM_ERROR;
            continue;
        }
        auto fdPos = importedShmFds_.find(rankId);
        if (fdPos != importedShmFds_.end()) {
            close(fdPos->second);
            importedShmFds_.erase(fdPos);
        }
        mappedRemoteRanks_.erase(rankId);
    }
    return ret;
}

Result HybmHostShmSegment::RegisterMemory(const void *addr, uint64_t size, std::shared_ptr<MemSlice> &slice) noexcept
{
    slice = std::make_shared<MemSlice>(sliceCount_++, MEM_TYPE_HOST_DRAM, MEM_PT_TYPE_SVM,
                                       reinterpret_cast<uint64_t>(addr), size);
    slices_.emplace(slice->index_, slice);
    return BM_OK;
}

Result HybmHostShmSegment::ReleaseSliceMemory(const std::shared_ptr<MemSlice> &slice) noexcept
{
    if (slice == nullptr) {
        return BM_INVALID_PARAM;
    }
    auto pos = slices_.find(slice->index_);
    if (pos == slices_.end() || pos->second.slice != slice) {
        return BM_INVALID_PARAM;
    }
    slices_.erase(pos);
    return BM_OK;
}

Result HybmHostShmSegment::GetExportSliceSize(size_t &size) noexcept
{
    size = sizeof(ShmExportInfo);
    return BM_OK;
}

bool HybmHostShmSegment::TryHugetlbfsAvailable() noexcept
{
    return IsHugetlbfsMounted() && HasAvailableHugePages();
}

std::string HybmHostShmSegment::GetShmFilePath(uint32_t rankId) const noexcept
{
    return GetShmFilePath(rankId, useHugetlbfs_);
}

std::string HybmHostShmSegment::GetShmFilePath(uint32_t rankId, bool useHugetlbfs) const noexcept
{
    const char *baseDir = useHugetlbfs ? HOST_SHM_HUGEPAGE_DIR : HOST_SHM_FALLBACK_DIR;
    return std::string(baseDir) + "/memfabric_hybrid_" + std::to_string(rankId);
}

Result HybmHostShmSegment::MapLocalShm() noexcept
{
    useHugetlbfs_ = TryHugetlbfsAvailable();
    auto shmPath = GetShmFilePath(options_.rankId);
    BM_LOG_INFO("MapLocalShm start: rankId=" << options_.rankId << " size=" << options_.size
                                             << " useHugetlbfs=" << useHugetlbfs_ << " shmPath=" << shmPath);
    auto openAndLockLocalShm = [&shmPath]() noexcept -> int {
        int shmFd = open(shmPath.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
        if (shmFd < 0) {
            return -1;
        }
        if (flock(shmFd, LOCK_EX | LOCK_NB) != 0) {
            auto lockErr = errno;
            close(shmFd);
            errno = lockErr;
            return -1;
        }
        return shmFd;
    };

    localShmFd_ = openAndLockLocalShm();
    if (localShmFd_ < 0 && errno == EEXIST) {
        BM_LOG_INFO("Local shm file already exists, trying cleanup: " << shmPath);
        int staleShmFd = open(shmPath.c_str(), O_RDWR);
        if (staleShmFd < 0) {
            BM_LOG_ERROR("Failed to open existing shm file " << shmPath << " error:" << errno << " "
                                                             << SafeStrError(errno));
            return BM_ERROR;
        }
        if (flock(staleShmFd, LOCK_EX | LOCK_NB) != 0) {
            auto lockErr = errno;
            close(staleShmFd);
            BM_LOG_ERROR("Local shm file is already in use " << shmPath << " error:" << lockErr << " "
                                                             << SafeStrError(lockErr));
            return BM_ERROR;
        }
        if (unlink(shmPath.c_str()) != 0) {
            auto unlinkErr = errno;
            close(staleShmFd);
            BM_LOG_ERROR("Failed to cleanup stale shm file " << shmPath << " error:" << unlinkErr << " "
                                                             << SafeStrError(unlinkErr));
            return BM_ERROR;
        }
        close(staleShmFd);
        localShmFd_ = openAndLockLocalShm();
    }
    if (localShmFd_ < 0) {
        BM_LOG_ERROR("Failed to open local shm file " << shmPath << " size:" << options_.size << " error:" << errno
                                                      << " " << SafeStrError(errno));
        return BM_ERROR;
    }
    BM_LOG_INFO("Local shm file opened and locked: " << shmPath << " fd=" << localShmFd_);
    if (ftruncate(localShmFd_, static_cast<off_t>(options_.size)) != 0) {
        BM_LOG_ERROR("Failed to truncate local shm file " << shmPath << " size:" << options_.size << " error:" << errno
                                                          << " " << SafeStrError(errno));
        close(localShmFd_);
        localShmFd_ = -1;
        (void)unlink(shmPath.c_str());
        return BM_ERROR;
    }
    BM_LOG_INFO("Local shm file truncated: " << shmPath << " size=" << options_.size);
    void *mapped = mmap(localVirtualBase_, options_.size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED | MAP_POPULATE,
                        localShmFd_, 0);
    if (mapped == MAP_FAILED || mapped != localVirtualBase_) {
        BM_LOG_ERROR("Failed to mmap local shm file " << shmPath << " addr:" << localVirtualBase_
                                                      << " size:" << options_.size << " ret:" << mapped
                                                      << " error:" << errno << " " << SafeStrError(errno));
        close(localShmFd_);
        localShmFd_ = -1;
        (void)unlink(shmPath.c_str());
        return BM_ERROR;
    }
    BM_LOG_INFO("MapLocalShm success: rankId=" << options_.rankId << " addr=" << localVirtualBase_
                                               << " size=" << options_.size << " useHugetlbfs=" << useHugetlbfs_
                                               << " fd=" << localShmFd_);
    return BM_OK;
}

Result HybmHostShmSegment::MapImportedShm(uint32_t rankId) noexcept
{
    if (mappedRemoteRanks_.count(rankId) > 0) {
        return BM_OK;
    }
    bool remoteUseHugetlbfs = useHugetlbfs_;
    auto flagIt = importedHugetlbfsFlags_.find(rankId);
    if (flagIt != importedHugetlbfsFlags_.end()) {
        remoteUseHugetlbfs = flagIt->second;
    }
    auto shmPath = GetShmFilePath(rankId, remoteUseHugetlbfs);
    int fd = -1;
    constexpr uint32_t extendedRetryTimes = 100U;
    for (uint32_t attempt = 0U; attempt < extendedRetryTimes; ++attempt) {
        fd = open(shmPath.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
        if (fd >= 0) {
            break;
        }
        if (errno != ENOENT) {
            BM_LOG_ERROR("Failed to open imported shm file " << shmPath << " rank:" << rankId << " error:" << errno
                                                             << " " << SafeStrError(errno));
            return BM_ERROR;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(HOST_SHM_IMPORT_OPEN_RETRY_INTERVAL_US));
    }
    if (fd < 0) {
        BM_LOG_ERROR("Imported shm file not ready after retry "
                     << shmPath << " rank:" << rankId << " retries:" << extendedRetryTimes
                     << " interval(us):" << HOST_SHM_IMPORT_OPEN_RETRY_INTERVAL_US << " last error:" << errno << " "
                     << SafeStrError(errno));
        return BM_ERROR;
    }
    struct stat fileStat {};
    if (fstat(fd, &fileStat) != 0 || static_cast<uint64_t>(fileStat.st_size) != options_.size) {
        BM_LOG_ERROR("Imported shm file size mismatch "
                     << shmPath << " rank:" << rankId << " expected:" << options_.size << " got:" << fileStat.st_size);
        close(fd);
        return BM_ERROR;
    }
    auto *remoteBase = globalVirtualAddress_ + options_.size * rankId;
    void *mapped = mmap(remoteBase, options_.size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED || mapped != remoteBase) {
        BM_LOG_ERROR("Failed to mmap imported shm file " << shmPath << " rank:" << rankId << " addr:" << remoteBase
                                                         << " size:" << options_.size << " ret:" << mapped
                                                         << " error:" << errno << " " << SafeStrError(errno));
        close(fd);
        return BM_ERROR;
    }
    importedShmFds_[rankId] = fd;
    mappedRemoteRanks_.insert(rankId);
    return BM_OK;
}

Result HybmHostShmSegment::RemapRemoteAsReserved(uint32_t rankId) noexcept
{
    auto *remoteBase = globalVirtualAddress_ + options_.size * rankId;
    void *reserved =
        mmap(remoteBase, options_.size, PROT_NONE, MAP_FIXED | MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE, -1, 0);
    if (reserved == MAP_FAILED || reserved != remoteBase) {
        BM_LOG_ERROR("Failed to remap remote rank as reserved rank:"
                     << rankId << " addr:" << remoteBase << " size:" << options_.size << " ret:" << reserved
                     << " error:" << errno << " " << SafeStrError(errno));
        return BM_ERROR;
    }
    return BM_OK;
}

void HybmHostShmSegment::CloseImportedShmFds() noexcept
{
    for (auto &item : importedShmFds_) {
        close(item.second);
    }
    importedShmFds_.clear();
}

} // namespace mf
} // namespace ock
