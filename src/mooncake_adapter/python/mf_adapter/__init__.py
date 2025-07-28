#!/usr/bin/env python
# coding=utf-8
# Copyright: (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
import os
import sys
import ctypes


current_path = os.path.abspath(__file__)
current_dir = os.path.dirname(current_path)
sys.path.append(current_dir)
libs_path = os.path.join(current_dir, 'lib')
for lib in ["libmf_hybm_core.so", "libmf_smem.so"]:
    ctypes.CDLL(os.path.join(libs_path, lib))


from _pymf_transfer import TransferEngine, set_log_level, create_config_store


__all__ = ['TransferEngine', 'set_log_level', 'create_config_store']
