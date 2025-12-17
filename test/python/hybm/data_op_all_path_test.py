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

import argparse
import ctypes
from dataclasses import dataclass
from enum import Enum
import hashlib
import logging
import multiprocessing
import sys
from typing import Any, Dict, List

import memfabric_hybrid as mf_hybrid
import torch
from torch_npu import npu

KB = 1024
MB = 1024 * KB
GB = 1024 * MB

STORE_URL = "tcp://127.0.0.1:20028"
LOCAL_HBM_SIZE: int = 1 * GB
LOCAL_DRAM_SIZE: int = 8 * GB
DATA_OP_TYPE = memfabric_hybrid.bm.BmDataOpType.DEVICE_RDMA
COPY_SIZE: int = 64 * MB

DEVICES = [0, 1]
ERROR_ACTION = "bypass"
REPEAT_TIMES = 1


class MemType(Enum):
    LH = 1
    LD = 2
    GH = 3
    GD = 4

    def __repr__(self):
        return self.name  # 返回枚举成员的名称


class RankType(Enum):
    LOCAL = 1
    REMOTE = 2

    def __repr__(self):
        return self.name  # 返回枚举成员的名称


@dataclass(frozen=True)
class RankMemType:
    mem: MemType
    rank: RankType

    def __repr__(self):
        if self.rank == RankType.REMOTE:
            return f"R{self.mem.name}"
        return f"{self.mem.name}"


@dataclass(frozen=True)
class DataCopyPath:
    src: MemType
    dst: MemType


@dataclass(frozen=True)
class RankDataCopyPath:
    src: RankMemType
    dst: RankMemType

    def __repr__(self):
        return f"{self.src}->{self.dst}"


valid_paths = [
    DataCopyPath(MemType.LH, MemType.GH),
    DataCopyPath(MemType.LH, MemType.GD),
    DataCopyPath(MemType.LD, MemType.GH),
    DataCopyPath(MemType.LD, MemType.GD),
    DataCopyPath(MemType.GH, MemType.GH),
    DataCopyPath(MemType.GH, MemType.GD),
    DataCopyPath(MemType.GH, MemType.LH),
    DataCopyPath(MemType.GH, MemType.LD),
    DataCopyPath(MemType.GD, MemType.GH),
    DataCopyPath(MemType.GD, MemType.GD),
    DataCopyPath(MemType.GD, MemType.LH),
    DataCopyPath(MemType.GD, MemType.LD),
]

LOCAL_G = [
    RankMemType(MemType.GH, RankType.LOCAL),
    RankMemType(MemType.GD, RankType.LOCAL),
]


def is_valid_path(src: RankMemType, dst: RankMemType):
    if src in LOCAL_G and dst in LOCAL_G:
        return False
    if src.rank == RankType.REMOTE and dst.rank == RankType.REMOTE:
        return False
    return DataCopyPath(src.mem, dst.mem) in valid_paths


def filter_path(
    paths: List[RankDataCopyPath],
    /,
    src: RankMemType | None = None,
    dst: RankMemType | None = None,
):
    if src is not None:
        paths = [x for x in paths if x.src == src]
    if dst is not None:
        paths = [x for x in paths if x.dst == dst]
    return paths


def choose_path(
    paths: List[RankDataCopyPath],
    /,
    src: RankMemType | None = None,
    dst: RankMemType | None = None,
):
    temp = paths
    if src is not None:
        temp = [x for x in temp if x.src == src]
    if dst is not None:
        temp = [x for x in temp if x.dst == dst]
    if not temp:
        return None
    return temp[0]


