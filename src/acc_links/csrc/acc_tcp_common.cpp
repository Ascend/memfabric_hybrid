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
} // namespace acc
} // namespace ock