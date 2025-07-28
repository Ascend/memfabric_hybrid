#!/usr/bin/env python
# coding=utf-8
# Copyright: (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
import os
import sys
import ctypes


current_path = os.path.abspath(__file__)
current_dir = os.path.dirname(current_path)
sys.path.append(current_dir)
libs_path = os.path.join(current_dir, 'lib')
for lib in ["libmf_hybm_core.so", "libmf_smem.so"]:
    ctypes.CDLL(os.path.join(libs_path, lib))


from _pymf_smem import (
    bm,
    shm,
    initialize,
    uninitialize,
    set_log_level,
    set_ssl_option,
    TlsOption,
    set_extern_logger,
    get_last_err_msg,
    get_and_clear_last_err_msg
)

__all__ = [
    'bm',
    'shm',
    'initialize',
    'uninitialize',
    'set_log_level',
    'set_ssl_option',
    'TlsOption',
    'set_extern_logger',
    'get_last_err_msg',
    'get_and_clear_last_err_msg'
]
