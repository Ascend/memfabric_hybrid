import argparse
import math
import os
import logging
import random
import time
from functools import partial
from typing import Optional

import zbal
from zbal import Buffer, Config
import numpy as np
import torch
import torch_npu
import torch.distributed as dist

from utils import (
    bench,
    bench_kineto,
    calc_diff,
    calculate_avg_stats,
    init_dist,
    inplace_unique,
    per_token_cast_back,
)


# noinspection PyShadowingNames
def test_main(
    args: argparse.Namespace,
    num_local_ranks: int,
    local_rank: int,
    num_ranks: int,
    rank: int,
    buffer: zbal.Buffer,
    group: dist.ProcessGroup,
):
    # Settings
    num_tokens, hidden = args.num_tokens, args.hidden
    num_topk, num_experts = args.num_topk, args.num_experts
    num_servers = num_ranks // num_local_ranks
    expert_token_nums_type = int(os.getenv("MOE_EXPERT_TOKEN_NUMS_TYPE", 1))
    use_quant = os.getenv("DEEP_NORMAL_MODE_USE_INT8_QUANT") == "1"

    multi_list = [1] * num_ranks
    num_tokens = int(num_tokens * multi_list[rank])
    if num_tokens == 0:
        num_tokens = 1

    assert num_experts % num_ranks == 0
    logging.info(
        f"[config] {rank=} {num_tokens=}, {hidden=}, {num_topk=}, {num_experts=}, "
        f"{num_ranks=}, {multi_list=}"
    )

    experts_per_rank = num_experts // num_ranks
    # Default: random over all experts (original behavior)
    scores = (
        torch.randn(
            (num_tokens, num_experts), dtype=torch.float32, device="npu"
        ).abs()
        + 1
    )
    # topk_idx = torch.topk(scores, num_topk, dim=-1, largest=True, sorted=False)[1]
    topk_idx = torch.zeros((num_tokens, num_topk), dtype=torch.int64, device='npu')
    for t in range(num_tokens):
        start = (t * num_topk) % num_experts
        for k in range(num_topk):
            topk_idx[t, k] = (start + k) % num_experts

    rank_idx = topk_idx // experts_per_rank
    rank_idx.masked_fill_(topk_idx == -1, -1)
    inplace_unique(rank_idx, num_ranks)

    # Expert meta
    num_tokens_per_expert = torch.zeros((num_experts,), dtype=torch.int, device="npu")
    for i in range(num_experts):
        num_tokens_per_expert[i] = (topk_idx == i).sum()
    gbl_num_tokens_per_expert = num_tokens_per_expert.clone()
    torch.npu.synchronize()
    # [bug] for all_reduce data sync
    logging.info(f"[before] {rank=} {gbl_num_tokens_per_expert[0]=}")
    dist.all_reduce(gbl_num_tokens_per_expert, group=group)
    logging.info(f"[after] {rank=} {gbl_num_tokens_per_expert[0]=}")

    # Rank layout meta
    num_tokens_per_rank = torch.empty((num_ranks,), dtype=torch.int, device="npu")
    token_idx_in_rank = torch.full(
        (num_ranks, num_tokens), -1, dtype=torch.long, device="npu"
    )
    for i in range(num_ranks):
        num_tokens_per_rank[i] = (rank_idx == i).sum()
        token_sel = (rank_idx == i).max(dim=-1)[0]
        count = token_sel.sum().item()
        tokens = torch.sort(token_sel.to(torch.int), descending=True)[1]
        tokens[:count] = torch.sort(tokens[:count])[0]
        token_idx_in_rank[i][tokens[:count]] = torch.arange(
            count, dtype=torch.long, device="npu"
        )
    token_idx_in_rank = token_idx_in_rank.T.contiguous().to(torch.int)
    is_token_in_rank = (token_idx_in_rank >= 0).to(torch.int)
    gbl_num_tokens_per_rank = num_tokens_per_rank.clone()
    dist.all_reduce(gbl_num_tokens_per_rank, group=group)

    t = bench(lambda: buffer.get_dispatch_layout(topk_idx, num_experts))[0]
    logging.info(f"[layout] Kernel performance: {t * 1000:.3f} ms")
    dist.barrier()
    time.sleep(1)

    return_values = buffer.get_dispatch_layout(topk_idx, num_experts)
    (
        ref_num_tokens_per_rank,
        _,
        ref_num_tokens_per_expert,
        ref_is_token_in_rank,
        _,
    ) = return_values
    try:
        assert torch.allclose(
            ref_num_tokens_per_rank, num_tokens_per_rank
        ), (
            f"Assertion num_tokens_per_rank failed on rank {rank}: "
            f"Expected {num_tokens_per_rank}, Actual {ref_num_tokens_per_rank}"
        )
        assert torch.allclose(
            ref_num_tokens_per_expert, num_tokens_per_expert
        ), (
            f"Assertion num_tokens_per_expert failed on rank {rank}: "
            f"Expected {num_tokens_per_expert}, Actual {ref_num_tokens_per_expert}"
        )
        assert torch.allclose(
            ref_is_token_in_rank, is_token_in_rank
        ), (
            f"Assertion is_token_in_rank failed on rank {rank}: "
            f"Expected {is_token_in_rank}, Actual {ref_is_token_in_rank}"
        )
    except AssertionError as e:
        logging.error("happen error: %s", e)
        raise

    # Config
    buffer_size = 256
    config = Config(24, 8, buffer_size)

    # Random data
    x = torch.ones((num_tokens, hidden), dtype=torch.bfloat16, device="npu") * rank
    x_pure_rand = torch.randn(
        (num_tokens, hidden), dtype=torch.bfloat16, device="npu"
    )
    topk_weights = (
        torch.ones((num_tokens, num_topk), dtype=torch.float32, device="npu") * rank
    )
    topk_weights_pure_rand = torch.randn(
        (num_tokens, num_topk), dtype=torch.float32, device="npu"
    )

    def get_num_tokens_per_expert_list(rank: int):
        local_expert_token = gbl_num_tokens_per_expert.view(num_ranks, -1)[rank]
        if expert_token_nums_type == 0:
            # 计算前缀和并转为 list
            local_expert_token_list = local_expert_token.cumsum(dim=0).tolist()
        else:
            local_expert_token_list = local_expert_token.tolist()
        return local_expert_token_list

    def test_correctness():
        for current_x in filter(lambda elem: elem is not None, (x, x_pure_rand)):
            if local_rank == 0:
                logging.info(
                    f'[testing] Running with {"FP8" if isinstance(current_x, tuple) else "BF16"}, '
                    f'with top-k {num_topk} ...'
                )
            # Test dispatch
            dispatch_args = {
                "x": current_x,
                "num_tokens_per_rank": ref_num_tokens_per_rank,
                "is_token_in_rank": ref_is_token_in_rank,
                "num_tokens_per_expert": ref_num_tokens_per_expert,
                "config": config,
                "topk_idx": topk_idx,
                "topk_weights": (
                    topk_weights_pure_rand if current_x is x_pure_rand else topk_weights
                ),
            }

            (
                recv_x,
                recv_topk_idx,
                recv_topk_weights,
                recv_num_tokens_per_expert_list,
                handle,
                event,
            ) = buffer.dispatch(**dispatch_args)
            recv_x = (
                per_token_cast_back(*recv_x) if isinstance(recv_x, tuple) else recv_x
            )

            # Checks notify output
            local_expert_token_list = get_num_tokens_per_expert_list(rank)
            assert local_expert_token_list == recv_num_tokens_per_expert_list

            # Test combine
            combine_args = {
                "x": recv_x,
                "handle": handle,
                "config": config,
                "async_finish": False,
                "topk_weights": handle[2],
            }
            combined_x, combined_topk_weights, event = buffer.combine(**combine_args)

            check_x = combined_x.float()
            ref_x = x_pure_rand if current_x is x_pure_rand else x
            ref_x_compute = (
                ref_x
                * handle[2].masked_fill(topk_idx == -1, 0).sum(dim=1).view(-1, 1)
            )
            diff = calc_diff(check_x, ref_x_compute)
            if diff > 5e-5 or math.isnan(diff):
                logging.error(
                    f"{rank=} {diff=} {check_x[:,:10]=} {ref_x_compute[:, :10]=}"
                )
            assert (diff < 5e-5 or math.isnan(diff))

            if local_rank == 0:
                logging.info("test passed")

    def test_tuning():
        config = Config(24, 8, buffer_size)

        current_x = x
        local_expert_token_list = get_num_tokens_per_expert_list(rank)
        real_recv_tokens = sum(local_expert_token_list)
        dispatch_bf16_recv_bytes = real_recv_tokens * hidden * 2
        combine_bf16_send_bytes = dispatch_bf16_recv_bytes

        # tuning dispatch
        recv_bytes = (
            (dispatch_bf16_recv_bytes / 2) if use_quant else dispatch_bf16_recv_bytes
        )
        tune_dispatch_args = {
            "x": current_x,
            "config": config,
            "num_tokens_per_rank": ref_num_tokens_per_rank,
            "is_token_in_rank": ref_is_token_in_rank,
            "num_tokens_per_expert": ref_num_tokens_per_expert,
            "topk_idx": topk_idx,
            "topk_weights": topk_weights,
        }
        dispatch_t = bench(lambda: buffer.dispatch(**tune_dispatch_args))[0]
        logging.info(
            f'[tuning] Dispatch ({"FP8" if isinstance(current_x, tuple) else "BF16"}) '
            f'{recv_bytes / 1e9 / dispatch_t:.2f} GB/s (HCCS), '
            f'avg_t: {dispatch_t * 1e6:.2f} us'
        )

        dispatch_args = {
            "x": x,
            "config": config,
            "num_tokens_per_rank": ref_num_tokens_per_rank,
            "is_token_in_rank": ref_is_token_in_rank,
            "num_tokens_per_expert": ref_num_tokens_per_expert,
            "topk_idx": topk_idx,
            "topk_weights": topk_weights,
        }
        recv_x, _, _, _, handle, _ = buffer.dispatch(**dispatch_args)
        recv_x = (
            per_token_cast_back(*recv_x) if isinstance(recv_x, tuple) else recv_x
        )
        # Tune combine performance
        tune_combine_args = {
            "x": recv_x,
            "handle": handle,
            "config": config,
            "async_finish": False,
            "topk_weights": handle[2],
        }
        combine_t = bench(lambda: buffer.combine(**tune_combine_args))[0]
        logging.info(
            f"[tuning] Combine {combine_bf16_send_bytes / 1e9 / combine_t:.2f} GB/s (HCCS), "
            f"avg_t: {combine_t * 1e6:.2f} us"
        )

        calculate_avg_stats(
            dispatch_t=dispatch_t,
            num_dispatch_comm_bytes=recv_bytes,
            combine_t=combine_t,
            num_combine_comm_bytes=combine_bf16_send_bytes,
            rank=rank,
            num_ranks=num_ranks,
            root_rank=0,
        )

    test_correctness()
    test_tuning()


