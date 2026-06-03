
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
from abc import ABC
import math
import torch
from tensor_rtl.core.comm_task import P2PCommTask, MFCommTask
from tensor_rtl.utils.utils import get_dtype_size
from tensor_rtl.core.buffer_pool import BufferPool


class PTensor(ABC):
    def __init__(self,
                 tensor: torch.Tensor,
                 dtype: torch.dtype,
                 ndim: int,
                 device_mesh: list[int],
                 global_size: torch.Size,
                 shard_dim: int,
                 rank: int = None,
                 backend: str = 'hccl',
                 check_sanity: bool = True):
        """
        Initialize a PTensor instance with validation.

        Args:
            tensor (torch.Tensor): The tensor data
            dtype (torch.dtype): The data type of the tensor
            ndim (int): Number of dimensions of the tensor
            device_mesh (list[int]): List of device ranks for distributed computing
            global_size (torch.Size): Global size of the tensor across all devices
            shard_dim (int): Dimension along which tensor is sharded (-1 for no sharding)
            rank (int, optional): Device rank. If None, uses current rank from distributed environment
            backend (str): Communication backend ('hccl' is supported)
        """
        self.tensor = tensor
        self.dtype = dtype
        self.ndim = ndim
        self.device_mesh = device_mesh
        self.global_size = global_size
        self.shard_dim = shard_dim

        self.rank = torch.distributed.get_rank() if rank is None else rank
        self.backend = backend
        self.check_sanity = check_sanity
        if check_sanity:
            self._validate_ptensor_params()
        self._op_list = []
        self.buffer_pool = BufferPool()
        self.use_empty_recv = True

    def transfer_map(self, update_device_mesh, use_empty_recv: bool = True):
        if self.check_sanity:
            self._validate_device_mesh(update_device_mesh, "update_device_mesh")
            self._validate_update_device_mesh_length(update_device_mesh)
        self.use_empty_recv = use_empty_recv
        if self.backend == 'hccl':
            ops = self._build_p2p_comm_mapping(update_device_mesh)
        elif self.backend == 'mf':
            ops = self._build_mf_comm_mapping(update_device_mesh)

        self._op_list.extend(ops)

    def get_transfer_list(self):
        """
        Get the list of transfer operations.

        Returns:
            list: Internal operation list
        """
        return self._op_list

    def collect_tensor(self):
        """
        Get the target tensor after communication and concat.

        Returns:
            Torch.Tensor: Targer tensor.
        """
        tensor_list = []
        for task in self._op_list:
            if not task.is_send and task.src_rank == self.rank:
                tensor_list.append(task.buffer)
        if len(tensor_list) == 0:
            return None
        if len(tensor_list) > 1:
            tensor = torch.cat(tensor_list, dim=self.shard_dim)
        else:
            tensor = tensor_list[0]
        return tensor

    def _build_mf_comm_mapping(self, update_device_mesh: list):
        """
        Create transfer mappings for tensor movement between devices.(using memory-fabric)
        """
        accept_data_stack = self._build_accept_mapping(update_device_mesh)
        launch_data_stack = self._build_launch_mapping(update_device_mesh)

        accept_ops = [
                MFCommTask(
                    idx=idx,
                    size=size,
                    is_send=False,
                    src_rank=self.rank,
                    dst_rank=from_rank,
                    gva_ptr=storage_offset,
                    buffer=tensor
                )
                for idx, (from_rank, tensor, size, storage_offset) in enumerate(accept_data_stack)
            ]
        launch_ops = []
        if self.rank in self.device_mesh:
            launch_rank_map = self._intra_bisect(self.device_mesh, update_device_mesh)
            _accept_data_stack_dict = self._build_accept_gva_map(update_device_mesh, launch_rank_map)

            idx = 0
            for to_rank, bf_data, size, tensor_slice in launch_data_stack:
                if not _accept_data_stack_dict[to_rank][self.rank]:
                    raise RuntimeError(f"[TensorRTL] Can't find recv memory pool gva for send task.")
                _, _, _size, storage_offset = _accept_data_stack_dict[to_rank][self.rank].pop(0)
                if _size != size:
                    raise RuntimeError(f"[TensorRTL] Two task have different size, please check send and recv tasks.")

                launch_ops.append(
                    MFCommTask(
                        idx=idx,
                        size=size,
                        is_send=True,
                        src_rank=self.rank,
                        dst_rank=to_rank,
                        gva_ptr=storage_offset,
                        buffer=bf_data,
                        tensor_slice=tensor_slice
                    )
                )
                idx += 1
        return accept_ops + launch_ops


    def _build_p2p_comm_mapping(self, update_device_mesh: list):
        """
        Create transfer mappings for tensor movement between devices.(using hccl)
        """

        accept_data_stack = self._build_accept_mapping(update_device_mesh)
        launch_data_stack = self._build_launch_mapping(update_device_mesh)

        accept_ops = [
                P2PCommTask(
                    idx=idx,
                    numel=None,
                    is_send=False,
                    src_rank=self.rank,
                    dst_rank=from_rank,
                    buffer=tensor
                )
                for idx, (from_rank, tensor, _, _) in enumerate(accept_data_stack)
            ]

        launch_ops = [
                P2PCommTask(
                    idx=idx,
                    numel=None,
                    is_send=True,
                    src_rank=self.rank,
                    dst_rank=to_rank,
                    buffer=bf_data
                )
                for idx, (to_rank, bf_data, _, _) in enumerate(launch_data_stack)
            ]

        return accept_ops + launch_ops


    def _build_accept_mapping(self, update_device_mesh):
        """create accept task mapping"""
        if self.rank not in update_device_mesh:
            return []

        local_size = torch.Size(
            int(size * (1 / len(update_device_mesh)) if dim == self.shard_dim else size)
            for dim, size in enumerate(self.global_size)
        )

        accept_rank_map = self._intra_bisect(update_device_mesh, self.device_mesh)

        accept_data_stack = []

        offset = 0
        for from_rank, tensor_slice in accept_rank_map.items():
            if self.use_empty_recv:
                dst_buffer = self._get_buffer_from_size(local_size, tensor_slice)
            else:
                dst_buffer = self._get_buffer(self.tensor, tensor_slice)
            size = dst_buffer.numel() * dst_buffer.element_size()
            accept_data_stack.append((from_rank, dst_buffer, size, offset))
            offset += size
        return accept_data_stack

    def _build_launch_mapping(self, update_device_mesh):
        """create launch task mapping"""
        if self.rank not in self.device_mesh:
            return []

        launch_rank_map = self._intra_bisect(self.device_mesh, update_device_mesh)
        launch_data_stack = []
        length_device_mesh, length_update_device_mesh = len(self.device_mesh), len(update_device_mesh)
        for to_rank, tensor_slice in launch_rank_map.items():
            src_buffer = self._get_buffer_buffer_pool(
                self.tensor, tensor_slice, 
                length_device_mesh, 
                length_update_device_mesh
                )
            size = src_buffer.numel() * src_buffer.element_size()
            launch_data_stack.append((to_rank, src_buffer, size, tensor_slice))

        return launch_data_stack


    def _build_accept_gva_map(self, update_device_mesh, launch_rank_map):
        """build gva mapping for mf task"""
        _accept_data_stack_dict = {}
        for to_rank in launch_rank_map.keys():
            _accept_data_stack_dict.setdefault(to_rank, {})
            local_size = torch.Size(
                int(size * (1 / len(update_device_mesh)) if dim == self.shard_dim else size)
                for dim, size in enumerate(self.global_size)
            )
            _recv_rank_map = self._intra_bisect(update_device_mesh, self.device_mesh, to_rank)
            storage_offset = 0
            for _from_rank, _tensor_slice in _recv_rank_map.items():
                _accept_data_stack_dict[to_rank].setdefault(_from_rank, [])
                size = self._get_buffer_meta(local_size, _tensor_slice)
                _accept_data_stack_dict[to_rank][_from_rank].append((None, None, size, storage_offset))
                storage_offset += size
        return _accept_data_stack_dict


    def _get_buffer_buffer_pool(self, buffer, tensor_slice, length_device_mesh, length_update_device_mesh):
        if self.buffer_pool.get((length_device_mesh, length_update_device_mesh), tensor_slice) is None:
            if self.shard_dim == -1:
                _register_buffer = buffer
            elif buffer.ndim == 1:
                _register_buffer = buffer[tensor_slice[0]:tensor_slice[1]]
            else:
                if self.shard_dim == 0:
                    _register_buffer = buffer[tensor_slice[0]: tensor_slice[1], :]
                elif self.shard_dim == 1:
                    _register_buffer = buffer[:, tensor_slice[0]: tensor_slice[1]].contiguous()
            self.buffer_pool.register((length_device_mesh, length_update_device_mesh), tensor_slice, _register_buffer)
            register_buffer = self.buffer_pool.get((length_device_mesh, length_update_device_mesh), tensor_slice)
        else:
            register_buffer = self.buffer_pool.get((length_device_mesh, length_update_device_mesh), tensor_slice)
        return register_buffer

    def _get_buffer_from_size(self, buffer_size, tensor_slice):
        try:
            device = torch.npu.current_device()
        except (RuntimeError, AttributeError):
            device = torch.device('cpu')
        if self.shard_dim == -1:
            return torch.empty(*buffer_size, dtype=self.dtype, device=device)
        if self.ndim == 1:
            return torch.empty((tensor_slice[1] - tensor_slice[0],), dtype=self.dtype, device=device)
        return {
            0: lambda: torch.empty(
                (tensor_slice[1] - tensor_slice[0], buffer_size[1]), dtype=self.dtype, device=device
            ),
            1: lambda: torch.empty(
                (buffer_size[0], tensor_slice[1] - tensor_slice[0]), dtype=self.dtype, device=device
            ),
        }[self.shard_dim]()


    def _get_buffer(self, buffer: torch.Tensor, tensor_slice: tuple):
        """
        Get tensor slice based on shard dimension.

        Args:
            buffer (torch.Tensor): The tensor buffer
            tensor_slice (tuple): Slice indices for tensor extraction

        Returns:
            torch.Tensor: Sliced tensor
        """
        if self.shard_dim == -1:
            return buffer
        if buffer.ndim == 1:
            return buffer[tensor_slice[0]:tensor_slice[1]]
        return {
            0: lambda: buffer[tensor_slice[0]: tensor_slice[1], :],
            1: lambda: buffer[:, tensor_slice[0]: tensor_slice[1]],
        }[self.shard_dim]()

    def _get_buffer_meta(self, local_size, tensor_slice):
        dtype_byte = get_dtype_size(self.dtype)
        if self.shard_dim == -1:
            return math.prod(local_size) * (dtype_byte)
        if self.ndim == 1:
            return (tensor_slice[1] - tensor_slice[0]) * (dtype_byte)
        return {
            0: lambda: (tensor_slice[1] - tensor_slice[0]) * local_size[1] * (dtype_byte),
            1: lambda: (tensor_slice[1] - tensor_slice[0]) * local_size[0] * (dtype_byte),
        }[self.shard_dim]()

    def _intra_bisect(self, old_device_mesh: list, update_device_mesh: list, dst_rank: int = None):
        """
        Perform intra-device bisection for tensor mapping.

        Args:
            old_device_mesh (list): Original device mesh
            update_device_mesh (list): Updated device mesh

        Returns:
            dict: Rank mapping for tensor slices
        """
        if dst_rank is None:
            dst_rank = self.rank
        slice_idx = old_device_mesh.index(dst_rank)

        peer_slice_world_size = len(update_device_mesh)

        hidden_size = self.global_size[self.shard_dim]
        global_slice = (
            hidden_size * slice_idx // len(old_device_mesh), hidden_size * (slice_idx + 1) // len(old_device_mesh)
        )

        peer_slices = [
            (slice_ * (hidden_size // peer_slice_world_size), (slice_ + 1) * (hidden_size // peer_slice_world_size))
            for slice_ in range(peer_slice_world_size)
        ]

        rank_map = {}
        for idx, map_slice in enumerate(peer_slices):
            if not (global_slice[0] >= map_slice[1] or global_slice[1] <= map_slice[0]):
                rank_map.setdefault(
                    update_device_mesh[idx], (max(map_slice[0], global_slice[0]), min(map_slice[1], global_slice[1]))
                )

        for rank, slices in rank_map.items():
            rank_map[rank] = tuple(bound - global_slice[0] for bound in slices)

        return rank_map

    def _validate_update_device_mesh_length(self, update_device_mesh: list):
        if self.shard_dim != -1:
            update_len, mesh_len = len(update_device_mesh), len(self.device_mesh)
            if max(update_len, mesh_len) % min(update_len, mesh_len) != 0:
                raise ValueError(f"PTensor requires device_mesh length ({mesh_len}) and "
                                 f"update_device_mesh length ({update_len}) to be multiples of each other")

    def _validate_device_mesh(self, device_mesh: list, mesh_name: str):
        """Validate device mesh parameters"""
        # Check that device_mesh is valid
        if device_mesh is None:
            raise ValueError(f"PTensor {mesh_name} cannot be None")

        if not isinstance(device_mesh, list):
            raise TypeError(f"PTensor {mesh_name} must be a list")

        if len(device_mesh) == 0:
            raise ValueError(f"PTensor {mesh_name} cannot be empty")

        if not all(isinstance(rank, int) for rank in device_mesh):
            raise TypeError(f"PTensor {mesh_name} must contain only int types")

        # Check that when shard_dim is -1, device_mesh length is 1
        if self.shard_dim == -1 and len(device_mesh) != 1:
            raise ValueError(f"PTensor shard_dim=-1 requires {mesh_name} length to be 1, "
                             f"but got device_mesh length {len(self.device_mesh)}")

        if len(device_mesh) != len(set(device_mesh)):
            raise ValueError(f"PTensor {mesh_name} cannot contain duplicate elements")

        world_size = torch.distributed.get_world_size()
        for rank in device_mesh:
            if rank < 0:
                raise ValueError(f"PTensor {mesh_name} contains negative rank {rank}")
            if self.backend == "hccl" and rank >= world_size:
                raise ValueError(f"PTensor {mesh_name} contains rank {rank} which is >= world_size {world_size}")

    def _validate_ptensor_params(self):
        """
        Validate all parameters of PTensor instance.

        Raises:
            ValueError: If any parameter validation fails
        """

        # Check that shard_dim is valid
        if self.shard_dim != -1 and self.shard_dim >= self.ndim:
            raise ValueError(f"PTensor shard_dim {self.shard_dim} must be less than ndim {self.ndim}")

        if self.shard_dim < -1:
            raise ValueError(f"PTensor shard_dim {self.shard_dim} must be non-negative")

        # Check that rank is valid
        if self.backend == "hccl":
            world_size = torch.distributed.get_world_size()
            if self.rank >= world_size:
                raise ValueError(f"PTensor rank {self.rank} must be less than world_size {world_size}")

        if self.rank < 0:
            raise ValueError(f"PTensor rank {self.rank} must be non-negative")

        # Check that backend is supported
        if self.backend not in ['hccl', 'mf']:
            raise ValueError(f"PTensor backend {self.backend} is not supported")

        self._validate_device_mesh(self.device_mesh, "device_mesh")

        # Check that all other int parameters are actually int types
        int_params = [
            ('ndim', self.ndim),
            ('shard_dim', self.shard_dim),
            ('rank', self.rank)
        ]

        for param_name, param_value in int_params:
            if not isinstance(param_value, int):
                raise ValueError(f"PTensor {param_name} {param_value} must be int type")

        # Check that global_size is tuple/list and all elements are integers
        if self.global_size is not None:
            if not isinstance(self.global_size, (tuple, list)):
                raise ValueError(f"PTensor global_size {self.global_size} must be tuple or list type")

            for i, size in enumerate(self.global_size):
                if not isinstance(size, int):
                    raise ValueError(f"PTensor global_size[{i}] {size} must be int type")


class PTensorSet:
    def __init__(self, ptensor_list: list[PTensor]):
        """
        Initialize a PTensorSet with a list of PTensors.

        Args:
            ptensor_list (list[PTensor]): List of PTensor instances
        """
        self.ptensor_list = ptensor_list
        self._op_list = []

    def transfer_map(self, update_map: list, use_empty_recv: bool = True):
        """
        Create transfer mappings for all PTensors in the set.

        Args:
            update_map (list): List of device mesh configurations
        """
        ptensor_idx = 0

        for mesh in update_map:
            self.ptensor_list[ptensor_idx % len(self.ptensor_list)].transfer_map(mesh, use_empty_recv)
            ptensor_idx += 1
        for ptensor in self.ptensor_list:
            op = ptensor.get_transfer_list()
            self._op_list.extend(op)


    def get_transfer_list(self):
        """
        Get internal transfer list.

        Returns:
            list: Internal operation list
        """
        return self._op_list

    def collect_tensor(self):
        """
        Get the target tensor after communication and concat.

        Returns:
            list: Targer tensors.
        """
        tensor_list = []
        for ptensor in self.ptensor_list:
            tensor = ptensor.collect_tensor()
            if tensor is not None:
                tensor_list.append(tensor)
        return tensor_list
