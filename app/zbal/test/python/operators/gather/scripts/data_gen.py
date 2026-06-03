#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os
import numpy as np

from ml_dtypes import bfloat16


def gen_random_data(row_num, size, dtype):
    return np.random.uniform(low=0.0, high=10.0, size=(row_num, size)).astype(dtype)


def golden_generate(data_len, rank_size, hidden_size, data_type, current_dir):
    rows_per_rank = data_len // hidden_size
    total_rows = rows_per_rank * rank_size
    golden_dir = f"gather_{rank_size}_{rows_per_rank}_{hidden_size}"
    cmd = f"mkdir -p {current_dir}/golden/{golden_dir}"
    os.system(cmd)

    dst = 0
    output_gm = np.zeros((total_rows, hidden_size), dtype=data_type)

    for i in range(rank_size):
        input_tensor = gen_random_data(rows_per_rank, hidden_size, dtype=data_type)
        input_tensor.tofile(f"{current_dir}/golden/{golden_dir}/input_gm_{i}.bin")
        if i == dst:
            output_gm[i * rows_per_rank:(i + 1) * rows_per_rank] = input_tensor

    output_gm.tofile(f"{current_dir}/golden/{golden_dir}/golden_{dst}.bin")


def gen_golden_data():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('rank_size', type=int)
    parser.add_argument('test_type', type=str)
    parser.add_argument('--case_num', type=int, default=0)
    parser.add_argument('--case_list', type=str, nargs='*', default=[])
    parser.add_argument('--hidden_size', type=int, default=7168)
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
    hidden_size = args.hidden_size
    current_dir = os.getenv("CURRENT_DIR", ".")
    if case_num == 0:
        for data_len in case_list:
            golden_generate(data_len, rank_size, hidden_size, data_type, current_dir)
    else:
        for i in range(case_num):
            data_len = 8 * (2 ** i)
            golden_generate(data_len, rank_size, hidden_size, data_type, current_dir)


if __name__ == '__main__':
    gen_golden_data()
    exit(0)
