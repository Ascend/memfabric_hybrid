
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

import torch


def get_dtype_size(dtype: torch.dtype):
    if dtype.is_floating_point:
        info = torch.finfo(dtype)
    else:
        info = torch.iinfo(dtype)
    return info.bits // 8

