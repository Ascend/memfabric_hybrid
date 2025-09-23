#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
import os
import sys
import ctypes

current_path = os.path.abspath(__file__)
current_dir = os.path.dirname(current_path)
sys.path.append(current_dir)

for lib in ["libmf_hybm_core.so", "libmf_smem.so"]:
    ctypes.CDLL(lib)


from _pymf_smem import (
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
