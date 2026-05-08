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

import logging
import os
import numpy as np
from ml_dtypes import bfloat16

logger = logging.getLogger(__name__)


def gen_random_data(size, dtype):
    return np.random.uniform(low=0.0, high=10.0, size=size).astype(dtype)


def golden_generate(data_len, rank_size, data_type, current_dir):
    golden_dir = f"alltoall_{data_len}_{rank_size}"
    cmd = f"mkdir -p {current_dir}/golden/{golden_dir}"
    os.system(cmd)

    # generate all input
    for i in range(rank_size):
        input_tensor = gen_random_data(data_len, dtype=data_type)
        input_tensor.tofile(f"{current_dir}/golden/{golden_dir}/input_gm_{i}.bin")

    logger.info(f"{rank_size=:3} {data_len=:5} alltoall golden generate success !")


def gen_golden_data():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('test_type', type=str)
    args = parser.parse_args()

    type_map = {
        "int": np.int32,
        "int32_t": np.int32,
        "float16_t": np.float16,
        "float": np.float32,
        "bfloat16_t": bfloat16
    }

    current_dir = os.getenv("CURRENT_DIR", ".")
    with open(f"{current_dir}/scripts/input_shapes.txt", "r") as f:
        f.readline()
        lines = f.readlines()

    data_type = type_map.get(args.test_type, 'int')
    for line in lines:
        if not line.strip() or line.startswith("#"):
            continue
        rank_size, input_rows, data_len = [int(x) for x in line.split(",")]
        golden_generate(rank_size, input_rows, data_len, data_type, current_dir)


def gen_golden_data():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('rank_size', type=int)
    parser.add_argument('test_type', type=str)
    parser.add_argument('--case_num', type=int, default=0)
    parser.add_argument('--case_list', type=str, nargs='*', default=[])
    args = parser.parse_args()

    type_map = {
        "int": np.int32,
        "int32_t": np.int32,
        "float16_t": np.float16,
        "float": np.float32,
        "bfloat16_t": bfloat16
    }

    data_type = type_map.get(args.test_type, 'int')
    rank_size = args.rank_size

    case_num = args.case_num
    case_list = args.case_list
    case_list = [int(case) for case in case_list]
    current_dir = os.getenv("CURRENT_DIR", ".")
    if case_num == 0:
        logger.info("case_num is 0, using case_list")
        for data_len in case_list:
            golden_generate(data_len, rank_size, data_type, current_dir)
    else:
        for i in range(case_num):
            data_len = 8 * (2 ** i)
            golden_generate(data_len, rank_size, data_type, current_dir)


if __name__ == '__main__':
    gen_golden_data()
    exit(0)