
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

from enum import Enum
from typing import List
import torch
import torch.distributed as dist

from tensor_rtl.api.abc import Executor
from tensor_rtl.core.comm_task import P2PCommTask, MFCommTask

try:
    import memfabric_hybrid
    from memfabric_hybrid import bm
except ImportError:
    memfabric_hybrid = None
    bm = None


class BatchP2PExecutor(Executor):
    def __init__(self):
        super().__init__()

    def execute(self, tasks: List[P2PCommTask]):

        batch_comm_list = []
        rank = dist.get_rank()

        for task in tasks:
            if rank != task.src_rank:
                continue

            if task.is_send:
                batch_comm_list.append(
                    dist.P2POp(dist.isend, task.buffer, task.dst_rank)
                )
            else:
                batch_comm_list.append(
                    dist.P2POp(dist.irecv, task.buffer, task.dst_rank)
                )

        handles = []
        if len(batch_comm_list) != 0:
            handles = dist.batch_isend_irecv(batch_comm_list)

        self.handles = handles

    def wait(self):
        for handle in self.handles:
            handle.wait()


class All2AllVExcutor(Executor):
    def __init__(self):
        super().__init__()
        self.all2allv_send_tensor = None
        self.all2allv_recv_tensor = None
        self.send_list = []
        self.recv_list = []
        self.cur_task_list = []
        self.handles = None

    def execute(self, tasks: List[P2PCommTask]):
        all2allv_send_tensor, all2allv_recv_tensor, send_list, recv_list = self.build_all2all_info(tasks)

        self.handles = dist.all_to_all_single(
            all2allv_recv_tensor,
            all2allv_send_tensor,
            output_split_sizes=recv_list,
            input_split_sizes=send_list,
            group=None,
            async_op=False
        )

        return all2allv_recv_tensor, tasks

    def build_all2all_info(self, task_list: List[P2PCommTask]):
        rank = dist.get_rank()
        send_rank_tasks = {}
        recv_rank_tasks = {}

        for task in task_list:
            if rank != task.src_rank:
                continue

            buffer = task.buffer
            if task.is_send:
                send_rank_tasks.setdefault(task.dst_rank, [])
                send_rank_tasks[task.dst_rank].append(buffer)
            else:
                recv_rank_tasks.setdefault(task.dst_rank, [])
                recv_rank_tasks[task.dst_rank].append(buffer)

        world_size = dist.get_world_size()
        send_list = [0] * world_size
        recv_list = [0] * world_size
        for dst_rank in range(world_size):
            if dst_rank in send_rank_tasks:
                send_list[dst_rank] = sum(tensor.numel() for tensor in send_rank_tasks[dst_rank])
            if dst_rank in recv_rank_tasks:
                recv_list[dst_rank] = sum(tensor.numel() for tensor in recv_rank_tasks[dst_rank])

        send_tensors = []
        for dst_rank in range(world_size):
            if dst_rank in send_rank_tasks:
                send_tensors.extend(send_rank_tasks[dst_rank])
        all2allv_send_tensor = torch.cat(send_tensors)

        recv_tensors = []
        for dst_rank in range(world_size):
            if dst_rank in recv_rank_tasks:
                recv_tensors.extend(recv_rank_tasks[dst_rank])
        all2allv_recv_tensor = torch.cat(recv_tensors)

        return all2allv_send_tensor, all2allv_recv_tensor, send_list, recv_list

    def execute_async(self, task_list: List[P2PCommTask], pre_build_task_list: List[P2PCommTask]):
        if len(pre_build_task_list) > 0:
            all2allv_send_tensor, all2allv_recv_tensor, send_list, recv_list = self.build_all2all_info(
                pre_build_task_list
            )
            cur_task_list = pre_build_task_list
        else:
            all2allv_send_tensor = self.all2allv_send_tensor
            all2allv_recv_tensor = self.all2allv_recv_tensor
            send_list = self.send_list
            recv_list = self.recv_list
            cur_task_list = self.cur_task_list

        self.handles = dist.all_to_all_single(
            all2allv_recv_tensor,
            all2allv_send_tensor,
            output_split_sizes=recv_list,
            input_split_sizes=send_list,
            group=None,
            async_op=True
        )

        if len(task_list) > 0:
            self.all2allv_send_tensor, self.all2allv_recv_tensor, self.send_list, self.recv_list =\
                self.build_all2all_info(
                    task_list
                )
            self.cur_task_list = task_list

        return all2allv_recv_tensor, cur_task_list

    def wait(self):
        """
        阻塞等待所有通信完成
        Args:
            handles: 通信算子句柄
        """
        if isinstance(self.handles, list):
            for handle in self.handles:
                handle.wait()
        else:
            self.handles.wait()


