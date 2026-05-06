#!/usr/bin/env python3
# -*- coding:utf-8 -*-
"""
Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
MemFabric_Hybrid is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
"""

import sys
import time
import argparse
import threading
import itertools
import torch
import memfabric_hybrid as mf
from memfabric_hybrid import TransferEngine, create_config_store, set_log_level, shm, initialize, uninitialize


def calculate_rate(data_bytes, duration_us):
    """Calculate transfer rate."""
    gb_per_second = (data_bytes / (1000**3)) / (duration_us / 1000000.0)
    return f"{gb_per_second:.2f} GB/s"


def trans_perf_test(rank_id, store_url, num_threads=1, data_op_type=None, npu_id=0):
    """Execute transfer performance test."""
    engine = None
    shm_handle = None
    shm_initialized = False  # Track if shm was successfully initialized
    try:
        # 初始化MemFabric
        ret = initialize(0)
        if ret != 0:
            print(f"memfabric initialize failed, ret: {ret}")
            return ret

        # 设置日志级别
        set_log_level(2)  # WARNING level

        # Initialize based on role
        if rank_id == 0:
            # Sender (Prefill)
            unique_id = "127.0.0.1:10000"
            role = "Prefill"

            # Create config store for sender (Prefill role)
            print(f"[{rank_id}] Creating config store at {store_url}")
            ret = create_config_store(store_url)
            if ret != 0:
                print(f"create_config_store failed, ret: {ret}")
                return ret
            time.sleep(3)  # Wait for config store to be ready
        else:
            # Receiver (Decode)
            unique_id = "127.0.0.1:10001"
            role = "Decode"

        # Initialize TransferEngine with specified data operation type
        engine = TransferEngine()

        # Initialize based on role
        ret = engine.initialize(store_url, unique_id, role, npu_id, data_op_type)
        if ret != 0:
            print(f"TransferEngine initialize failed, ret: {ret}")
            return ret

        print(f"[{rank_id}] TransferEngine initialized with data type {data_op_type} on NPU {npu_id}")

        gva_size = 1024 * 1024 * 1024  # 1GB
        kb_size = 1024

        # 分配设备内存
        dev_tensor = torch.zeros((gva_size,), dtype=torch.uint8, device='npu')
        dev_addr = dev_tensor.data_ptr()
        print(f"[{rank_id}] malloc dev mem {hex(dev_addr)}")

        # Initialize SHM for address exchange
        shm_config = shm.ShmConfig()
        shm_config.start_store = False  # Don't start config store since it's already started

        ret = shm.initialize(store_url, 2, rank_id, npu_id, shm_config)  # 2 ranks
        if ret != 0:
            print(f"SHM initialize failed, ret: {ret}")
            return ret
        shm_initialized = True  # Mark that SHM was successfully initialized

        # Create SHM handle
        shm_handle = shm.create(0, 2, rank_id, gva_size, shm.ShmDataOpType.MTE, 0)
        if not shm_handle:
            print(f"SHM create failed for rank {rank_id}")
            return -1

        # Perform barrier to sync all ranks
        shm_handle.barrier()

        # Use all_gather to exchange device addresses
        local_addr_bytes = dev_addr.to_bytes(8, byteorder='little', signed=False)  # Convert pointer to bytes
        print(f"[{rank_id}] local dev addr {hex(dev_addr)} sending to all_gather")
        gathered_bytes = shm_handle.all_gather(local_addr_bytes)

        # all_gather returns a bytes object with length rank_size * 8 (2 * 8 = 16 bytes for 2 ranks)
        # Split into 8-byte addresses
        addr0_bytes = gathered_bytes[0:8]
        addr1_bytes = gathered_bytes[8:16]

        addr0 = int.from_bytes(addr0_bytes, byteorder='little', signed=False)
        addr1 = int.from_bytes(addr1_bytes, byteorder='little', signed=False)

        # Print all gathered addresses for debugging
        print(f"[{rank_id}] rank 0 addr: {hex(addr0)}")
        print(f"[{rank_id}] rank 1 addr: {hex(addr1)}")

        # Select the peer address based on our rank
        peer_addr = addr1 if rank_id == 0 else addr0

        if peer_addr == 0:
            print(f"[{rank_id}] Error: Got zero peer address")
            return -1

        print(f"[{rank_id}] got peer dev addr {hex(peer_addr)}")

        # Register memory after all_gather is complete
        ret = engine.register_memory(dev_addr, gva_size)
        if ret != 0:
            print(f"failed to register device memory, ret: {ret}")
            return ret

        # Synchronize both ranks after memory registration
        shm_handle.barrier()

        # Add delay to ensure memory registration propagates to remote side
        time.sleep(10)

        if rank_id == 0:
            block_iteration = 10
            base_block_size = 32 * 1024  # 32KB
            times = 100
            batch_size = 32
            dst_session_id = "127.0.0.1:10001"

            print(f"[{rank_id}] get dst session id {dst_session_id}")

            # 初始化预热数据
            print("Warmup Start")

            # Warmup transfer - use local dev_addr as source, peer_addr as destination
            ret = engine.transfer_sync_write(dst_session_id, dev_addr, peer_addr, base_block_size)
            if ret != 0:
                print(f"trans copy failed, ret: {ret} rank: {rank_id}")
                return ret
            print("Warmup End")

            test_title = "Trans Test Start"
            separator = "=" * 50
            print(f"{separator}{test_title}{separator}")

            for i in range(block_iteration):
                block_size = base_block_size * (1 << i)

                # Latency test
                start_time = time.time()

                # Create multiple threads for concurrent testing
                threads = []
                error_counter = itertools.count()

                def worker(tid):
                    for j in range(times):
                        ret = engine.transfer_sync_write(
                            dst_session_id,
                            dev_addr,  # local address as source
                            peer_addr,  # peer address as destination
                            block_size
                        )
                        if ret != 0:
                            next(error_counter)
                            return

                for t in range(num_threads):
                    thread = threading.Thread(target=worker, args=(t,))
                    threads.append(thread)
                    thread.start()

                for thread in threads:
                    thread.join()

                fail_count = next(error_counter)
                if fail_count > 0:
                    print("Latency test failed with errors")
                    return -1

                end_time = time.time()
                total_duration = (end_time - start_time) * 1000000  # Convert to microseconds
                avg_duration = total_duration / (num_threads * times)

                # BW test - batch transfer
                # Prepare data for batch transfer
                src_addrs = []
                dst_addrs = []
                sizes = []
                for k in range(batch_size):
                    l_addr = dev_addr + k * block_size  # Use local dev_addr area as source
                    r_addr = peer_addr + k * block_size  # Use peer address as destination

                    src_addrs.append(l_addr)  # Source addresses
                    dst_addrs.append(r_addr)  # Destination addresses
                    sizes.append(block_size)
                
                error_counter = itertools.count()
                threads.clear()

                def bw_worker(tid):
                    for j in range(times):
                        ret = engine.batch_transfer_sync_write(
                            dst_session_id,
                            src_addrs,
                            dst_addrs,
                            sizes
                        )
                        if ret != 0:
                            next(error_counter)
                            return

                start_time = time.time()

                for t in range(num_threads):
                    thread = threading.Thread(target=bw_worker, args=(t,))
                    threads.append(thread)
                    thread.start()

                for thread in threads:
                    thread.join()

                fail_count = next(error_counter)
                if fail_count > 0:
                    print("BW test failed with errors")
                    return -1

                end_time = time.time()
                total_bw_duration = (end_time - start_time) * 1000000  # Convert to microseconds
                total_bytes = num_threads * times * batch_size * block_size
                throughput = calculate_rate(total_bytes, total_bw_duration)

                print(f"Test completed: latency {avg_duration:.2f}us, block size {block_size // kb_size}KB, "
                      f"total threads={num_threads}, per-thread times={times}, "
                      f"aggregated throughput {throughput}")

            print(f"{separator}Test End{separator}")

        # Sync barrier before cleanup
        shm_handle.barrier()

    except Exception as e:
        print(f"Error in trans_perf_test: {e}")
        import traceback
        traceback.print_exc()
        return -1
    finally:
        # Clean up resources
        if shm_handle:
            shm_handle.destroy()

        if shm_initialized:
            shm.uninitialize(0)

        if engine:
            engine.destroy()
            engine.unInitialize()

    return 0


