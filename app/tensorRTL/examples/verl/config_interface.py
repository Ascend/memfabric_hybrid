
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

from enum import Enum, auto
from dataclasses import dataclass
from typing import Optional
import torch


class Engine(Enum):
    MEGATRON = auto()
    VLLM = auto()


class MoeExpertType(Enum):
    GroupedExpert = True
    SequentialExpert = True


@dataclass
class ParamPartitonConfig:
    shape: Optional[torch.Size] = None
    ndim: Optional[int] = 2
    fused: Optional[bool] = False
    fused_size: Optional[int] = None
    tp_partition: Optional[bool] = False
    pp_partition: Optional[bool] = True
    ep_partition: Optional[bool] = False
    shard_dim: Optional[int] = -1


def build_param_partition_config(
    param_size: torch.Size = None,
    param_ndim: int = 2,
    fused: bool = False,
    fused_size: int = -1,
    tp_partition: bool = False,
    pp_partition: bool = True,
    ep_partition: bool = False,
    shard_dim: int = -1
):
    return ParamPartitonConfig(
        shape=param_size,
        ndim=param_ndim,
        tp_partition=tp_partition,
        pp_partition=pp_partition,
        ep_partition=ep_partition,
        shard_dim=shard_dim,
        fused=fused,
        fused_size=fused_size
    )


def get_moe_members():
    return MoeExpertType.__members__