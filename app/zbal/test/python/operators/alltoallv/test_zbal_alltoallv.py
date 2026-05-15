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
import logging
import os
import argparse
import torch
import torch.distributed as dist
import torch_npu
import numpy as np
from zbal import zbal_init, zbal_uninit, zbal_set_logger_level

enable_perf_test = os.environ.get("ZBAL_ENABLE_ALLTOALL_PERF_TEST", "0") == "1"
if enable_perf_test:
    from scripts.inout_splits_perf import input_shapes, output_shapes, input_splits, output_splits
else:
    from scripts.inout_splits import input_shapes, output_shapes, input_splits, output_splits


torch_npu.npu.config.allow_internal_format = True
RANK_PER_NODE = 16
logger = logging.getLogger(__name__)
logging.basicConfig(
    level=logging.INFO,
    format='[%(filename)s:%(lineno)d - %(funcName)s()] - %(message)s'
)


def get_cur_cases(expect_world_size):
    cur_input_shapes = []
    cur_output_shapes = []
    cur_input_split = []
    cur_output_split = []
    case_index = []
    for i in range(len(input_shapes)):
        if len(input_shapes[i]) == expect_world_size:
            cur_input_shapes.append(input_shapes[i])
            cur_output_shapes.append(output_shapes[i])
            cur_input_split.append(input_splits[i])
            cur_output_split.append(output_splits[i])
            case_index.append(i)

    return cur_input_shapes, cur_output_shapes, cur_input_split, cur_output_split, case_index


