
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

import re
import os
from typing import Optional

import torch


def get_prefix_name(name):
    if re.search(r'\d+\.(.*)', name) is not None:
        prefix_name = re.search(r'\d+\.(.*)', name).group(1)
    else:
        prefix_name = name.split('.')[-2:][0] 
    return prefix_name


def generate_transfer_device_mesh(
    train_tp_world_size, 
    train_pp_world_size, 
    infer_tp_world_size, 
    infer_pp_world_size, 
    tp_partition, 
    pp_stage,
    world_size=None,
    ):
    if world_size is None:
        world_size = torch.distributed.get_world_size()
    num_ranks_in_pp = world_size // train_pp_world_size
    current_device_mesh = []

    if tp_partition:
        for i in range(pp_stage * num_ranks_in_pp, (pp_stage + 1) * num_ranks_in_pp,
                       train_tp_world_size):
            current_device_mesh.append(list(range(i, i + train_tp_world_size)))
    else:
        for i in range(pp_stage * num_ranks_in_pp, (pp_stage + 1) * num_ranks_in_pp):
            current_device_mesh.append([i])

    if infer_pp_world_size != 1:
        raise RuntimeError(f"[TensorRTL][ERROR] Not support infer pp.")
    update_device_mesh = []

    if tp_partition:
        for i in range(0, world_size, infer_tp_world_size):
            update_device_mesh.append(list(range(i, i + infer_tp_world_size)))
    else:
        for i in range(0, world_size):
            update_device_mesh.append([i])

    return current_device_mesh, update_device_mesh


def generate_transfer_ep_device_mesh(
    num_experts, 
    train_ep_world_size, 
    train_etp, 
    train_pp_world_size, 
    infer_ep_world_size, 
    infer_etp, 
    infer_pp_world_size, 
    pp_stage,
    world_size=None,
    combined_tensor=False
):
    if world_size is None:
        world_size = torch.distributed.get_world_size()
    num_ranks_in_pp = world_size // train_pp_world_size
    experts_per_stage = (num_experts // train_ep_world_size) if not combined_tensor else 1  # 合并之后，只有一个合并后的tensor
    current_device_mesh = []

    for i in range(pp_stage * num_ranks_in_pp, (pp_stage + 1) * num_ranks_in_pp,
                       train_etp):
        for _ in range(experts_per_stage):
            current_device_mesh.append(list(range(i, i + train_etp)))

    if infer_pp_world_size != 1:
        raise RuntimeError(f"[TensorRTL][ERROR] Not support infer pp.")
    update_device_mesh = []

    infer_experts_per_stage = (num_experts // infer_ep_world_size) if not combined_tensor else 1  # 合并之后，只有一个合并后的tensor
    for i in range(0, world_size, infer_etp):
        for _ in range(infer_experts_per_stage):
            update_device_mesh.append(list(range(i, i + infer_etp)))
    return current_device_mesh, update_device_mesh



def qkv_from_megatron_to_sglang(qkv_proj,
                                num_query_groups,
                                num_attention_heads,
                                q_head_dim,
                                k_head_dim,
                                v_head_dim):
    # qkv在megatron侧以头的形式存在，[q1,k1,v1,q2,k2,v2,...]所以在tp变幻时不需要切分重组，但是在sglang侧以qkv的形式存在，如[q1,q2,q3,k1,k2,k3,v1,v2,v3]
    # 所以在获取qkv后需要转换成sglang格式

    one_head_num_querys = num_attention_heads // num_query_groups

    q_size = q_head_dim * one_head_num_querys
    k_size = k_head_dim
    v_size = v_head_dim
    per_head = q_size + k_size + v_size
    head_list = torch.split(qkv_proj, per_head, dim=0)
    q, k, v = [], [], []
    for head in head_list:
        qkv = torch.split(head, [q_size, k_size, v_size], dim=0)
        q.append(qkv[0])
        k.append(qkv[1])
        v.append(qkv[2])
    qkv_proj = torch.cat(q + k + v, dim=0)
    return qkv_proj


def get_expert_param_name_and_idx(name):
    expert_name = re.sub(r'\d+$', '', name)
    expert_idx = int(name[len(expert_name):])
    return expert_name, expert_idx


# 硬编码映射：NPU卡逻辑序号 -> CPU 核心范围
DIE_TO_CPUS = {
    0: "0-79", 1: "0-79",
    2: "80-159", 3: "80-159",
    4: "160-239", 5: "160-239",
    6: "240-319", 7: "240-319",
    8: "320-399", 9: "320-399",
    10: "400-479", 11: "400-479",
    12: "480-559", 13: "480-559",
    14: "560-639", 15: "560-639",
}


def parse_cpu_range(cpus_str: str) -> list[int]:
    result = []
    for part in cpus_str.split(','):
        if '-' in part:
            lo, hi = part.split('-')
            result.extend(range(int(lo), int(hi) + 1))
        else:
            result.append(int(part))
    return result


def bind_actor_cpu(visible_offset: Optional[int] = None):
    """
    将当前进程绑定到与该 actor 对应的 CPU 核心上。

    :param visible_offset: 当前进程在可见设备列表中的索引（从 0 开始）。
                           若为 None，则自动从环境变量 `LOCAL_RANK` 或 `RAY_LOCAL_RANK` 获取。
                           最终通过 `ASCEND_RT_VISIBLE_DEVICES[visible_offset]` 得到物理 die ID。
    """

    if os.getenv("TENSOR_RTL") != "1":
        return

    # 1. 获取可见设备列表
    visible = os.environ.get("ASCEND_RT_VISIBLE_DEVICES")

    die_list = [int(x) for x in visible.split(",") if x.strip()]

    # 2. 确定 visible_offset
    if visible_offset is None:
        # 自动推断：尝试 LOCAL_RANK，然后是 RAY_LOCAL_RANK
        offset = int(os.environ.get("LOCAL_RANK", -1))
        if offset == -1:
            offset = int(os.environ.get("RAY_LOCAL_RANK", -1))
        # 如果列表只有一个元素，即使没有 LOCAL_RANK 也可以安全地取 0
        if offset == -1 and len(die_list) == 1:
            offset = 0
    else:
        offset = visible_offset

    # 3. 获取物理 die ID
    phys_die = die_list[offset]

    cpus_str = DIE_TO_CPUS[phys_die]
    cpu_list = parse_cpu_range(cpus_str)

    # 5. 执行绑核
    os.sched_setaffinity(0, cpu_list)
