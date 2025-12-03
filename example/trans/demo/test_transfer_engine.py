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
from mf_adapter import TransferEngine, create_config_store, set_log_level, set_conf_store_tls


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
    total_buffer = torch.zeros(
        (10, 50, 40, 20, 60),
        dtype=torch.float16,
        device='npu',
    )
    total_bytes = total_buffer.element_size() * total_buffer.numel()
    
    engine.register_memory(total_buffer.data_ptr(), total_bytes)\
    # 等待并注册内存
    time.sleep(5)
    print(f'register address={hex(total_buffer.data_ptr())}')
    print("register success.")
    
    # 等待完成并读出处理数据
    while True:
        time.sleep(10)
        print("-----------------begin read buffer info----------------")
        for i in range(10):
            print(f"get buffer success {i}, {torch.sum(total_buffer[i])=}")


def run_prefill_role(engine, args, unique_id):
    # 创建缓冲区
    total_buffer = torch.randn(
        (10, 50, 40, 20, 60),
        dtype=torch.float16,
        device='npu',
    )
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

    # 传输数据
    for i in range(10):
        buffer = total_buffer[i]
        print(f'buffer address={hex(buffer.data_ptr())}')
        print(f'buffer sum={torch.sum(buffer)}')
        
        total_bytes = buffer.element_size() * buffer.numel()
        print(f"total_bytes={total_bytes}")
        
        engine.register_memory(buffer.data_ptr(), total_bytes)
        # 等待注册完成
        time.sleep(10)
        peer_address = buffer.data_ptr()  # 对端目的地址信息和buffer应该相同
        # 同步写入数据
        engine.transfer_sync_write(args.dst_unique_id, buffer.data_ptr(), peer_address, total_bytes)
        print(f"write success peer_address={hex(peer_address)}")
    
    # 等待解码完成
    while True:
        time.sleep(20)
        print("wait decode read finish")


if __name__ == "__main__":
    main()