def test_alltoallv(dist_type):
    global_rank = int(os.environ["RANK"])
    local_rank = int(os.environ["LOCAL_RANK"])
    world_size = int(os.environ["WORLD_SIZE"] or 2)
    test_type = os.environ["TEST_TYPE"] or "int"
    current_dir = os.environ.get("CURRENT_DIR", ".")
    enable_profiling = os.environ.get("ENABLE_PROFILING", "0") == "1"
    device_id = local_rank
    cur_input_shapes, cur_output_shapes, cur_input_splits, cur_output_splits, case_index = get_cur_cases(world_size)
    if not cur_input_shapes:
        logger.warning(f"{global_rank} find 0 case with {world_size=}")
        return

    type_map = {
        "int": np.int32,
        "int32_t": np.int32,
        "float16_t": np.float16,
        "float": np.float32,
        "bfloat16_t": np.float16,
    }
    data_type = type_map.get(test_type, 'int')

    torch_type_map = {
        "int": torch.int32,
        "int32_t": torch.int32,
        "float16_t": torch.float16,
        "float": torch.float32,
        "bfloat16_t": torch.bfloat16
    }

    tensor_data_type = torch_type_map.get(test_type, 'int')

    if dist_type == "zbal":
        zbal_set_logger_level(3)
        if enable_perf_test:
            local_mem = 4 * 1024 * 1024 * 1024
        else:
            local_mem = 128 * 1024 * 1024
        if not zbal_init(world_size, device_id, global_rank, local_mem):
            logger.error(f"zbal_init failed on rank {global_rank}.")
            return
        else:
            logger.info(f"zbal_init success on rank {global_rank}\n")

        group = dist.init_process_group("zbal", rank=global_rank, world_size=world_size)
        logger.info(f"init zbal group success on rank {global_rank=} {world_size=}")
    else:
        torch.npu.set_device(device_id)
        group = dist.init_process_group("hccl", rank=global_rank, world_size=world_size)
        logger.info(f"init hccl group success on rank {global_rank=} {world_size=}")

    if enable_profiling:
        prof_cnt = 0
        experimental_config = torch_npu.profiler._ExperimentalConfig(
            aic_metrics=torch_npu.profiler.AiCMetrics.PipeUtilization,
            profiler_level=torch_npu.profiler.ProfilerLevel.Level2,
            l2_cache=False,
            data_simplification=False,
        )
        profiling_path = f"{current_dir}/profiling.{dist_type}_{world_size}/"
        prof = torch_npu.profiler.profile(
            activities=[
                torch_npu.profiler.ProfilerActivity.CPU,
                torch_npu.profiler.ProfilerActivity.NPU,
            ],
            on_trace_ready=torch_npu.profiler.tensorboard_trace_handler(
                profiling_path
            ),
            schedule=torch_npu.profiler.schedule(
                    wait=1, warmup=1, active=30, repeat=1, skip_first=4
                ),
                record_shapes=True,
                profile_memory=True,
                with_stack=False,
                with_flops=False,
                with_modules=False,
                experimental_config=experimental_config,
        )

    try:
        prof_cnt = 0
        if enable_profiling:
            torch.npu.synchronize()
            prof.start()
        for i in range(len(case_index)):
            case_id = case_index[i]
            tensor_output_dir = f"{current_dir}/output/alltoallv_{case_id}_{world_size}_{global_rank}/"
            os.makedirs(tensor_output_dir, exist_ok=True)
            if dist_type == 'zbal':
                golden_tensor = torch.load(f"{tensor_output_dir}/output_hccl.bin", weights_only=False).npu()

            repeat_times = 15
            for j in range(repeat_times):
                try:
                    if enable_profiling and prof_cnt >= 1:
                        prof.step()

                    cur_input_shape = cur_input_shapes[i][global_rank]
                    if len(cur_input_shape) == 2:
                        input_row_num, data_len = cur_input_shape
                    else:
                        input_row_num = 1
                        data_len = cur_input_shape[0]
                    golden_input_dir = f"alltoallv_{case_id}_{world_size}_{global_rank}/"
                    golden_file = f"{current_dir}/golden/{golden_input_dir}/input_gm_{global_rank}.bin"
                    data = np.fromfile(golden_file, dtype=data_type)
                    if len(cur_input_shape) == 2:
                        tensor_input = torch.from_numpy(data).to(tensor_data_type).npu().view(input_row_num, data_len)
                    else:
                        tensor_input = torch.from_numpy(data).to(tensor_data_type).npu().view(data_len)

                    cur_output_shape = cur_output_shapes[i][global_rank]

                    cur_output_split = cur_output_splits[i][global_rank]
                    cur_input_split = cur_input_splits[i][global_rank]
                    tensor_output = torch.zeros(cur_output_shape, dtype=tensor_input.dtype, device=tensor_input.device)

                    dist.barrier()
                    dist.all_to_all_single(tensor_output, tensor_input, cur_output_split, cur_input_split)
                    prof_cnt += 1

                    if dist_type == 'hccl':
                        tensor_output_file = f"{tensor_output_dir}/output_{dist_type}.bin"
                        torch.save(tensor_output.cpu(), tensor_output_file)
                    elif dist_type == 'zbal':
                        if not torch.allclose(tensor_output, golden_tensor, rtol=1e-4, atol=1e-8):
                            logger.exception(f"alltoallv {world_size=} {global_rank=} {data_len=} precision failed.")
                            raise Exception("precision error")
                except Exception as e:
                    logger.exception(f"{global_rank=} run alltoallv case {i} round {j} failed. {e}")

        if dist_type == 'hccl':
            logger.info(f"alltoallv {global_rank=} {world_size=} {len(case_index)} {dist_type} cases generate success")
        else:
            logger.info(f"alltoallv {global_rank=} {world_size=} case len={len(case_index)} {dist_type} "
                        f"cases {repeat_times} times compare success")

        if enable_profiling and prof_cnt >= 1:
            torch.npu.synchronize()
            prof.stop()
    finally:
        dist.destroy_process_group(group)

    if dist_type == "zbal" and not zbal_uninit():
        logger.error("zbal uninit failed.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('dist_type', type=str, choices=["hccl", "zbal"])
    args = parser.parse_args()
    dist_type = args.dist_type
    test_alltoallv(dist_type)
