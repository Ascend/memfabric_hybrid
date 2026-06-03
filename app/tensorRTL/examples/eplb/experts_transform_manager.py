
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.

import logging
import enum
from typing import TYPE_CHECKING, List
from multiprocessing import Queue, Manager

import torch

from sglang.srt.eplb.eplb_manager import EPLBManager
from sglang.srt.eplb.expert_location import ExpertLocationMetadata, get_global_expert_location_metadata
from sglang.srt.eplb.expert_distribution import get_global_expert_distribution_recorder
from tensor_rtl import All2AllVExcutor, PTensor, PTensorSet, pack_tensors, unpack_tensors
from .eplb_process import EplbProcess

if TYPE_CHECKING:
    from sglang.srt.model_executor.model_runner import ModelRunner

logger = logging.getLogger(__name__)


class EplbState(int, enum.Enum):
    REBALANCE = 0,
    WAIT_WORKER = 1,
    PLANNER = 2,
    UPDATE = 3,
    FINAL = 4


class ExpertsTransformManager(EPLBManager):
    """Core Manager: Interfaces with SGLang, coordinates memory/communication/strategy modules"""
    def __init__(
        self,
        model_runner: "ModelRunner",
        async_mode: bool = False,
        num_wait_worker_iterations: int = 40
    ):
        super().__init__(model_runner)

        self.config = self._model_runner.model_config
        self._server_args = model_runner.server_args
        self._rebalance_layers_per_chunk = (
            self._server_args.eplb_rebalance_layers_per_chunk
        )
        self.async_mode = async_mode
        self.num_wait_worker_iterations = num_wait_worker_iterations
        self.executor = All2AllVExcutor()
        self._main_generator = self._entrypoint()
        self.common = ExpertLocationMetadata._init_common(self._server_args, self.config)
        self.num_physical_expert = self.common["num_physical_experts"]
        self.num_local_physical_expert = self.num_physical_expert // self.common["ep_size"]
        self.device = self._server_args.device
        self.rank = torch.distributed.get_rank()

        self.planner_q = Queue()
        self.block_q = Queue(maxsize=1)
        self.manager = Manager()
        self.shared_dict = self.manager.dict({
            "moe_load": None,
        })
        self.eplb = EplbProcess(
            shared_dict=self.shared_dict,
            planner_q=self.planner_q,
            block_q=self.block_q,
            server_args=self._server_args,
            model_config=self.config,
            rank=self.rank,
        )
        self.eplb_process = self.eplb.launch_process()

        self.state = EplbState.REBALANCE
        self.expert_location_metadata = None
        self.old_expert_map = None
        self.update_expert_map = None
        self.ops_list = []
        self.ops_activated = []
        self.packed_expert_dict = {}  # layer_id:[(packed_tensor_1, metadata_1), (packed_tensor_2, metadata_2)]
        self.self_rank_update_info = {}
        logger.info(f"[ModelRunner] Launched EPLB process (pid={self.eplb_process.pid})")

    def get_state(self):
        return self.state

    def register_experts(self, expert_tensor: dict):
        """
        Register expert weights for all layers
        Args:
        expert_tensor: Expert memory on this rank
        """
        self.expert_dict = expert_tensor

    def on_forward_pass_end(self):
        next(self._main_generator)

    def _get_forward_update_weight_func(self):
        if self.async_mode:
            return self._forward_update_weight_steps_async
        else:
            return self._forward_update_weight_steps

    def _entrypoint(self):
        while True:
            self.state = EplbState.REBALANCE
            for _ in range(self._rebalance_num_iterations):
                yield
            self.state = EplbState.WAIT_WORKER
            self._forward_eplb_process()
            for cur_iteration in range(self.num_wait_worker_iterations):
                if cur_iteration == self.num_wait_worker_iterations - 1:
                    self.state = EplbState.PLANNER
                yield
            self._take_update_info_from_eplb_process()
            self._init_expert_maps()
            forward_update_weight_steps = self._get_forward_update_weight_func()
            for step in forward_update_weight_steps():
                yield step

    def _forward_eplb_process(self):
        logical_count = get_global_expert_distribution_recorder().dump_record(output_mode="object")["logical_count"]
        self.shared_dict["moe_load"] = logical_count.cpu()
        self._wakeup_eplb_worker()

    def _wakeup_eplb_worker(self):
        self.planner_q.put(1)

    def _take_update_info_from_eplb_process(self):
        # Batch after eplb process being triggered, get update info provided by eplb process
        self.update_info_all = self.block_q.get()

    def _init_expert_maps(self):
        old_expert_location_metadata = get_global_expert_location_metadata()
        self.expert_location_metadata = self._to_device(self.update_info_all)

        old_expert_map = old_expert_location_metadata.physical_to_logical_map_cpu
        update_expert_map = self.expert_location_metadata.physical_to_logical_map_cpu

        self.old_expert_map = {}
        self.update_expert_map = {}
        for layer_id in range(old_expert_map.shape[0]):
            self.old_expert_map.setdefault(layer_id, [])
            self.old_expert_map[layer_id] = old_expert_map[layer_id].tolist()
            self.update_expert_map.setdefault(layer_id, [])
            self.update_expert_map[layer_id] = update_expert_map[layer_id].tolist()

    def _forward_update_weight_steps(self):
        all_layer_ids = sorted(list(self.expert_dict.keys()))
        chunk_size = self._rebalance_layers_per_chunk or 1000000
        update_layer_ids_chunks = list(_chunk_list(all_layer_ids, chunk_size=chunk_size))
        old_expert_location_metadata = get_global_expert_location_metadata()

        for chunk_index, update_layer_ids in enumerate(update_layer_ids_chunks):
            self._pack_experts(update_layer_ids)
            self._build_planner_ops(self.old_expert_map, self.update_expert_map, update_layer_ids)
            yield

            self._execute_expert_transform()
            self._update_local_experts(update_layer_ids)
            self._update_experts()
            self._unpack_experts(update_layer_ids)

            if self.state != EplbState.UPDATE:
                self.state = EplbState.UPDATE
            yield

            old_expert_location_metadata.update(
                self.expert_location_metadata,
                update_layer_ids=update_layer_ids,
            )
            if chunk_index == len(update_layer_ids_chunks) - 1:
                self.state = EplbState.FINAL
            yield

    def _forward_update_weight_steps_async(self):
        all_layer_ids = sorted(list(self.expert_dict.keys()))
        chunk_size = self._rebalance_layers_per_chunk or 1000000
        update_layer_ids_chunks = list(_chunk_list(all_layer_ids, chunk_size=chunk_size))
        old_expert_location_metadata = get_global_expert_location_metadata()

        # Generate a planner for the first time to facilitate subsequent concealment
        self._pack_experts(update_layer_ids_chunks[0])
        self._build_planner_ops(self.old_expert_map, self.update_expert_map, update_layer_ids_chunks[0])

        for chunk_index in range(1, len(update_layer_ids_chunks)):
            update_layer_ids = update_layer_ids_chunks[chunk_index]

            self._pack_experts(update_layer_ids)
            self._build_planner_ops(self.old_expert_map, self.update_expert_map, update_layer_ids)
            yield

            self._execute_expert_transform_async(chunk_index)

            self._update_local_experts(update_layer_ids_chunks[chunk_index - 1])
            self._update_experts()
            self._unpack_experts(update_layer_ids_chunks[chunk_index - 1])

            if self.state != EplbState.UPDATE:
                self.state = EplbState.UPDATE
            yield

            old_expert_location_metadata.update(
                self.expert_location_metadata,
                update_layer_ids=update_layer_ids_chunks[chunk_index - 1],
            )

            if chunk_index == len(update_layer_ids_chunks) - 1:
                self.state = EplbState.FINAL

                self._execute_expert_transform_async()
                self._update_local_experts(update_layer_ids_chunks[-1])
                self._update_experts()
                self._unpack_experts(update_layer_ids_chunks[-1])
            yield

            if chunk_index == len(update_layer_ids_chunks) - 1:
                old_expert_location_metadata.update(
                    self.expert_location_metadata,
                    update_layer_ids=update_layer_ids_chunks[-1],
                )

    def _to_device(self, metadata):
        if metadata.physical_to_logical_map is not None:
            metadata.physical_to_logical_map = metadata.physical_to_logical_map.to(self.device)

        if metadata.logical_to_all_physical_map is not None:
            metadata.logical_to_all_physical_map = metadata.logical_to_all_physical_map.to(self.device)

        if metadata.logical_to_all_physical_map_num_valid is not None:
            metadata.logical_to_all_physical_map_num_valid = (
                metadata.logical_to_all_physical_map_num_valid.to(self.device)
            )

        if metadata.logical_to_rank_dispatch_physical_map is not None:
            metadata.logical_to_rank_dispatch_physical_map = (
                metadata.logical_to_rank_dispatch_physical_map.to(self.device)
            )
        return metadata

    def _pack_experts(self, update_layer_ids):
        """
        Package local experts into a large tensor for sending, receiving, and other tasks
        """
        for layer_id in update_layer_ids:
            if self.packed_expert_dict.get(layer_id) is None:
                local_experts = self.expert_dict[layer_id]
                self.packed_expert_dict.setdefault(layer_id, [])
                for physical_expert_id in range(self.num_local_physical_expert):
                    expert_tensor_list = [
                        expert[physical_expert_id]
                        for expert in local_experts
                    ]
                    packed_tensor, expert_metadata = pack_tensors(
                        expert_tensor_list,
                        target_dtype=torch.int8
                    )
                    self.packed_expert_dict[layer_id].append(
                        (packed_tensor, expert_metadata)
                    )

    def _unpack_experts(self, update_layer_ids):
        """
        Unpack local experts into original tensors for inference
        """
        for layer_id in update_layer_ids:
            packed_tensor_list = self.packed_expert_dict[layer_id]
            for physical_expert_id, (packed_tensor, expert_metadata) in enumerate(packed_tensor_list):
                unpacked_tensor_list = unpack_tensors(packed_tensor, expert_metadata)
                for i, tensor in enumerate(unpacked_tensor_list):
                    self.expert_dict[layer_id][i][physical_expert_id].data.copy_(tensor)

            self.packed_expert_dict.pop(layer_id, None)

    def _update_local_experts(self, update_layer_ids):
        tmp_expert_weights_dict = {}
        for layer_id in update_layer_ids:
            self.self_rank_update_info.setdefault(layer_id, [])
            tmp_expert_weights_dict.setdefault(layer_id, [])
            for old_expert_id, _ in self.self_rank_update_info[layer_id]:
                tmp_expert_weights_dict[layer_id].append(
                    self.packed_expert_dict[layer_id][old_expert_id][0].clone()
                )

        for layer_id in update_layer_ids:
            for _, update_expert_id in self.self_rank_update_info[layer_id]:
                self.packed_expert_dict[layer_id][update_expert_id][0].data.copy_(
                    tmp_expert_weights_dict[layer_id].pop(0)
                )

            self.self_rank_update_info[layer_id] = []

    def _build_eplb_comm_ops(
        self,
        old_expert_map: torch.Tensor,
        update_expert_map: torch.Tensor,
        update_layer_ids: List[int]
    ):
        # build ptensor_list
        ptensor_list = []
        update_device_meshes = []
        for layer_id in update_layer_ids:
            old_experts = old_expert_map[layer_id]
            local_experts = self.packed_expert_dict[layer_id]
            update_experts = update_expert_map[layer_id]

            self.self_rank_update_info.setdefault(layer_id, [])

            expert_to_new_position = {}
            for position, expert_id in enumerate(update_experts):
                expert_to_new_position[expert_id] = position

            # old:[3, 4, 2, 0, 1]   new:[4, 2, 1, 0, 3]
            for old_position, old_globl_expert_idx in enumerate(old_experts):
                expert_rank = old_position // self.num_local_physical_expert
                local_expert_idx = old_position % self.num_local_physical_expert
                old_globl_expert_idx = old_experts[old_position]

                # Calculate the rank where this expert needs to go after update and
                # the corresponding local id of the exper
                new_position = expert_to_new_position.get(old_globl_expert_idx)
                if new_position is None:
                    raise KeyError(
                        f"[TensorRTL] ExpertID {old_globl_expert_idx} not found in expert_to_new_position"
                    )

                update_rank = new_position // self.num_local_physical_expert
                update_expert_idx = new_position % self.num_local_physical_expert

                tensor_ref = self._get_comm_buffer(
                    local_experts,
                    layer_id,
                    expert_rank,
                    update_rank,
                    local_expert_idx,
                    update_expert_idx
                )
                ptensor = PTensor(
                    tensor=tensor_ref,
                    dtype=tensor_ref.dtype,
                    device_mesh=[expert_rank],  # ETP not adapt
                    global_size=tensor_ref.shape,
                    shard_dim=-1,
                    ndim=tensor_ref.ndim
                )
                ptensor_list.append(ptensor)
                update_device_meshes.append([update_rank])

        ptensor_set = PTensorSet(ptensor_list)
        ptensor_set.transfer_map(update_device_meshes, use_empty_recv=False)
        ops = ptensor_set.get_transfer_list()

        return ops

    def _get_comm_buffer(
        self,
        local_experts,
        layer_id,
        expert_rank,
        update_rank,
        local_expert_idx,
        update_expert_idx
    ):
        if expert_rank == self.rank:
            tensor_ref = local_experts[local_expert_idx][0]

            if update_rank == expert_rank:
                self.self_rank_update_info.setdefault(layer_id, [])
                self.self_rank_update_info[layer_id].append((local_expert_idx, update_expert_idx))
            return tensor_ref
        elif update_rank == self.rank:
            return local_experts[update_expert_idx][0]
        else:
            return torch.empty(
                local_experts[local_expert_idx][0].shape, device=local_experts[local_expert_idx][0].device
            )

    def _build_planner_ops(self, old_expert_map, update_expert_map, update_layer_ids: list[int]):
        ops = self._build_eplb_comm_ops(
            old_expert_map, update_expert_map, update_layer_ids,
        )

        self.ops_list.append(ops)

    def _execute_expert_transform_async(self, chunk_index: int = -1):
        pre_build_task_list = []
        task_list = []

        if chunk_index == 1:
            pre_build_task_list = self.ops_list.pop(0)

        if len(self.ops_list) > 0:
            task_list = self.ops_list.pop(0)

        all_recv_tensors, cur_task_list = self.executor.execute_async(task_list, pre_build_task_list)
        self.ops_activated.append((all_recv_tensors, cur_task_list))

    def _execute_expert_transform(self):
        pre_build_task_list = self.ops_list.pop(0)
        all_recv_tensors, cur_task_list = self.executor.execute_async([], pre_build_task_list)
        self.ops_activated.append((all_recv_tensors, cur_task_list))

    def _update_experts(self):
        all_recv_tensors, task_list = self.ops_activated.pop(0)
        self.executor.wait()

        task_list.sort(key=lambda task: task.dst_rank)

        start_offset = 0
        for task in task_list:
            if self.rank != task.src_rank:
                continue

            if not task.is_send:
                buffer = task.buffer
                if task.src_rank == task.dst_rank:
                    start_offset += buffer.numel()
                    continue

                recv_buffer = all_recv_tensors[start_offset:start_offset + buffer.numel()]

                start_offset += buffer.numel()
                buffer.data.copy_(recv_buffer)


def _chunk_list(items: List, chunk_size):
    for start_index in range(0, len(items), chunk_size):
        yield items[start_index: start_index + chunk_size]


experts_transform_manager = None


def get_experts_transform_manager(
    model_runner: "ModelRunner" = None,
    async_mode: bool = False,
    num_wait_worker_iterations: int = 40
):
    global experts_transform_manager
    if experts_transform_manager is None:
        experts_transform_manager = ExpertsTransformManager(
            model_runner, async_mode, num_wait_worker_iterations
        )
    return experts_transform_manager
