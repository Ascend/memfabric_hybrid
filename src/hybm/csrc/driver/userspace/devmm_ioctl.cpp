/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <cerrno>
#include <cstring>

#include "hybm_logger.h"
#include "hybm_cmd.h"
#include "devmm_ioctl.h"

namespace ock {
namespace mf {
namespace drv {

namespace {
constexpr auto DAVINCI_SVM_SUB_MODULE_NAME = "SVM";
constexpr auto DAVINCI_SVM_AGENT_SUB_MODULE_NAME = "SVM_AGENT";

constexpr char DAVINCI_INTF_IOC_MAGIC = 'Z';
constexpr auto DAVINCI_INTF_IOCTL_OPEN = _IO(DAVINCI_INTF_IOC_MAGIC, 0);
constexpr auto DAVINCI_INTF_IOCTL_CLOSE = _IO(DAVINCI_INTF_IOC_MAGIC, 1);
constexpr auto DAVINCI_INTF_IOCTL_STATUS = _IO(DAVINCI_INTF_IOC_MAGIC, 2);
constexpr char DEVMM_SVM_MAGIC = 'M';

#define DEVMM_SVM_IPC_MEM_OPEN _IOW(DEVMM_SVM_MAGIC, 21, DevmmCommandMessage)
#define DEVMM_SVM_TRANSLATE _IOWR(DEVMM_SVM_MAGIC, 17, DevmmMemTranslateParam)
#define DEVMM_SVM_ALLOC_PROC_STRUCT _IOW(DEVMM_SVM_MAGIC, 64, DevmmCommandMessage)
#define DEVMM_SVM_DEV_SET_SIBLING _IOW(DEVMM_SVM_MAGIC, 65, DevmmCommandMessage)
#define DEVMM_SVM_PREFETCH _IOW(DEVMM_SVM_MAGIC, 14, DevmmMemAdvisePara)
#define DEVMM_SVM_IPC_MEM_QUERY _IOWR(DEVMM_SVM_MAGIC, 29, DevmmMemQuerySizePara)
#define DEVMM_SVM_ALLOC _IOW(DEVMM_SVM_MAGIC, 3, DevmmCommandMessage)
#define DEVMM_SVM_ADVISE _IOW(DEVMM_SVM_MAGIC, 13, DevmmCommandMessage)
#define DEVMM_SVM_FREE_PAGES _IOW(DEVMM_SVM_MAGIC, 4, DevmmCommandMessage)
#define DEVMM_SVM_IPC_MEM_CLOSE _IOW(DEVMM_SVM_MAGIC, 22, DevmmCommandMessage)

struct OpenCloseArgs {
    char moduleName[DAVINIC_MODULE_NAME_MAX];
    int deviceId;
};

int gDeviceId = -1;
int gDeviceFd = -1;
}

void HybmInitialize(int deviceId, int fd) noexcept
{
    gDeviceId = deviceId;
    gDeviceFd = fd;
}

int HybmMapShareMemory(const char *name, void *expectAddr, uint64_t size, uint64_t flags) noexcept
{
    if (gDeviceId == -1 || gDeviceFd == -1) {
        BM_LOG_ERROR("deviceId or fd not set! id:" << gDeviceId << " fd:" << gDeviceFd);
        return -1;
    }

    DevmmCommandMessage arg{};
    arg.head.devId = static_cast<uint32_t>(gDeviceId);
    memcpy(arg.data.queryParam.name, name, DEVMM_MAX_NAME_SIZE);

    auto ret = ioctl(gDeviceFd, DEVMM_SVM_IPC_MEM_QUERY, &arg);
    if (ret != 0) {
        BM_LOG_ERROR("query for name: (" << name << ") failed = " << ret);
        return -1;
    }

    BM_LOG_INFO("shm(" << name <<") size=" << arg.data.queryParam.len << ", isHuge=" << arg.data.queryParam.isHuge);

    memset(&arg.data, 0, sizeof(arg.data));
    arg.data.openParam.vptr = (uint64_t)(ptrdiff_t)expectAddr;
    BM_LOG_DEBUG("before map share memory: " << name);

    strcpy(arg.data.openParam.name, name);
    ret = ioctl(gDeviceFd, DEVMM_SVM_IPC_MEM_OPEN, &arg);
    if (ret != 0) {
        BM_LOG_ERROR("open share memory failed:" << ret << " : " << errno << " : " << strerror(errno)
                                                 << ", name = " << arg.data.openParam.name);
        return -1;
    }

    memset(&arg.data, 0, sizeof(arg.data));
    arg.data.prefetchParam.ptr = (uint64_t)(ptrdiff_t)expectAddr;
    arg.data.prefetchParam.count = size;
    ret = ioctl(gDeviceFd, DEVMM_SVM_PREFETCH, &arg);
    if (ret != 0) {
        BM_LOG_ERROR("prefetch share memory failed:" << ret << " : " << errno << " : " << strerror(errno)
                                                     << ", name = " << arg.data.openParam.name);
        return -1;
    }

    return 0;
}

int HybmUnmapShareMemory(void *expectAddr, uint64_t flags) noexcept
{
    DevmmCommandMessage arg{};
    int32_t ret;

    arg.data.freePagesPara.va = reinterpret_cast<uint64_t>(expectAddr);
    ret = ioctl(gDeviceFd, DEVMM_SVM_IPC_MEM_CLOSE, &arg);
    if (ret != 0) {
        BM_LOG_ERROR("gva close error.\n");
        return ret;
    }
    return 0;
}

int HybmIoctlAllocAnddAdvice(uint64_t ptr, size_t size, uint32_t devid, uint32_t advise) noexcept
{
    DevmmCommandMessage arg{};
    int32_t ret;

    arg.data.allocSvmPara.p = ptr;
    arg.data.allocSvmPara.size = size;

    ret = ioctl(gDeviceFd, DEVMM_SVM_ALLOC, &arg);
    if (ret != 0) {
        BM_LOG_ERROR("svm alloc failed:" << ret << " : " << errno << " : " << strerror(errno));
        return -1;
    }

    arg.head.devId = devid;
    arg.data.advisePara.ptr = ptr;
    arg.data.advisePara.count = size;
    arg.data.advisePara.advise = advise;

    ret = ioctl(gDeviceFd, DEVMM_SVM_ADVISE, &arg);
    if (ret != 0) {
        BM_LOG_ERROR("svm advise failed:" << ret << " : " << errno << " : " << strerror(errno));

        arg.data.freePagesPara.va = ptr;
        (void)ioctl(gDeviceFd, DEVMM_SVM_FREE_PAGES, &arg);
        return -1;
    }

    return 0;
}

int HybmTranslateAddress(uint64_t va, uint64_t &pa, uint32_t &da) noexcept
{
    if (gDeviceId == -1 || gDeviceFd == -1) {
        BM_LOG_ERROR("deviceId or fd not set! id:" << gDeviceId << " fd:" << gDeviceFd);
        return -1;
    }

    DevmmCommandMessage arg{};
    arg.head.devId = static_cast<uint32_t>(gDeviceId);
    arg.data.translateParam.vptr = va;

    auto ret = ioctl(gDeviceFd, DEVMM_SVM_TRANSLATE, &arg);
    if (ret != 0) {
        BM_LOG_ERROR("translate memory failed:" << ret << " : " << errno << " : " << strerror(errno)
                                                << ", va = " << (void *)(ptrdiff_t)va);
        return -1;
    }

    pa = arg.data.translateParam.pptr;
    da = arg.data.translateParam.addrInDevice;
    BM_LOG_INFO("translate va:" << (void *)(ptrdiff_t)va << ", pa = " << (void *)(ptrdiff_t)pa << ", da=" << da);

    return 0;
}
}
}
}