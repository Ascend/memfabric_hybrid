#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.

import os
import sys
import ctypes

current_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(current_dir)

lib_dir = os.path.join(current_dir, 'lib')
lib_list = ['libhybm_gvm.so', 'libmf_hybm_core.so', 'libmf_smem.so', 'libmf_memcache.so']
for lib_source in lib_list:
    ctypes.CDLL(os.path.join(lib_dir, lib_source))

from _pymmc import SliceBuffer, DistributedObjectStore, KeyInfo

__all__ = ['SliceBuffer', 'DistributedObjectStore', 'KeyInfo']
