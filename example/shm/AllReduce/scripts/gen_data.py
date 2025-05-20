#!/usr/bin/python3
# coding=utf-8
#
# Copyright (C) 2023-2024. Huawei Technologies Co., Ltd. All rights reserved.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# ===============================================================================

import numpy as np
import sys


def gen_golden_data_simple(rank_size):
    golden = np.zeros([16, 2048], dtype=np.float16)
    for idx in range(rank_size):
        input = np.random.uniform(1, 100, [16, 2048]).astype(np.float16)
        golden = (golden + input).astype(np.float16)
        input_name = './input/input_' + str(idx) + '.bin'
        input.tofile(input_name)

    golden.tofile("./output/golden.bin")


if __name__ == "__main__":
    rank_size = int(sys.argv[1])
    gen_golden_data_simple(rank_size)
