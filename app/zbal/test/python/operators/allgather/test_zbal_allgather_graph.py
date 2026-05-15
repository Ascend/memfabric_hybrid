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


def test_allgather_graph(case_list, backend="zbal", use_graph=False):
    def infer_fx(input_data, output_data, group_size, use_current=False):
        if not use_current:
            dist.all_gather_into_tensor(output_data, input_data)
        else:
            middle = torch.empty_like(output_data)
            dist.all_gather_into_tensor(middle, input_data)

            chunks = torch.chunk(middle, chunks=group_size, dim=0)
            dist.all_gather_into_tensor(middle, chunks[0])

            chunks = torch.chunk(middle, chunks=group_size, dim=0)
            dist.all_gather_into_tensor(output_data, chunks[0])

    local_rank = int(os.environ["LOCAL_RANK"])
    world_size = int(os.environ["WORLD_SIZE"] or 2)
    test_type = os.environ["TEST_TYPE"] or "int"
    current_dir = os.getenv("CURRENT_DIR", ".")
    # os.environ["ASCEND_LAUNCH_BLOCKING"] = "1"
    device_id = local_rank

    data_type = g_type_map.get(test_type, 'int')
    tensor_data_type = g_torch_type_map.get(test_type, 'int')

    if backend == "zbal":
        zbal_set_logger_level(3)
        local_mem = (world_size + 8) * 1024 * 1024 * 1024
        if not zbal_init(world_size, device_id, local_rank, local_mem):
            logging.error(f"zbal_init failed on rank {local_rank}.")
            return
        else:
            logging.info(f"zbal_init success on rank {local_rank}\n")

        group = dist.init_process_group("zbal", rank=local_rank, world_size=world_size)
        logging.info(f"init zbal group success on rank {local_rank=} {world_size=}")
    elif backend == "hccl":
        group = dist.init_process_group("hccl", rank=local_rank, world_size=world_size)
        logging.info(f"init hccl group success on rank {local_rank=} {world_size=}")

    if not use_graph:
        try:
            ret = 0
            for i, data_len in enumerate(case_list):
                golden_dir = f"allgather_{data_len}_{world_size}"
                data = np.fromfile(f"{current_dir}/golden/{golden_dir}/input_gm_{local_rank}.bin", dtype=data_type)
                in_tensor = torch.from_numpy(data).to(tensor_data_type).npu()
                gold_data = np.fromfile(f"{current_dir}/golden/{golden_dir}/golden.bin", dtype=data_type)
                gold_tensor = torch.from_numpy(gold_data).to(tensor_data_type).npu()
                out_tensor = torch.zeros(data_len * world_size, dtype=tensor_data_type).npu()
                for k in range(0, 10):
                    dist.all_gather_into_tensor(out_tensor, in_tensor)
                    # logging.info(f"{local_rank=}, {in_tensor=}, {out_tensor=}")
                    if not torch.allclose(gold_tensor, out_tensor, rtol=1e-4, atol=1e-8):
                        logging.error(f"[ERROR] rank {local_rank}, case {i} allgather result not correct\n")
                        ret = 1
                        break
            if ret == 0:
                logging.info(f"[INFO] rank {local_rank}, allgather run all case success\n")
            torch.npu.synchronize()
        finally:
            dist.destroy_process_group(group)

    else:
        ret = 0

        cur_device = f"npu:{device_id}"
        capture_stream = torch.npu.Stream(device=cur_device)
        graph_list = [torch.npu.NPUGraph() for _ in case_list]
        input_buf_list = [
            torch.rand(data_len, dtype=tensor_data_type, device=cur_device).contiguous()
            for data_len in case_list
        ]

        output_buf_list = [torch.rand([world_size * data_len]).to(dtype=tensor_data_type).to(cur_device).contiguous()
                           for data_len in case_list]

        cases_len = len(case_list)
        for case_idx in range(cases_len):
            graph = graph_list[case_idx]
            input_buf = input_buf_list[case_idx]
            output_buf = output_buf_list[case_idx]
            # warmup
            with torch.npu.stream(capture_stream):
                dist.all_gather_into_tensor(output_buf, input_buf)
            # capture
            with torch.npu.graph(graph, stream=capture_stream):
                infer_fx(input_buf, output_buf, world_size)
        torch.npu.synchronize()

        # replay
        for case_idx in range(cases_len):
            graph = graph_list[case_idx]
            input_buf = input_buf_list[case_idx]
            output_buf = output_buf_list[case_idx]

            golden_dir = f"allgather_{case_list[case_idx]}_{world_size}"
            data = np.fromfile(f"{current_dir}/golden/{golden_dir}/input_gm_{local_rank}.bin", dtype=data_type)
            in_tensor = torch.from_numpy(data).to(tensor_data_type).npu()
            input_buf.copy_(in_tensor)

            gold_data = np.fromfile(f"{current_dir}/golden/{golden_dir}/golden.bin", dtype=data_type)
            gold_tensor = torch.from_numpy(gold_data).to(tensor_data_type).npu()
            output_tensor = torch.empty_like(gold_tensor)

            for _ in range(1):
                with torch.npu.stream(capture_stream):
                    graph.replay()
            capture_stream.synchronize()
            output_tensor.copy_(output_buf)
            if not torch.allclose(gold_tensor, output_tensor, rtol=1e-4, atol=1e-8):
                logging.info(f"[ERROR] rank {local_rank}, case {case_idx} allgather result not correct\n")
                ret += 1
                break

    if ret == 0:
        logging.info(f"[INFO] rank {local_rank}, allgather run all case success\n")

    if backend == "zbal" and not zbal_uninit():
        logging.error("zbal uninit failed.")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('dist_type', type=str, choices=["hccl", "zbal"])
    parser.add_argument('--case_num', type=int, default=0)
    parser.add_argument('--case_list', type=str, nargs='*', default=[])
    args = parser.parse_args()

    dist_type = args.dist_type
    case_num = args.case_num
    case_list = args.case_list
    case_list = [int(case) for case in case_list]

    if case_num == 0:
        assert len(case_list) >= 1, "case_num is 0, using case_list but case_list is None"
    else:
        case_list = [6 * (2 ** i) for i in range(case_num)]

    test_allgather_graph(case_list, backend=dist_type, use_graph=True)
