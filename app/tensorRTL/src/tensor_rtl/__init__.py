#!/usr/bin/python3.10
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

from tensor_rtl.core.ptensor import PTensor, PTensorSet
from tensor_rtl.core.executor import BatchP2PExecutor, All2AllVExcutor, MemoryFabricExecutor
from tensor_rtl.utils.dtype_converter import pack_tensors, unpack_tensors
try:
    import memfabric_hybrid
    from tensor_rtl.core.backend_config import MfBmConfig
except ImportError:
    MfBmConfig = None
