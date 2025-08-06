/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "acc_tcp_common.h"

namespace ock {
namespace acc {

void SafeCloseFd(int &fd, bool needShutdown)
{
    if (UNLIKELY(fd < 0)) {
        return;
    }

    auto tmpFd = fd;
    if (__sync_bool_compare_and_swap(&fd, tmpFd, -1)) {
        if (needShutdown) {
            shutdown(tmpFd, SHUT_RDWR);
        }
        close(tmpFd);
    }
}
}  // namespace acc
}  // namespace ock