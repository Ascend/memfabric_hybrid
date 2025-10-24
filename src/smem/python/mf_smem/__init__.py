#!/usr/bin/env python
# coding=utf-8
# Copyright: (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.

import os
import sys
import ctypes


current_path = os.path.abspath(__file__)
current_dir = os.path.dirname(current_path)
sys.path.append(current_dir)

lib_dir = os.path.join(current_dir, 'lib')
for lib in ["libhybm_gvm.so", "libmf_hybm_core.so", "libmf_smem.so"]:
    ctypes.CDLL(os.path.join(lib_dir, lib))


from _pymf_smem import (
    bm,
    initialize,
    uninitialize,
    set_log_level,
    set_extern_logger,
    get_last_err_msg,
    get_and_clear_last_err_msg
)


__all__ = [
    'bm',
    'initialize',
    'uninitialize',
    'set_log_level',
    'set_extern_logger',
    'get_last_err_msg',
    'get_and_clear_last_err_msg'
]
