#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import sys
import os
import numpy as np


def gen_golden_data_simple(rank_size):
    golden = np.zeros([16, 2048], dtype=np.float16)
    for idx in range(rank_size):
        np_input = np.random.uniform(1, 100, [16, 2048]).astype(np.float16)
        golden = (golden + np_input).astype(np.float16)
        input_name = os.path.join('input', f'input_{idx}.bin')
        np_input.tofile(input_name)

    golden.tofile("./output/golden.bin")


if __name__ == "__main__":
    rank_size = int(sys.argv[1])
    gen_golden_data_simple(rank_size)
