#!/usr/bin/env python3.10
#  Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
import os
from time import sleep
from typing import List
import torch
import torch_npu

from memcache import DistributedObjectStore

process_count: int = 8
one_batch_count: int = 32
call_count: int = 64
test_times: int = 8
size1 = [128 * 1024 for _ in range(61)]
size2 = [16 * 1024 for _ in range(61)]
block_size = [item for pair in zip(size1, size2) for item in pair]
upper_layer: int = len(block_size)
key_prefix: str = "key_"
IS_2D = False


def set_device(device_id):
    import acl
    acl.init()
    ret = acl.rt.set_device(device_id)
    if ret != 0:
        raise RuntimeError("acl set device failed")


def tensor_sum(tensor: List[torch.Tensor], sizes: List[int] = None):
    if tensor is None:
        return 0
    if sizes is None:
        return sum(layer.sum().item() for layer in tensor)
    return sum(layer[:size].sum().item() for layer, size in zip(tensor, sizes))


def allocate_aligned_tensor(shape, dtype=torch.float32, alignment=2*1024*1024):
    num_elements = torch.prod(torch.tensor(shape)).item()
    element_size = torch.finfo(dtype).bits // 8 if dtype.is_floating_point else torch.iinfo(dtype).bits // 8
    total_bytes = num_elements * element_size

    padding = alignment - 1
    buffer = torch.empty(total_bytes + padding, dtype=dtype, device='npu')

    address = buffer.data_ptr()
    aligned_address = (address + alignment - 1) & ~(alignment - 1)
    offset = (aligned_address - address) // element_size

    aligned_tensor = buffer[offset:offset + num_elements].view(*shape)
    print(f"Aligned tensor address: {aligned_tensor.data_ptr():x}")
    return aligned_tensor


def malloc_npu_blocks(min_block_size: int, layer_num: int, block_num: int):
    raw_blocks = allocate_aligned_tensor((layer_num, block_num, min_block_size), torch.uint8)
    torch_npu.npu.current_stream().synchronize()
    return raw_blocks


def get_col_tensors_by_index(tensors, layer_num, block_index):
    block_tensor = []
    for li in range(layer_num):
        block_tensor.append(tensors[li][block_index])
    return block_tensor


def get_col_tensors_ptr_by_index(tensors, layer_num, block_index):
    block_ptrs = []
    for li in range(layer_num):
        block_ptrs.append(tensors[li][block_index].data_ptr())
    return block_ptrs


def write_worker(device: int):
    device_id = device
    set_device(device_id)
    print(f"npu:{device_id} 开始，PID: {os.getpid()}")
    tensor1 = malloc_npu_blocks(max(size1, default=0), len(size1), one_batch_count)
    tensor2 = malloc_npu_blocks(max(size2, default=0), len(size2), one_batch_count)
    register_buffs = [item for pair in zip(get_col_tensors_ptr_by_index(tensor1, len(size1), 0),
                                           get_col_tensors_ptr_by_index(tensor2, len(size2), 0)) for item in pair]
    register_size = [mini_block_size * one_batch_count for mini_block_size in block_size]
    store = DistributedObjectStore()
    print(f"==== Start to init memcache device:{device_id}")
    res = store.init(device_id)
    if res != 0:
        raise f"Failed to start pid:{os.getpid()} deviceId:{device_id}"
    print(f"==== Success to init device:{device_id}")
    store.register_layer_buffer(register_buffs, register_size, one_batch_count)
    for i in range(call_count):
        keys = []
        buffs = []
        sizes = []
        for j in range(one_batch_count):
            key = key_prefix + str(device) + '_' + str(i) + '_' + str(j)
            keys.append(key)
            block_buffs = [item for pair in zip(get_col_tensors_ptr_by_index(tensor1, len(size1), j),
                                                get_col_tensors_ptr_by_index(tensor2, len(size2), j)) for item in pair]
            buffs.append(block_buffs)
            sizes.append(block_size)
        ret = store.batch_put_from_layers(keys, buffs, sizes, 0)
        for j in range(one_batch_count):
            block_tensors = [item for pair in zip(get_col_tensors_by_index(tensor1, len(size1), j),
                                                  get_col_tensors_by_index(tensor2, len(size2), j)) for item in pair]
            print(f"==== key({keys[j]}) res({ret[j]})) sum({tensor_sum(block_tensors, block_size)})")
    print(f"===== npu:{device_id} 结束 wait......")
    sleep(30 * 60)


def read_worker(device: int):
    device_id = device
    set_device(device_id)
    print(f"npu:{device_id} 开始，PID: {os.getpid()}")
    tensor1 = malloc_npu_blocks(max(size1, default=0), len(size1), one_batch_count)
    tensor2 = malloc_npu_blocks(max(size2, default=0), len(size2), one_batch_count)
    register_buffs = [item for pair in zip(get_col_tensors_ptr_by_index(tensor1, len(size1), 0),
                                           get_col_tensors_ptr_by_index(tensor2, len(size2), 0)) for item in pair]
    register_size = [mini_block_size * one_batch_count for mini_block_size in block_size]
    store = DistributedObjectStore()
    print(f"==== Start to init memcache device:{device_id}")
    res = store.init(device_id)
    if res != 0:
        raise f"Failed to start pid:{os.getpid()} deviceId:{device_id}"
    print(f"==== Success to init device:{device_id}")
    store.register_layer_buffer(register_buffs, register_size, one_batch_count)
    for i in range(call_count):
        keys = []
        buffs = []
        sizes = []
        for j in range(one_batch_count):
            key = key_prefix + str(device) + '_' + str(i) + '_' + str(j)
            keys.append(key)
            block_buffs = [item for pair in zip(get_col_tensors_ptr_by_index(tensor1, len(size1), j),
                                                get_col_tensors_ptr_by_index(tensor2, len(size2), j)) for item in pair]
            buffs.append(block_buffs)
            sizes.append(block_size)
        ret = store.batch_get_into_layers(keys, buffs, sizes, 1)
        for j in range(one_batch_count):
            block_tensors = [item for pair in zip(get_col_tensors_by_index(tensor1, len(size1), j),
                                                  get_col_tensors_by_index(tensor2, len(size2), j)) for item in pair]
            print(f"==== key({keys[j]}) res({ret[j]})) sum({tensor_sum(block_tensors, block_size)})")
    print(f"===== npu:{device_id} 结束 wait......")
    sleep(30 * 60)
