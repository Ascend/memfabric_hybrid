
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

import math
from typing import List, Tuple, Dict, Any
import torch


def pack_tensors(tensor_list: List[torch.Tensor],
                 target_dtype: torch.dtype = torch.int8) -> Tuple[torch.Tensor, Dict[str, Any]]:
    """
    将任意张量列表打包为指定数据类型的单个张量

    Args:
        tensor_list: 要传输的张量列表
        target_dtype: 传输时使用的目标数据类型

    Returns:
        packed_tensor: 打包后的张量
        metadata: 恢复张量所需的元数据
    """
    if not tensor_list:
        raise ValueError("张量列表不能为空")

    # 计算总字节数和收集元数据
    total_bytes = 0
    metadata = {
        'original_shapes': [],
        'original_dtypes': [],
        'element_counts': [],  # 每个张量在目标类型中的元素数量
        'original_element_counts': [],  # 原始张量的元素数量
        'target_dtype': target_dtype
    }

    device = tensor_list[0].device
    target_element_size = torch.tensor(1, dtype=target_dtype).element_size()

    for i, tensor in enumerate(tensor_list):
        if tensor.device != device:
            raise ValueError("所有张量必须在同一设备上")

        if not tensor.is_contiguous():
            tensor_list[i] = tensor.contiguous()

        tensor_bytes = tensor.numel() * tensor.element_size()
        tensor_elements = math.ceil(tensor_bytes / target_element_size)
        original_num_elements = tensor.numel()

        metadata['original_shapes'].append(tensor.shape)
        metadata['original_dtypes'].append(tensor.dtype)
        metadata['element_counts'].append(tensor_elements)
        metadata['original_element_counts'].append(original_num_elements)
        total_bytes += tensor_bytes

    # 计算总元素数量
    total_elements = sum(metadata['element_counts'])

    # 创建打包张量
    packed_tensor = torch.empty(total_elements, dtype=target_dtype, device=device)

    # 直接使用目标数据类型复制数据
    global pack
    current_offset = 0
    for i, tensor in enumerate(tensor_list):
        element_count = metadata['element_counts'][i]

        # 将源张量转换为目标数据类型视图
        source_view = tensor.view(target_dtype)

        # 直接复制到打包张量
        packed_tensor[current_offset:current_offset + element_count].copy_(
            source_view.flatten()[:element_count]  # source_view可能比element_count长（由于向上取整）
        )

        current_offset += element_count

    return packed_tensor, metadata


def unpack_tensors(packed_tensor: torch.Tensor,
                   metadata: Dict[str, Any]) -> List[torch.Tensor]:
    """
    从打包张量中恢复原始张量列表

    Args:
        packed_tensor: 打包后的张量
        metadata: 打包时保存的元数据

    Returns:
        tensor_list: 恢复后的原始张量列表
    """
    restored_tensors = []
    current_offset = 0
    global unpack
    for shape, original_dtype, element_count, original_num_elements in zip(
        metadata['original_shapes'],
        metadata['original_dtypes'],
        metadata['element_counts'],
        metadata['original_element_counts']
    ):
        # 提取数据段
        data_slice = packed_tensor[current_offset:current_offset + element_count]

        # 恢复为原始数据类型
        restored_tensor = data_slice.view(original_dtype)

        # 只在需要时进行截断
        if restored_tensor.numel() > original_num_elements:
            # 由于向上取整有多余元素，需要截断
            restored_tensor = restored_tensor[:original_num_elements]

        # 调整形状
        restored_tensor = restored_tensor.reshape(shape)

        restored_tensors.append(restored_tensor)
        current_offset += element_count

    return restored_tensors