def main():
    """Main function."""
    parser = argparse.ArgumentParser(description='MemFabric Hybrid Transfer Performance Test')
    parser.add_argument('--rank-id', type=int, required=True, help='Current rank ID (required)')
    parser.add_argument('--store-url', type=str, default='tcp://127.0.0.1:12050',
                       help='Config store URL (default: tcp://127.0.0.1:12050)')
    parser.add_argument('--num-threads', type=int, default=1, help='Number of concurrent threads (default: 2)')
    parser.add_argument('--data-op-type', type=str, choices=['sdma', 'rdma'], default='sdma',
                       help='Data operation type: sdma or rdma (default: sdma)')
    parser.add_argument('--npu-id', type=int, default=0, help='NPU device ID (default: 0)')

    args = parser.parse_args()

    print(f"[TEST] input rank_id: {args.rank_id} store_url: {args.store_url} num_threads: {args.num_threads} "
          f"data_op_type: {args.data_op_type} npu_id: {args.npu_id}")

    # 设置NPU设备
    try:
        import torch_npu
        torch.npu.set_device(args.npu_id)
    except ImportError:
        print("torch_npu not found, please install it")
        return -1

    # Access TransDataOpType through TransferEngine class
    try:
        # Try to access the enums through the TransferEngine class
        trans_data_op_type = TransferEngine.TransDataOpType
    except AttributeError:
        # Fallback if the enum is not accessible
        print("Warning: Could not access TransDataOpType from TransferEngine")
        return -1

    # Map data type string to TransDataOpType
    if args.data_op_type.lower() == 'sdma':
        data_op_type = trans_data_op_type.SDMA
    elif args.data_op_type.lower() == 'rdma':
        data_op_type = trans_data_op_type.DEVICE_RDMA
    else:
        print(f"Invalid data operation type: {args.data_op_type}")
        return -1

    # Execute performance test
    ret = trans_perf_test(
        args.rank_id,
        args.store_url,
        args.num_threads,
        data_op_type,
        args.npu_id
    )

    return ret


if __name__ == "__main__":
    sys.exit(main())