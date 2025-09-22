#!/usr/bin/env python3.10
#  Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
import os
from time import sleep
from typing import Callable, Dict, List
import torch
import torch_npu

from memcache import DistributedObjectStore

process_count: int = 8
one_batch_count: int = 10
call_count: int = 1024
size1 = [64 * 1024 for _ in range(64)]
size2 = [64 * 1024 for _ in range(64)]
size = size1 + size2
key_prefix: str = "key_"
IS_2D = False


def tensor_sum(tensor, sizes: List[int] = None):
    if tensor is None:
        return 0
    if sizes is None:
        return tensor.sum().item()
    return sum(layer[:size].sum().item() for layer, size in zip(tensor, sizes))


def malloc_tensor(layer_num: int = 1, mini_block_size: int = 1024, device='cpu'):
    if device not in ('cpu', 'npu'):
        raise RuntimeError(f"invalid device: {device}")
    if mini_block_size <= 0:
        return None

    raw_blocks = torch.randint(
        low=0, high=256,
        size=(layer_num, mini_block_size),
        dtype=torch.uint8,
        device=torch.device(device)
    )
    torch_npu.npu.current_stream().synchronize()
    return raw_blocks


def set_device(device_id):
    import acl
    acl.init()
    ret = acl.rt.set_device(device_id)
    if ret != 0:
        raise RuntimeError("acl set device failed")


def batch_malloc_tensor(count: int, tesor_size: list, need_2d: bool = False):
    layer = len(size)
    tensors = []
    buffs = []
    sizes = []
    for _ in range(count):
        tensor = malloc_tensor(layer_num=layer, mini_block_size=max(tesor_size, default=0), device='npu')
        tensors.append(tensor)
        buff = [] if tensor is None else [layer.data_ptr() for layer in tensor]
        if not need_2d:
            buff[0], buff[-1] = buff[-1], buff[0]
        buffs.append(buff)
        sizes.append(size)
    return tensors, buffs, sizes


def write_worker(device_id: int):
    set_device(device_id)
    print(f"npu:{device_id} 开始，PID: {os.getpid()}")
    tensors, buffs, sizes = batch_malloc_tensor(one_batch_count, size, IS_2D)
    store = DistributedObjectStore()
    res = store.init(device_id)
    if res != 0:
        raise f"Failed to start pid:{os.getpid()} deviceId:{device_id}"
    print(f"==== Success to init device:{device_id}")
    for i in range(one_batch_count):
        store.register_buffer(tensors[i].data_ptr(), max(size, default=0) * len(size))
    for i in range(call_count):
        keys = []
        for j in range(one_batch_count):
            key = key_prefix + str(device_id) + '_' + str(i) + '_' + str(j)
            keys.append(key)
        ret = store.batch_put_from_layers(keys, buffs, sizes, 0)
        for j in range(one_batch_count):
            print(f"==== key({keys[j]}) res({ret[j]}) sum({tensor_sum(tensors[j], sizes[j])})")
    print(f"===== npu:{device_id} 结束 wait......")
    sleep(30 * 60)


def read_worker(device_id: int):
    set_device(device_id)
    print(f"npu:{device_id} 开始，PID: {os.getpid()}")
    tensors, buffs, sizes = batch_malloc_tensor(one_batch_count, size, IS_2D)
    store = DistributedObjectStore()
    res = store.init(device_id)
    if res != 0:
        raise f"Failed to start pid:{os.getpid()} deviceId:{device_id}"
    print("==== Success to init device:{device_id}")
    for i in range(one_batch_count):
        store.register_buffer(tensors[i].data_ptr(), max(size, default=0) * len(size))
    for i in range(call_count):
        keys = []
        for j in range(one_batch_count):
            key = key_prefix + str(device_id) + '_' + str(i) + '_' + str(j)
            keys.append(key)
        ret = store.batch_get_into_layers(keys, buffs, sizes, 1)
        for j in range(one_batch_count):
            print(f"==== key({keys[j]}) res({ret[j]}) sum({tensor_sum(tensors[j], sizes[j])})")
    print(f"==== npu:{device_id} 结束 wait......")
    sleep(30 * 60)
