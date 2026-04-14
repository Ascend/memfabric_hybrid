#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os.path
import argparse
import time
import torch
import torch_npu
import acl
import numpy as np
from memfabric_hybrid import TransferEngine, create_config_store, set_log_level, set_conf_store_tls

shape0 = 32
shape1 = 16
shape2 = 4096
tensor_shape = (shape0, shape1, shape2)


def main():
    # 解析命令行参数
    parser = argparse.ArgumentParser(description='Transfer Engine with role selection')
    parser.add_argument('--role', type=str, required=True, choices=['Decode', 'Prefill'],
                        help='Role of this instance: Decode or Prefill')
    parser.add_argument('--src-unique-id', type=str, help='Source unique ID')
    parser.add_argument('--store-url', type=str, required=True, help='URL for the store (e.g., tcp://xx.xx.xx.xx:xxxx)')
    parser.add_argument('--npu-id', type=int, default=0, help='NPU device ID')
    parser.add_argument('--dst-unique-id', type=str, help='Destination unique ID')
    parser.add_argument('--log-level', type=int, default=0, choices=[0, 1, 2, 3],
                        help='Log level: 0 debug, 1 info, 2 warn, 3 error')

    args = parser.parse_args()
    torch.npu.set_device(device=args.npu_id)
    # 初始化引擎
    engine = TransferEngine()
    set_log_level(args.log_level)
    set_conf_store_tls(False, "")
    # 根据角色执行不同的初始化和逻辑
    if args.role == "Decode":
        run_decode_role(engine, args, args.src_unique_id)
    elif args.role == "Prefill":
        if not args.dst_unique_id:
            raise ValueError("dst-unique-id is required for Prefill role")
        run_prefill_role(engine, args, args.src_unique_id)

    engine.destroy()


def run_decode_role(engine, args, unique_id):
    # 初始化引擎
    ret_value = engine.initialize(
        args.store_url,
        unique_id,
        args.role,
        args.npu_id,
    )

    if ret_value != 0:
        print("Ascend Transfer Engine initialization failed.")
        raise RuntimeError("Ascend Transfer Engine initialization failed.")

    print(f"AscendTransferEngine init success {args.store_url=} {unique_id=} {args.role=} {args.npu_id=}")

    # 创建缓冲区
    total_buffer = torch.zeros(tensor_shape, dtype=torch.float16, device='npu')
    scale_buffer = torch.zeros((shape0, shape1), dtype=torch.float, device='npu')
    quant = torch.zeros(tensor_shape, dtype=torch.int8, device='npu')
    torch.npu.synchronize(device=args.npu_id) # 同步,保证tensor初始化赋值完成

    total_bytes = total_buffer.element_size() * total_buffer.numel()
    ret_value = engine.register_memory(total_buffer.data_ptr(), total_bytes) # 等待并注册内存
    if ret_value != 0:
        raise RuntimeError("register tensor failed.")

    ret_value = engine.register_memory(scale_buffer.data_ptr(), shape0 * shape1 * 4) # 等待并注册内存
    if ret_value != 0:
        raise RuntimeError("register scale buffer failed.")

    time.sleep(5)
    print(f'[TEST] register address={hex(total_buffer.data_ptr())}, size={hex(total_bytes)}')
    print("[TEST] register success.")

    # 等待完成并处理数据
    time.sleep(10)
    for i in range(shape0):
        acl.rt.memcpy(quant[i].data_ptr(), shape1 * shape2, total_buffer[i].data_ptr(), shape1 * shape2, 3)
        torch.npu.synchronize(device=args.npu_id)

    print(f"[TEST] ============= ret_scale:{torch.sum(scale_buffer)}, ret_quant:{torch.sum(quant)}, shape:{quant.shape} dtype:{quant.dtype}")
    print("[TEST] wait prifill write finish, decode node exit")


def run_prefill_role(engine, args, unique_id):
    # 创建缓冲区
    total_buffer = torch.randn(tensor_shape, dtype=torch.float16, device='npu')
    scale_buffer = torch.zeros((shape0, shape1), dtype=torch.float, device='npu')
    # 创建配置存储
    create_config_store(args.store_url)
    time.sleep(3)

    # 初始化引擎
    ret_value = engine.initialize(
        args.store_url,
        unique_id,
        args.role,
        args.npu_id,
    )

    if ret_value != 0:
        print("Ascend Transfer Engine initialization failed.")
        raise RuntimeError("Ascend Transfer Engine initialization failed.")

    total_bytes = total_buffer.element_size() * total_buffer.numel()
    ret_value = engine.register_memory(total_buffer.data_ptr(), total_bytes) # 等待并注册内存
    if ret_value != 0:
        raise RuntimeError("register tensor failed.")
    time.sleep(10)
    print(f'[TEST] register address={hex(total_buffer.data_ptr())}, size={hex(total_bytes)}')
    print("[TEST] register success.")

    quant, scale = torch_npu.npu_dynamic_quant(total_buffer)
    # 同步,保证tensor初始化赋值完成
    torch.npu.synchronize(device=args.npu_id)
    # 传输数据
    buffer_list = []
    count_list = []
    scale_list = []
    for i in range(shape0):
        buffer_list.append(total_buffer[i].data_ptr())
        total_bytes = total_buffer[i].element_size() * total_buffer[i].numel()
        count_list.append(total_bytes)
        scale_list.append(scale_buffer[i].data_ptr())

    ret_value = engine.batch_transfer_write_with_quant(args.dst_unique_id, buffer_list, buffer_list, count_list, scale_list, [], shape2, 1)
    if ret_value != 0:
        raise RuntimeError("batch_transfer_write_with_quant failed.")
    print(f"[TEST] ============= write success, expect_scale={torch.sum(scale)}, expect_quant:{torch.sum(quant)}, shape:{quant.shape} dtype:{quant.dtype}")
    # 等待解码完成
    time.sleep(15)
    print("[TEST] prefill node exit")


if __name__ == "__main__":
    main()
