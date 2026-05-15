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
import argparse
import logging
import torch
import torch.distributed as dist
import torch_npu
import numpy as np
from zbal import zbal_init, zbal_uninit, zbal_set_logger_level


torch_npu.npu.config.allow_internal_format = True
logger = logging.getLogger(__name__)


def test_broadcast(dist_type, case_list, hidden_size):
    global_rank = int(os.environ["RANK"])
    local_rank = int(os.environ["LOCAL_RANK"])
    world_size = int(os.environ["WORLD_SIZE"] or 2)
    test_type = os.environ["TEST_TYPE"] or "int"
    current_dir = os.environ.get("CURRENT_DIR", ".")
    check_precision = os.getenv("CHECK_PRECISION", "1") == "1"
    enable_profiling = os.environ.get("ENABLE_PROFILING", "0") == "1"
    profiling_step = int(os.getenv("PROFILING_STEP", "10"))
    device_id = local_rank

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
        zbal_set_logger_level(2)
        local_mem = 4 * 1024 * 1024 * 1024
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
        profiling_path = f"{current_dir}/profiling.{dist_type}_{case_list[0]}/"
        prof = torch_npu.profiler.profile(
            activities=[
                torch_npu.profiler.ProfilerActivity.CPU,
                torch_npu.profiler.ProfilerActivity.NPU,
            ],
            on_trace_ready=torch_npu.profiler.tensorboard_trace_handler(
                profiling_path
            ),
            schedule=torch_npu.profiler.schedule(
                    wait=1, warmup=1, active=10, repeat=1, skip_first=1
                ),
                record_shapes=True,
                profile_memory=True,
                with_stack=False,
                with_flops=False,
                with_modules=False,
                experimental_config=experimental_config,
        )

    try:
        ret = 0
        prof_cnt = 0
        if enable_profiling:
            torch.npu.synchronize()
            prof.start()
        for data_len in case_list:
            row_num = data_len // hidden_size
            golden_dir = f"broadcast_{world_size}_{row_num}_{hidden_size}"
            tensor_output_dir = f"{current_dir}/output/broadcast_{data_len}_{world_size}/"
            os.makedirs(tensor_output_dir, exist_ok=True)

            if dist_type == 'zbal':
                golden_tensor = torch.load(f"{tensor_output_dir}/output_hccl_{data_len}_{global_rank}.bin")

            for k in range(15):
                if enable_profiling and prof_cnt > 1:
                    prof.step()
                root = 0
                data = np.fromfile(f"{current_dir}/golden/{golden_dir}/input_gm_{root}.bin", dtype=data_type)
                tensor_input = torch.from_numpy(data).to(tensor_data_type).npu().view(row_num, hidden_size)
                if global_rank != root:
                    tensor_input = torch.zeros_like(tensor_input, dtype=tensor_input.dtype, device=tensor_input.device)

                tensor_output = torch.zeros_like(tensor_input, dtype=tensor_input.dtype, device=tensor_input.device)
                if global_rank == root:
                    tensor_output = tensor_input
                dist.barrier()
                dist.broadcast(tensor_output, src=root)
                prof_cnt += 1
                if dist_type == 'hccl' and k == 0:
                    tensor_output_file = f"{tensor_output_dir}/output_hccl_{data_len}_{global_rank}.bin"
                    torch.save(tensor_output, tensor_output_file)
                    break
                elif dist_type == 'zbal':
                    if not torch.allclose(golden_tensor, tensor_output, rtol=1e-4, atol=1e-8):
                        logger.error(f"rank {global_rank} case {data_len} broadcast result not correct")
                        raise Exception(f"procesion error case:{data_len}")
            logger.info(f"{global_rank=} {world_size=} {data_len} {dist_type} broadcast cases run successfully")
        if enable_profiling:
            torch.npu.synchronize()
            prof.stop()
    finally:
        dist.destroy_process_group(group)

    if dist_type == "zbal" and not zbal_uninit():
        logger.error("zbal uninit failed.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('dist_type', type=str, choices=["hccl", "zbal"])
    parser.add_argument('--case_num', type=int, default=0)
    parser.add_argument('--case_list', type=str, nargs='*', default=[])
    parser.add_argument('--hidden_size', type=int, default=0)
    args = parser.parse_args()

    dist_type = args.dist_type
    case_num = args.case_num
    case_list = args.case_list
    case_list = [int(case) for case in case_list]
    hidden_size = args.hidden_size
    test_broadcast(dist_type, case_list, hidden_size)