class MemoryFabricExecutor(Executor):
    memfabric_hybrid_initialize = False
    bm_handle = None

    class ExecutorType(Enum):
        WRITER = 'writer'
        READER = 'reader'
        MIXER = 'mix'

    def __init__(self, bm_config, rank_id=None, executor_type=ExecutorType.MIXER):
        super().__init__()
        if not MemoryFabricExecutor.memfabric_hybrid_initialize:
            ret = memfabric_hybrid.initialize()
            if rank_id is None:
                rank_id = dist.get_rank()
            device_id = torch.npu.current_device()
            if ret != 0:
                raise RuntimeError(
                    f"[TensorRTL]rank: {rank_id}, rank_size: {bm_config.rank_world_size},"
                    f"url: {bm_config.url} initialize failed: {ret}"
                )

            config = bm.BmConfig()
            config.auto_ranking = bm_config.auto_ranking
            if not bm_config.auto_ranking:
                config.rank_id = rank_id
            config.flags = bm_config.hybm_int_gvm_flag
            config.set_nic("tcp://127.0.0.1:1234")  # for device port
            ret = bm.initialize(
                store_url=bm_config.url, world_size=bm_config.rank_world_size, device_id=device_id, config=config
            )
            if ret != 0:
                raise RuntimeError(f"[TensorRTL]smem BM initialize failed: {rank_id=}, {device_id=}, {ret}")

            bm_handle = bm.create(
                id=0,
                local_dram_size=bm_config.local_dram_size,
                local_hbm_size=bm_config.local_hbm_size,
                data_op_type=bm_config.data_op_type
            )
            bm_handle.join()
            bm_config.bm_handle = bm_handle
            MemoryFabricExecutor.bm_handle = bm_handle
            MemoryFabricExecutor.memfabric_hybrid_initialize = True

        self.bm_config = bm_config
        self.executor_type = executor_type
        self.bm_handle = MemoryFabricExecutor.bm_handle

    def execute(self, tasks: List[MFCommTask]):
        src_addrs_list, dst_addrs_list, size_list, count = self.convert_to_batch_copy(tasks)

        torch.npu.synchronize()
        if self.is_writer():
            ret = self.bm_handle.copy_data_batch(
                src_addrs=src_addrs_list, dst_addrs=dst_addrs_list, sizes=size_list, count=count,
                type=bm.BmCopyType.L2G, flags=0
            )
        if self.is_reader():
            ret = self.bm_handle.copy_data_batch(
                src_addrs=src_addrs_list, dst_addrs=dst_addrs_list, sizes=size_list, count=count,
                type=bm.BmCopyType.G2L, flags=0
            )

    def wait(self):
        pass

    def is_writer(self):
        return self.executor_type is not self.ExecutorType.READER

    def is_reader(self):
        return self.executor_type is not self.ExecutorType.WRITER

    def convert_to_batch_copy(self, task_list: List[MFCommTask]):
        src_addrs_list = []
        dst_addrs_list = []
        size_list = []
        count = 0
        is_writer = self.is_writer()
        is_reader = self.is_reader()
        for task in task_list:
            if is_writer and not task.is_send:
                continue
            if is_reader and task.is_send:
                continue
            rank_handle = self.bm_handle.peer_rank_ptr(task.pool_rank, mem_type=self.bm_config.mem_type)
            gva = rank_handle + task.gva_ptr

            if task.is_send:
                src_addrs_list.append(task.buffer.data_ptr())
                dst_addrs_list.append(gva)
            else:
                src_addrs_list.append(gva)
                dst_addrs_list.append(task.buffer.data_ptr())

            size_list.append(task.size)
            count += 1
        return src_addrs_list, dst_addrs_list, size_list, count

    def _write(self, task):
        rank_handle = self.bm_handle.peer_rank_ptr(task.pool_rank, mem_type=self.bm_config.mem_type)
        gva = rank_handle + task.gva_ptr
        torch.npu.synchronize()
        self.bm_handle.copy_data(src_ptr=task.buffer.data_ptr(), dst_ptr=gva, size=task.size, type=bm.BmCopyType.L2G)

    def _read(self, task):
        rank_handle = self.bm_handle.peer_rank_ptr(task.pool_rank, mem_type=self.bm_config.mem_type)
        gva = rank_handle + task.gva_ptr
        torch.npu.synchronize()
        self.bm_handle.copy_data(src_ptr=gva, dst_ptr=task.buffer.data_ptr(), size=task.size, type=bm.BmCopyType.G2L)