class BigMemoryCopyPathResolver:
    mem_node = [
        RankMemType(MemType.LH, RankType.LOCAL),
        RankMemType(MemType.LD, RankType.LOCAL),
        RankMemType(MemType.GH, RankType.LOCAL),
        RankMemType(MemType.GD, RankType.LOCAL),
        RankMemType(MemType.GH, RankType.REMOTE),
        RankMemType(MemType.GD, RankType.REMOTE),
    ]
    first_node = mem_node[0]

    def __init__(self) -> None:
        pass

    def all_edges(self):
        edges: List[RankDataCopyPath] = []
        for lhs in self.mem_node:
            for rhs in self.mem_node:
                if is_valid_path(lhs, rhs):
                    edges.append(RankDataCopyPath(lhs, rhs))
        return edges

    def find_eulerian_path(self):
        start = self.first_node
        edges = self.all_edges()
        stack: List[RankMemType] = [start]
        path: List[RankMemType] = []
        while stack:
            top = stack[-1]
            edge = choose_path(edges, src=top)
            if edge:
                edges.remove(edge)
                stack.append(edge.dst)
            else:
                path.append(stack.pop())
        return path


class BigMemoryContext:
    def __init__(self, store_url: str, device_id: int, rank_id: int):
        self.__store_url = store_url
        self.__device_id = device_id
        self.__rank_id = rank_id
        self.__config = mf_hybrid.bm.BmConfig()
        self.__config.start_store = True
        self.__config.rank_id = self.__rank_id
        self.__config.auto_ranking = False
        self.__config.start_store_only = False
        self.__config.set_nic("tcp://0.0.0.0/0:10005")
        self.__handle = None

    def prepare(self):
        mf_hybrid.set_log_level(0)
        try:
            ret = mf_hybrid.initialize()
            if ret != 0:
                raise RuntimeError(f"{ret=}")
        except Exception as e:
            logging.error(f"mf_hybrid.initialize failed: {e}")
            return -1

        try:
            ret = mf_hybrid.bm.initialize(
                store_url=self.__store_url,
                world_size=2,
                device_id=self.__device_id,
                config=self.__config,
            )
            if ret != 0:
                raise RuntimeError(f"{ret=}")
        except Exception as e:
            logging.error(f"mf_hybrid.bm.initialize failed: {e}")
            mf_hybrid.uninitialize()
            return -2

        try:
            self.__handle = mf_hybrid.bm.create(
                id=0,
                local_dram_size=LOCAL_DRAM_SIZE,
                local_hbm_size=LOCAL_HBM_SIZE,
                data_op_type=DATA_OP_TYPE,
                flags=0,
            )
            res = self.__handle.join()
            logging.info(f"join in return {hex(res)}")
            npu.set_device(device=self.__device_id)
        except RuntimeError as e:
            logging.error(f"mf_hybrid.bm.create failed: {e}")
            mf_hybrid.bm.uninitialize()
            mf_hybrid.uninitialize()
            return -1

        return 0

    def get_handle(self) -> mf_hybrid.bm.BigMemory:
        return self.__handle


@dataclass(frozen=True)
class BigMemoryBlock:
    tp: RankMemType
    address: int
    size: int
    handle: Any

    def __repr__(self):
        return f"{hex(self.address)}({self.tp})"

    @property
    def kind(self):
        if self.tp.mem == MemType.LH:
            return "H"
        if self.tp.mem == MemType.LD:
            return "L"
        return "G"

    def get_checksum(self):
        if self.tp not in [
            RankMemType(MemType.LH, RankType.LOCAL),
            RankMemType(MemType.GH, RankType.LOCAL),
        ]:
            raise RuntimeError(f"get_checksum cannot run for {self.tp} block")
        buffer = ctypes.string_at(self.address, self.size)
        md5_hash = hashlib.md5()
        md5_hash.update(buffer)
        return md5_hash.hexdigest()


