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
import sys
import ctypes

current_path = os.path.abspath(__file__)
current_dir = os.path.dirname(current_path)
sys.path.append(current_dir)
libs_path = os.path.join(current_dir, 'lib')
for lib in ["libmf_hybm_core.so", "libmf_smem.so"]:
    ctypes.CDLL(os.path.join(libs_path, lib))

from _pymf_transfer import (
    TransferEngine,
    TransferOpcode,
    create_config_store
)
from _pymf_hybrid import (
    bm,
    shm,
    initialize,
    uninitialize,
    set_log_level,
    set_extern_logger,
    get_last_err_msg,
    set_conf_store_tls,
    set_conf_store_tls_key,
    get_and_clear_last_err_msg
)

__all__ = [
    'TransferEngine',
    'TransferOpcode',
    'create_config_store',
    'bm',
    'shm',
    'initialize',
    'uninitialize',
    'set_log_level',
    'set_extern_logger',
    'get_last_err_msg',
    'set_conf_store_tls',
    'set_conf_store_tls_key',
    'get_and_clear_last_err_msg'
]