# noinspection PyUnboundLocalVariable,PyShadowingNames
def test_loop(local_rank: int, num_local_ranks: int, args: argparse.Namespace):
    rank, num_ranks, group = init_dist(local_rank, num_local_ranks)
    logging.info(f'[group] {group.rank()=} {group.size()=}')

    logging.info(f"[Rank {rank} | Local rank {local_rank}] Initializing buffer...")
    buffer = zbal.Buffer(
        group,
        int(2e9),
        0,
        low_latency_mode=False,
    )
    logging.info(f"[Rank {rank}] Buffer created OK.")
    torch.manual_seed(rank)

    test_main(args, num_local_ranks, local_rank, num_ranks, rank, buffer, group)

    dist.barrier()
    dist.destroy_process_group()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Test intranode EP kernels")
    parser.add_argument(
        "--num-processes",
        type=int,
        default=16,
        help="Number of processes to spawn (default: 16)",
    )
    parser.add_argument(
        "--num-tokens",
        type=int,
        default=1024,
        help="Number of tokens (default: 4096)",
    )
    parser.add_argument(
        "--hidden",
        type=int,
        default=7168,
        help="Hidden dimension size (default: 7168)",
    )
    parser.add_argument(
        "--num-topk",
        type=int,
        default=8,
        help="Number of top-k experts (default: 8)",
    )
    parser.add_argument(
        "--num-experts",
        type=int,
        default=256,
        help="Number of experts (default: 256)",
    )
    args = parser.parse_args()

    num_processes = args.num_processes
    torch.multiprocessing.spawn(
        test_loop, args=(num_processes, args), nprocs=num_processes
    )