def create_tensor(device: str, size: int):
    tensor = torch.rand(
        size=(1, 1, size // 2),
        dtype=torch.float16,
        device=torch.device(device),
    )
    if device == "npu":
        npu.current_stream().synchronize()
    return tensor


def copy_kind(src: BigMemoryBlock, dst: BigMemoryBlock):
    kind = f"{src.kind}2{dst.kind}"
    if kind == "G2G":
        return mf_hybrid.bm.BmCopyType.G2G
    if kind == "L2G":
        return mf_hybrid.bm.BmCopyType.L2G
    if kind == "H2G":
        return mf_hybrid.bm.BmCopyType.H2G
    if kind == "G2L":
        return mf_hybrid.bm.BmCopyType.G2L
    if kind == "G2H":
        return mf_hybrid.bm.BmCopyType.G2H
    raise RuntimeError(f"invalid copy path: {src.tp} -> {dst.tp}")


def copy_data(
    src: BigMemoryBlock, dst: BigMemoryBlock, size: int, ctx: BigMemoryContext
):
    kind = copy_kind(src, dst)
    size = min(size, src.size, dst.size)
    logging.info(f"copy {size} bytes from {src} to {dst}, {kind}")
    ctx.get_handle().copy_data(src.address, dst.address, size, kind)


class BigMemoryTest:
    def __init__(self):
        self.test_rank_id = 0
        self.peer_rank_id = 1
        self.begin_barrier = multiprocessing.Barrier(2)
        self.end_barrier = multiprocessing.Barrier(2)
        pass

    def make_mem(
        self, tp: RankMemType, idx: int, ctx: BigMemoryContext
    ) -> BigMemoryBlock:
        if tp == RankMemType(MemType.LH, RankType.LOCAL):
            tensor = create_tensor("cpu", COPY_SIZE)
            return BigMemoryBlock(tp, tensor.data_ptr(), COPY_SIZE, tensor)

        if tp == RankMemType(MemType.LD, RankType.LOCAL):
            tensor = create_tensor("npu", COPY_SIZE)
            return BigMemoryBlock(tp, tensor.data_ptr(), COPY_SIZE, tensor)

        if tp == RankMemType(MemType.GH, RankType.LOCAL):
            rank_id = self.test_rank_id
            pos = mf_hybrid.bm.BmMemType.HOST
            base = ctx.get_handle().peer_rank_ptr(rank_id, pos)
            return BigMemoryBlock(tp, base + idx * COPY_SIZE, COPY_SIZE, None)

        if tp == RankMemType(MemType.GD, RankType.LOCAL):
            rank_id = self.test_rank_id
            pos = mf_hybrid.bm.BmMemType.DEVICE
            base = ctx.get_handle().peer_rank_ptr(rank_id, pos)
            return BigMemoryBlock(tp, base + idx * COPY_SIZE, COPY_SIZE, None)

        if tp == RankMemType(MemType.GH, RankType.REMOTE):
            rank_id = self.peer_rank_id
            pos = mf_hybrid.bm.BmMemType.HOST
            base = ctx.get_handle().peer_rank_ptr(rank_id, pos)
            return BigMemoryBlock(tp, base + idx * COPY_SIZE, COPY_SIZE, None)

        if tp == RankMemType(MemType.GD, RankType.REMOTE):
            rank_id = self.peer_rank_id
            pos = mf_hybrid.bm.BmMemType.DEVICE
            base = ctx.get_handle().peer_rank_ptr(rank_id, pos)
            return BigMemoryBlock(tp, base + idx * COPY_SIZE, COPY_SIZE, None)

        raise RuntimeError(f"unknown mem_type: {tp}")

    def make_mems(self, nodes: List[RankMemType], ctx: BigMemoryContext):
        idx_dict: Dict[RankMemType, int] = {}
        mems: List[BigMemoryBlock] = []
        for node in nodes:
            idx = idx_dict[node] if node in idx_dict else 0
            mem = self.make_mem(node, idx, ctx)
            logging.info(f"make mem block: {mem}")
            mems.append(mem)
            idx_dict[node] = idx + 1
        return mems

    def run_test(self, device_id: int):
        logging.info("test prepare start.")
        rank_id = self.test_rank_id
        ctx = BigMemoryContext(
            store_url=STORE_URL, device_id=device_id, rank_id=rank_id
        )
        ret = ctx.prepare()
        if ret != 0:
            logging.error(f"prepare failed: {ret=}")
            self.begin_barrier.wait()
            self.end_barrier.wait()
            return
        logging.info("test prepare success.")
        self.begin_barrier.wait()

        src_checksum_list = []
        dst_checksum_list = []
        errors_list = []

        try:
            for idx in range(REPEAT_TIMES):
                iter_mark = f"[{idx+1}/{REPEAT_TIMES}]"
                path_resolver = BigMemoryCopyPathResolver()
                nodes = path_resolver.find_eulerian_path()
                mems = self.make_mems(nodes, ctx)
                errors = []
                for i in range(1, len(mems)):
                    op_mark = f"[{i}/{len(mems)-1}]"
                    src = mems[i - 1]
                    dst = mems[i]
                    size = min(src.size, dst.size)
                    action = (
                        f"{iter_mark} {op_mark} copy {size} bytes from {src} to {dst}"
                    )
                    logging.info(action)
                    try:
                        copy_data(mems[i - 1], mems[i], size, ctx)
                    except Exception as error:
                        logging.error(f"{iter_mark} {op_mark} copy error: {error}")
                        if ERROR_ACTION == "stop":
                            raise error
                        else:
                            errors.append((error, action))
                src_checksum = mems[0].get_checksum()
                dst_checksum = mems[-1].get_checksum()
                src_checksum_list.append(src_checksum)
                dst_checksum_list.append(dst_checksum)
                errors_list.append(errors)
                print(
                    f"{iter_mark} Checksum matches: {src_checksum==dst_checksum} {src_checksum} {dst_checksum}"
                )

            print("\nTotal Results:")
            for idx in range(REPEAT_TIMES):
                iter_mark = f"[{idx+1}/{REPEAT_TIMES}]"
                src_checksum = src_checksum_list[idx]
                dst_checksum = dst_checksum_list[idx]
                errors = errors_list[idx]
                print(
                    f"{iter_mark} Checksum matches: {src_checksum==dst_checksum} {src_checksum} {dst_checksum}"
                )
                for error, action in errors:
                    print(f"{action} error: {error}")

        except Exception as error:
            logging.info(f"test error: {error}")

        self.end_barrier.wait()
        logging.info("test exiting...")
        del ctx
        mf_hybrid.bm.uninitialize()
        mf_hybrid.uninitialize()
        logging.info("test exited.")

    def run_peer(self, device_id: int):
        logging.info("peer prepare start.")
        rank_id = self.peer_rank_id
        context = BigMemoryContext(
            store_url=STORE_URL, device_id=device_id, rank_id=rank_id
        )
        ret = context.prepare()
        if ret != 0:
            logging.error(f"prepare failed: {ret=}")
            self.begin_barrier.wait()
            self.end_barrier.wait()
            return

        logging.info("peer prepare success.")
        self.begin_barrier.wait()
        self.end_barrier.wait()
        logging.info("peer exiting...")
        del context
        mf_hybrid.bm.uninitialize()
        mf_hybrid.uninitialize()
        logging.info("peer exited.")


def main_process():
    logging.info("main process start ...")

    test = BigMemoryTest()

    children = [
        multiprocessing.Process(
            name="Test-Process", target=test.run_test, args=(DEVICES[0],)
        ),
        multiprocessing.Process(
            name="Peer-Process", target=test.run_peer, args=(DEVICES[1],)
        ),
    ]

    [child.start() for child in children]
    [child.join() for child in children]

    logging.info("main process exit.")


if __name__ == "__main__":
    # 配置日志，将默认日志输出到标准输出
    logging.basicConfig(
        level=logging.DEBUG,  # 设置日志级别
        format="\033[33m%(asctime)s - %(processName)s - %(levelname)s - [PID: %(process)d] - %(message)s\033[0m",
        datefmt="%Y-%m-%d %H:%M:%S",  # 设置时间戳格式
        stream=sys.stdout,  # 指定输出流为标准输出
    )
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--devices",
        help="specify two device_id to use",
        type=int,
        nargs=2,
        metavar="id",
        default=[0, 1],
    )
    parser.add_argument(
        "--error",
        help="specify error behavior",
        choices=["bypass", "stop"],
        default="bypass",
    )
    parser.add_argument(
        "--repeat", help="specify test repeat times", type=int, default=1
    )
    args = parser.parse_args()
    DEVICES = args.devices
    ERROR_ACTION = args.error
    REPEAT_TIMES = args.repeat
    main_process()