/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <algorithm>
#include <cerrno>
#include <cstring>

#include "hybm_logger.h"
#include "hybm_cmd.h"
#include "devmm_ioctl.h"

namespace ock {
namespace mf {
namespace drv {

namespace {
const char DEVMM_SVM_MAGIC = 'M';
#define DEVMM_SVM_IPC_MEM_OPEN _IOW(DEVMM_SVM_MAGIC, 21, DevmmCommandMessage)
#define DEVMM_SVM_PREFETCH _IOW(DEVMM_SVM_MAGIC, 14, DevmmMemAdvisePara)
#define DEVMM_SVM_IPC_MEM_QUERY _IOWR(DEVMM_SVM_MAGIC, 29, DevmmMemQuerySizePara)
#define DEVMM_SVM_ALLOC _IOW(DEVMM_SVM_MAGIC, 3, DevmmCommandMessage)
#define DEVMM_SVM_ADVISE _IOW(DEVMM_SVM_MAGIC, 13, DevmmCommandMessage)
#define DEVMM_SVM_FREE_PAGES _IOW(DEVMM_SVM_MAGIC, 4, DevmmCommandMessage)
#define DEVMM_SVM_IPC_MEM_CLOSE _IOW(DEVMM_SVM_MAGIC, 22, DevmmCommandMessage)

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
    if (name == nullptr) {
        BM_LOG_ERROR("name is null");
        return -1;
    }
    if (strlen(name) > DEVMM_MAX_NAME_SIZE) {
        BM_LOG_ERROR("name is too long:" << strlen(name) << ", max is " << DEVMM_MAX_NAME_SIZE);
        return -1;
    }
    std::copy_n(name, strlen(name), arg.data.queryParam.name);

    auto ret = ioctl(gDeviceFd, DEVMM_SVM_IPC_MEM_QUERY, &arg);
    if (ret != 0) {
        BM_LOG_ERROR("query for name: (" << name << ") failed = " << ret);
        return -1;
    }

    BM_LOG_INFO("shm(" << name <<") size=" << arg.data.queryParam.len << ", isHuge=" << arg.data.queryParam.isHuge);

    std::fill_n(reinterpret_cast<char *>(&arg.data), sizeof(arg.data), 0);
    arg.data.openParam.vptr = reinterpret_cast<uint64_t>(expectAddr);
    BM_LOG_DEBUG("before map share memory: " << name);

    std::copy_n(name, strlen(name), arg.data.openParam.name);
    ret = ioctl(gDeviceFd, DEVMM_SVM_IPC_MEM_OPEN, &arg);
    if (ret != 0) {
        BM_LOG_ERROR("open share memory failed:" << ret << " : " << errno << " : " << strerror(errno)
                                                 << ", name = " << arg.data.openParam.name);
        return -1;
    }

    std::fill_n(reinterpret_cast<char *>(&arg.data), sizeof(arg.data), 0);
    arg.data.prefetchParam.ptr = reinterpret_cast<uint64_t>(expectAddr);
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

}
}
}