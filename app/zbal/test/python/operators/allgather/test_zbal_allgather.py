import os
import sys
import time
import logging
import torch
import torch.distributed as dist
import torch_npu
import numpy as np
from zbal import zbal_init, zbal_uninit, zbal_set_logger_level

torch_npu.npu.config.allow_internal_format = True
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s',
                    handlers=[logging.StreamHandler()])

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


def test_allgather(case_list, dist_type, data_op_type):
    global_rank = int(os.environ["RANK"])
    local_rank = int(os.environ["LOCAL_RANK"])
    world_size = int(os.environ["WORLD_SIZE"] or 2)
    test_type = os.environ["TEST_TYPE"] or "int"
    current_dir = os.getenv("CURRENT_DIR", ".")
    device_id = local_rank

    data_type = g_type_map.get(test_type, 'int')
    tensor_data_type = g_torch_type_map.get(test_type, 'int')

    if dist_type == 'zbal':
        zbal_set_logger_level(3)
        local_mem = (world_size + 8) * 1024 * 1024 * 1024
        if not zbal_init(world_size, device_id, global_rank, local_mem, data_op_type=data_op_type):
            logging.error(f"zbal_init failed on rank {global_rank}.")
            return
        else:
            logging.info(f"zbal_init success on rank {global_rank}\n")

        group = dist.init_process_group("zbal", rank=global_rank, world_size=world_size)
        logging.info(f"init zbal group success on rank {global_rank=} {world_size=}")
    else:
        torch.npu.set_device(device_id)
        group = dist.init_process_group("hccl", rank=global_rank, world_size=world_size)
        logging.info(f"init hccl group success on rank {global_rank=} {world_size=}")

    allgather_list = os.getenv("ZBAL_AG_LIST", "0") == "1"
    enable_2_dims = os.getenv("ZBAL_AG_2_DIMS", "0") == "1"
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
        if dist_type == 'zbal':
            profiling_path = f"{current_dir}/profiling.zbal_{case_list[0]}/"
        else:
            profiling_path = f"{current_dir}/profiling.hccl_{case_list[0]}/"
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
        for case_id, data_len in enumerate(case_list):
            prof_cnt = 0
            if enable_profiling:
                torch.npu.synchronize()
                prof.start()
            golden_dir = f"allgather_{data_len}_{world_size}"
            data = np.fromfile(f"{current_dir}/golden/{golden_dir}/input_gm_{local_rank}.bin", dtype=data_type)
            in_tensor = torch.from_numpy(data).to(tensor_data_type).npu()

            # test with 2 dimensions
            view_0_dim = 2

            if allgather_list:
                if enable_2_dims:
                    in_tensor = in_tensor.view(view_0_dim, -1)
                out_tensor_list = [torch.zeros_like(in_tensor, dtype=tensor_data_type).npu() for _ in range(world_size)]
            else:
                out_tensor = torch.zeros(data_len * world_size, dtype=tensor_data_type).npu()
                if enable_2_dims:
                    out_tensor = out_tensor.view(world_size * view_0_dim, -1)

            tensor_output_dir = f"{current_dir}/output/allgather_{data_len}_{world_size}/"
            os.makedirs(tensor_output_dir, exist_ok=True)
            if dist_type == 'zbal':
                golden_tensor = torch.load(f"{tensor_output_dir}/output_hccl_{global_rank}.bin",
                                           weights_only=False).npu()
            for k in range(0, 20):
                if enable_profiling and prof_cnt > 5:
                    prof.step()

                if allgather_list:
                    dist.all_gather(out_tensor_list, in_tensor)
                    out_tensor = torch.cat(out_tensor_list, dim=0)
                else:
                    dist.all_gather_into_tensor(out_tensor, in_tensor)
                prof_cnt = prof_cnt + 1

                if dist_type == 'hccl' and k == 0:
                    tensor_output_file = f"{tensor_output_dir}/output_hccl_{global_rank}.bin"
                    torch.save(out_tensor.cpu(), tensor_output_file)
                    break
                elif dist_type == 'zbal':
                    if not torch.allclose(out_tensor, golden_tensor, rtol=1e-4, atol=1e-8):
                        logging.error(f"allgather {world_size=} {global_rank=} {data_len=} precision failed. ")
                        raise Exception("precision error")

            if dist_type == "zbal":
                logging.info(f"allgather {world_size=} {global_rank=} {data_len=} {k} times compare precision "
                             f"success {os.linesep}")
            else:
                logging.info(f"allgather {world_size=} {global_rank=} {data_len=} {k} generate success{os.linesep}")

            if enable_profiling:
                torch.npu.synchronize()
                prof.stop()

        torch.npu.synchronize()
    finally:
        dist.destroy_process_group(group)

    if not zbal_uninit():
        logging.error("zbal uninit failed.")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('dist_type', type=str, choices=["hccl", "zbal"])
    parser.add_argument('--case_num', type=int, default=0)
    parser.add_argument('--case_list', type=str, nargs='*', default=[])
    parser.add_argument('--data_op_type', type=int, default=0)
    args = parser.parse_args()

    dist_type = args.dist_type
    case_num = args.case_num
    case_list = args.case_list
    case_list = [int(case) for case in case_list]
    data_op_type = args.data_op_type

    if case_num == 0:
        logging.info(f"case_list:{case_list}")
    else:
        case_list = [6 * (2 ** i) for i in range(case_num)]

    test_allgather(case_list, dist_type, data_op_type)
