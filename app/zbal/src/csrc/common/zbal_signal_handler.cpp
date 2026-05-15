/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ZBAL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef ZBAL_SIGNAL_HANDLER_H
#define ZBAL_SIGNAL_HANDLER_H

#include <csignal>
#include "zbal_common_includes.h"
#include "zbal_communicator.h"

namespace zbal {

void signal_handler(int signal)
{
    if (signal != SIGUSR1) {
        return;
    }

    // dump all trace
    zbal::operators::Communicator::DumpAllComm();
}

} // namespace zbal

#endif // ZBAL_SIGNAL_HANDLER_H
