#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os
import signal
import sys
from _pymf_transfer import create_config_store, set_log_level


def launch_ascend_mf_store():
    log_level = os.getenv("ASCEND_MF_LOG_LEVEL")
    if log_level and log_level.isdigit():
        set_log_level(int(log_level))
    ascend_mf_store_url = os.getenv("ASCEND_MF_STORE_URL")
    if not ascend_mf_store_url:
        raise ValueError("env ASCEND_MF_STORE_URL not exist")
    ret = create_config_store(ascend_mf_store_url)
    if ret != 0:
        sys.exit(ret)


if __name__ == "__main__":
    def handle_exit(signum, frame):
        sys.exit(0)


    signal.signal(signal.SIGINT, handle_exit)
    signal.signal(signal.SIGTERM, handle_exit)

    launch_ascend_mf_store()
    while True:
        signal.pause()
