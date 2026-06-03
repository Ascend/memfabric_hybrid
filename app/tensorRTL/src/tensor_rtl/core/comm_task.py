
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

from dataclasses import dataclass
from typing import List
import torch
from tensor_rtl.api.abc import BaseCommTask


@dataclass
class P2PCommTask(BaseCommTask):
    """Definition of a single P2P operation"""
    idx: int | List[int] = None
    numel: int = None
    is_send: bool | List[bool] = None
    src_rank: int | List[int] = None
    dst_rank: int | List[int] = None
    buffer: torch.Tensor | List[torch.Tensor] = None
    stream: torch.distributed.ProcessGroup | List[torch.distributed.ProcessGroup] = None  # op stream

    def __repr__(self) -> str:
        return f'{self.is_send} {self.src_rank} {self.dst_rank} \
            {self.buffer.size() if isinstance(self.buffer, torch.Tensor) else "List"}'


@dataclass
class MFCommTask(BaseCommTask):
    """Definition of a single memory-fabric operation"""
    idx: int | List[int] = None
    size: int = None  # tensor.numel() * tensor.element_size()
    is_send: bool | List[bool] = None
    src_rank: int | List[int] = None
    dst_rank: int | List[int] = None
    gva_ptr: int | List[int] = None
    buffer: int | List[int] = None
    tensor_slice: tuple = None

    def __repr__(self) -> str:
        return (f'{self.is_send=} {self.src_rank=} {self.dst_rank=} {self.size=} {self.gva_ptr=}'
                f'{self.buffer.size() if isinstance(self.buffer, torch.Tensor) else "None"}')
