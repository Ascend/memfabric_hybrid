#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# ZBAL is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import logging
import os
import time
import random
import torch
import torch.distributed as dist
import torch_npu
import numpy as np
from ml_dtypes import bfloat16
from zbal import zbal_init, zbal_uninit, zbal_set_logger_level

torch_npu.npu.config.allow_internal_format = True
logger = logging.getLogger(__name__)

g_type_map = {
    "int": np.int32,
    "int32_t": np.int32,
    "float16_t": np.float16,
    "float": np.float32,
    "bfloat16_t": np.float16
}

g_torch_type_map = {
    "int": torch.int32,
    "int32_t": torch.int32,
    "float16_t": torch.float16,
    "float": torch.float32,
    "bfloat16_t": torch.bfloat16
}



def get_golden_from_file(filepath):
    """load pre-computed HCCL golden tensor from disk"""
    return torch.load(filepath, weights_only=False).npu()


def is_perf_test():
    """check if running in performance test mode"""
    return os.environ.get("ZBAL_ENABLE_PERF_TEST", "0") == "1"


def get_golden_by_assembly(golden_dir, current_dir, data_type, tensor_data_type):
    """p2p golden = sender's input tensor"""
    data = np.fromfile(f"{current_dir}/golden/{golden_dir}/input_gm.bin", dtype=data_type)
    return torch.from_numpy(data).to(tensor_data_type).npu()


def test_p2p(dist_type, case_list, send_rank, recv_rank, data_op_type):
    global_rank = int(os.environ["RANK"])
    local_rank = int(os.environ["LOCAL_RANK"])
    world_size = int(os.environ["WORLD_SIZE"] or 2)
    test_type = os.environ["TEST_TYPE"] or "int"
    current_dir = os.getenv("CURRENT_DIR", ".")
    device_id = local_rank

    data_type = g_type_map.get(test_type, 'int')
    tensor_data_type = g_torch_type_map.get(test_type, 'int')

    if dist_type == "zbal":
        zbal_set_logger_level(3)
        local_mem = world_size * 256 * 1024 * 1024
        if not zbal_init(world_size, device_id, global_rank, local_mem, data_op_type=data_op_type):
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

    check_precision = os.getenv("CHECK_PRECISION", "1") == "1"
    enable_profiling = os.getenv("ENABLE_PROFILING", "0") == "1"
    profiling_step = int(os.getenv("PROFILING_STEP", "10"))
    if enable_profiling:
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
                wait=1, warmup=1, active=profiling_step, repeat=1, skip_first=1
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
        send_role = global_rank == send_rank
        recv_role = global_rank == recv_rank
        if send_role or recv_role:
            for i, data_len in enumerate(case_list):
                prof_cnt = 0
                if enable_profiling:
                    torch.npu.synchronize()
                    prof.start()
                golden_dir = f"p2p_{data_len}"
                data = np.fromfile(f"{current_dir}/golden/{golden_dir}/input_gm.bin", dtype=data_type)
                send_tensor = torch.from_numpy(data).to(tensor_data_type).npu()
                recv_tensor = torch.zeros(data_len, dtype=tensor_data_type).npu()

                tensor_output_dir = f"{current_dir}/output/{golden_dir}/"
                os.makedirs(tensor_output_dir, exist_ok=True)
                if check_precision and recv_role and dist_type == 'zbal':
                    if is_perf_test():
                        golden_tensor = get_golden_from_file(f"{tensor_output_dir}/output_hccl.bin")
                    else:
                        golden_tensor = get_golden_by_assembly(golden_dir, current_dir, data_type, tensor_data_type)

                for k in range(0, 20):
                    if enable_profiling and prof_cnt > 5:
                        prof.step()

                    if send_role:
                        dist.send(send_tensor, recv_rank)
                    else:
                        dist.recv(recv_tensor, send_rank)

                    prof_cnt = prof_cnt + 1
                    if check_precision:
                        if dist_type == 'hccl' and i == 0:
                            if recv_role:
                                tensor_output_file = f"{tensor_output_dir}/output_hccl.bin"
                                torch.save(recv_tensor.cpu(), tensor_output_file)
                            break
                        elif (dist_type == 'zbal' and recv_role and
                              not torch.allclose(golden_tensor, recv_tensor, rtol=1e-4, atol=1e-8)):
                            logger.error(golden_tensor, recv_tensor)
                            logger.error(f"[ERROR] rank {global_rank}, case {i} p2p result not correct\n")
                            raise Exception(f"procesion error case:{data_len}")

                if enable_profiling:
                    torch.npu.synchronize()
                    prof.stop()

        dist.barrier()
        if ret == 0:
            logger.info(f"[INFO] rank {global_rank}, p2p run all case success\n")
        torch.npu.synchronize()
    finally:
        dist.destroy_process_group(group)

    if dist_type == "zbal" and not zbal_uninit():
        logger.error("zbal uninit failed.")


if __name__ == "__main__":
    import argparse
    world_size = int(os.environ["WORLD_SIZE"] or 2)
    parser = argparse.ArgumentParser()
    parser.add_argument('dist_type', type=str, choices=["hccl", "zbal"])
    parser.add_argument('--case_num', type=int, default=0)
    parser.add_argument('--case_list', type=str, nargs='*', default=[])
    parser.add_argument('--send_rank', type=int, default=0)
    parser.add_argument('--recv_rank', type=int, default=world_size - 1)
    parser.add_argument('--data_op_type', type=int, default=0)
    args = parser.parse_args()

    dist_type = args.dist_type
    case_num = args.case_num
    case_list = args.case_list
    case_list = [int(case) for case in case_list]
    send_rank = args.send_rank
    recv_rank = args.recv_rank
    data_op_type = args.data_op_type

    if case_num == 0:
        logger.info(f"case_list:{case_list}")
    else:
        case_list = [8 * (2 ** i) for i in range(case_num)]

    test_p2p(dist_type, case_list, send_rank, recv_rank, data_op_type)

