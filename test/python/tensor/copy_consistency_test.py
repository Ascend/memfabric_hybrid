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

import logging
import time
import os
import re
import multiprocessing as mp
import torch
from torch_npu import npu
import acl


class TensorCopyTest:
    def __init__(
        self,
        /,
        task_id: int,
        device_id: int,
        copy_size: int,
        repeat: int,
        result_pipe: mp.Queue,
        sync_after_create=False,
        use_same_stream=False,
    ) -> None:
        self.task_id = task_id
        self.device_id = device_id
        self.copy_size = copy_size
        self.repeat = repeat
        self.sync_after_create = sync_after_create
        self.use_same_stream = use_same_stream
        self.result_pipe = result_pipe

    @staticmethod
    def tensor_sum(tensor):
        if tensor is None:
            return 0
        ret = torch.sum(tensor, dtype=torch.float32)
        return ret.item()

    def create_tensor(self) -> torch.Tensor:
        tensor = torch.rand(
            size=(1, 1, self.copy_size // 2),
            dtype=torch.float16,
            device=torch.device("npu"),
        )
        if self.sync_after_create:
            npu.current_stream().synchronize()

        return tensor

    def copy_d2d(self, dst, src, stream):
        size = self.copy_size
        ret = acl.rt.memcpy_async(dst, size, src, size, 3, stream)
        if ret != 0:
            raise RuntimeError(f"memcpy_async failed, {ret=}")

        ret = acl.rt.synchronize_stream(stream)
        if ret != 0:
            raise RuntimeError(f"synchronize_stream failed, {ret=}")

    def test_repeat(self, stream):
        count = self.repeat
        print(f"[task{self.task_id}] run test for {count} times...")
        mismatch = 0
        for _ in range(count):
            equals = self.test_once(stream)
            self.result_pipe.put(equals)
            if not equals:
                mismatch += 1
        self.result_pipe.put(None)
        return (count, mismatch)

    def test_once(self, stream) -> bool:
        ptr, ret = acl.rt.malloc(self.copy_size, 0)
        if ret != 0:
            raise RuntimeError(f"acl malloc failed, {ret=}")

        tensor1 = self.create_tensor()
        tensor2 = self.create_tensor()

        self.copy_d2d(ptr, tensor1.data_ptr(), stream)
        self.copy_d2d(tensor2.data_ptr(), ptr, stream)

        origin_value = self.tensor_sum(tensor1)
        copied_value = self.tensor_sum(tensor2)

        equals = origin_value == copied_value

        del tensor1
        del tensor2

        ret = acl.rt.free(ptr)
        if ret != 0:
            raise RuntimeError(f"acl free failed, {ret=}")

        return equals

    def run(self, begin_barrier, end_barrier):
        try:
            print(f"[task{self.task_id}] acl init...")
            ret = acl.init()
            if ret != 0:
                raise RuntimeError(f"acl init failed, {ret=}")
            print(f"[task{self.task_id}] acl set device({self.device_id})...")
            ret = acl.rt.set_device(self.device_id)

            if ret != 0:
                raise RuntimeError(f"acl set device failed, {ret=}")
            print(f"[task{self.task_id}] {npu.current_device()=}")
            print(f"[task{self.task_id}] {npu.default_stream().npu_stream=}")
            print(f"[task{self.task_id}] {npu.current_stream().npu_stream=}")
            if self.use_same_stream:
                stream = npu.current_stream().npu_stream
            else:
                current_device = npu.current_device()
                stream = npu.Stream(current_device).npu_stream
            print(f"[task{self.task_id}] test copy_async use {stream=}")

            begin_barrier.wait()
            (count, mismatch) = self.test_repeat(stream)
            end_barrier.wait()

            print(f"[task{self.task_id}] test result: {count=} {mismatch=}")
            time.sleep(0.1)
            print(f"[task{self.task_id}] acl reset device...")
            ret = acl.rt.reset_device(self.device_id)
            if ret != 0:
                raise RuntimeError(f"acl reset device failed, {ret=}")
            print(f"[task{self.task_id}] acl finalize...")
            ret = acl.finalize()
            if ret != 0:
                raise RuntimeError(f"acl finalize failed, {ret=}")

        except Exception as e:
            logging.error(f"[task{self.task_id}] test run failed: {e}")


def byte_size(size):
    """parse string argument with this type"""
    units = {"B": 1, "KB": 1024, "MB": 1024**2, "GB": 1024**3}
    size = size.upper().strip()
    pattern = r"^(\d+(?:\.\d+)?)([A-Z]+)?$"
    match = re.match(pattern, size)
    if match:
        number = match.group(1)
        unit = match.group(2)
        if unit in units:
            return int(float(number) * units[unit])
        elif not unit:
            return int(number)
        else:
            raise argparse.ArgumentTypeError(f"invalid byte size unit: {unit}")
    else:
        raise argparse.ArgumentTypeError(f"invalid byte size format: {size}")


def print_result(parallel, begin_barrier, end_barrier, queue: mp.Queue):
    interval = 0.1
    sz = os.get_terminal_size()
    print_width = ((sz.columns // 50) * 50) or 100
    line = 0
    last = time.monotonic()
    done = 0

    begin_barrier.wait()

    while True:
        equals = queue.get()
        if equals is None:
            done += 1
            if done == parallel:
                break
            else:
                continue
        flush = time.monotonic() - last > interval
        if equals:
            print(".", end="", flush=flush)
        else:
            print("!", end="", flush=flush)
        if flush:
            last = time.monotonic()
        line += 1
        if (line % print_width) == 0:
            print(flush=True)
            last = time.monotonic()
            line = 0
    if line > 0:
        print(flush=True)

    end_barrier.wait()


def main_process(args, task_id, begin_barrier, end_barrier, result_pipe):
    test = TensorCopyTest(
        task_id=task_id,
        device_id=args.device,
        copy_size=args.size,
        repeat=args.repeat,
        sync_after_create=args.sync_create,
        use_same_stream=args.same_stream,
        result_pipe=result_pipe,
    )
    test.run(begin_barrier, end_barrier)


if __name__ == "__main__":
    logging.basicConfig(
        level=logging.DEBUG,
        format=" %(process)d - %(asctime)s - %(levelname)s - %(message)s",
    )

    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--device",
        type=int,
        default=0,
        help="the device id of the NPU to be used for the test(default: %(default)s)",
    )
    parser.add_argument(
        "--repeat",
        type=int,
        default=1000,
        help="the number of times to repeat the test (default: %(default)s)",
    )
    parser.add_argument(
        "--parallel",
        type=int,
        default=4,
        help="number of processes to be executed in parallel (default: %(default)s)",
    )
    parser.add_argument(
        "--size",
        type=byte_size,
        default=2 * 1024 * 1024,
        help="the size of the memory to be copied in the test, supporting positive integers "
        + "without units or with the following units (B, MB, GB) (default: %(default)s)",
    )
    parser.add_argument(
        "--sync-create",
        action="store_true",
        help="whether to perform a synchronize_stream after creating the tensor (default: %(default)s)",
    )
    parser.add_argument(
        "--same-stream",
        action="store_true",
        help="whether to use the same stream for the copy operation as the one used for "
        + "creating the tensor (default: %(default)s)",
    )

    args = parser.parse_args()

    print(f"{args}")

    mp.set_start_method("spawn")

    begin_barrier = mp.Barrier(args.parallel + 1)
    end_barrier = mp.Barrier(args.parallel + 1)
    queue = mp.Queue(maxsize=args.parallel * args.repeat)
    plist = [
        mp.Process(
            target=main_process, args=(args, i, begin_barrier, end_barrier, queue)
        )
        for i in range(args.parallel)
    ]
    plist.append(
        mp.Process(
            target=print_result, args=(args.parallel, begin_barrier, end_barrier, queue)
        )
    )
    [p.start() for p in plist]
    [p.join() for p in plist]