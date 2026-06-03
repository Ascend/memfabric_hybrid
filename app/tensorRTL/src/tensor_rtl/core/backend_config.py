
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.

from dataclasses import dataclass
from typing import Union
try:
    import memfabric_hybrid
    from memfabric_hybrid import bm
except ImportError:
    memfabric_hybrid = None
    bm = None


@dataclass
class MfBmConfig:
    url: str = 'tcp://127.0.0.1:8570'
    mem_type: Union[bm.BmMemType.HOST, bm.BmMemType.DEVICE] = bm.BmMemType.DEVICE
    data_op_type: Union[bm.BmDataOpType.SDMA, bm.BmDataOpType.HOST_RDMA] = bm.BmDataOpType.SDMA
    local_dram_size: int = 0
    local_hbm_size: int = 0
    auto_ranking: bool = False
    hybm_int_gvm_flag: int = 2
    rank_world_size: int = 8

    def __post_init__(self):
        if self.mem_type == bm.BmMemType.HOST:
            raise TypeError(f"{self.mem_type} not supported yet")
        if self.data_op_type == bm.BmDataOpType.HOST_RDMA:
            raise TypeError(f"{self.data_op_type} not supported yet")

        if self.mem_type not in [bm.BmMemType.HOST, bm.BmMemType.DEVICE]:
            raise TypeError(
                f"name must be in [bm.BmMemType.HOST, bm.BmMemType.DEVICE], got {self.mem_type}"
            )
        if self.data_op_type not in [bm.BmDataOpType.SDMA, bm.BmDataOpType.HOST_RDMA]:
            raise TypeError(
                f"name must be in [bm.BmDataOpType.SDMA, bm.BmDataOpType.HOST_RDMA], got {self.data_op_type}"
            )