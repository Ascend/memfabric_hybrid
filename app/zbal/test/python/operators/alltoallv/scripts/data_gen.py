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
enable_perf_test = os.environ.get("ZBAL_ENABLE_ALLTOALL_PERF_TEST", "0") == "1"
if enable_perf_test:
    from scripts.inout_splits_perf import input_shapes
else:
    from scripts.inout_splits import input_shapes
from ml_dtypes import bfloat16


def gen_random_data_2d(row_num, size, dtype):
    return np.random.uniform(low=0.0, high=10.0, size=(row_num, size)).astype(dtype)


def gen_random_data_1d(size, dtype):
    return np.random.uniform(low=0.0, high=10.0, size=(size,)).astype(dtype)


def golden_generate(case_index, cur_input_shapes, data_type, current_dir):
    rank_size = len(cur_input_shapes)

    for rank_id in range(rank_size):
        golden_input_dir = f"alltoallv_{case_index}_{rank_size}_{rank_id}/"
        golden_input_file = f"{current_dir}/golden/{golden_input_dir}/input_gm_{rank_id}.bin"
        if os.path.isfile(golden_input_file):
            continue

        cmd = f"mkdir -p {current_dir}/golden/{golden_input_dir}"
        os.system(cmd)

        rank_i_input_shape = cur_input_shapes[rank_id]
        if len(rank_i_input_shape) == 2:
            rank_i_row, rank_i_hidden = rank_i_input_shape
            input_tensor = gen_random_data_2d(rank_i_row, rank_i_hidden, dtype=data_type)
        else:
            rank_i_row = 1
            rank_i_hidden = rank_i_input_shape[0]
            input_tensor = gen_random_data_1d(rank_i_hidden, dtype=data_type)

        # dump input to file
        input_tensor.tofile(golden_input_file)


def gen_golden_data():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('test_type', type=str)
    parser.add_argument('rank_size', type=int)
    args = parser.parse_args()

    type_map = {
        "int": np.int32,
        "int32_t": np.int32,
        "float16_t": np.float16,
        "float": np.float32,
        "bfloat16_t": bfloat16
    }

    current_dir = os.getenv("CURRENT_DIR", ".")

    data_type = type_map.get(args.test_type, 'int')
    rank_size = args.rank_size
    for i in range(len(input_shapes)):
        if len(input_shapes[i]) == rank_size:
            golden_generate(i, input_shapes[i], data_type, current_dir)


if __name__ == '__main__':
    gen_golden_data()
    exit(0)