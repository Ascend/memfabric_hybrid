
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

import sys
import os
import argparse
import logging
from functools import partial
import pytest
import torch
try:
    import mf_smem
    from mf_smem import bm
except ImportError:
    mf_smem = None
    bm = None
from tensor_rtl import PTensor, PTensorSet
from tensor_rtl import BatchP2PExecutor, All2AllVExcutor, MemoryFabricExecutor
from tensor_rtl import MfBmConfig


@pytest.fixture(scope="session")
def base_distributed_args():
    """
    Fixture to initialize distributed training environment for tests.

    Returns:
        None: Initializes distributed environment
    """
    parser = argparse.ArgumentParser(description='Distributed test arguments')

    parser.add_argument('--backend', type=str, default='hccl',
                        choices=['nccl', 'hccl', 'gloo'])
    parser.add_argument('--local-rank', type=int, default=None,
                        help='local rank passed from distributed launcher', dest='local_rank')
    parser.add_argument('--master-addr', type=str, default='localhost')
    parser.add_argument('--master-port', type=str, default='29500')

    args, unknown = parser.parse_known_args()
    sys.argv = [sys.argv[0]] + unknown  # Only keep arguments recognized by unittest
    local_rank = args.local_rank

    # Get rank and world size.
    rank = int(os.getenv('RANK', '0'))
    world_size = int(os.getenv("WORLD_SIZE", '8'))
    logging.info('> initializing torch.distributed with local rank: {}, '
          'rank: {}, world size: {}'.format(local_rank, rank, world_size))

    # Set the device id.
    if rank is not None:
        device = rank
    torch.npu.set_device(device)
    # Call the init process.
    init_method = 'tcp://'
    master_ip = os.getenv('MASTER_ADDR', 'localhost')
    master_port = os.getenv('MASTER_PORT', '6000')
    init_method += master_ip + ':' + master_port
    torch.distributed.init_process_group(
        backend='hccl',
        world_size=world_size,
        rank=rank,
        init_method=init_method)


@pytest.mark.parametrize("old_device_meshes, update_device_meshes",
                         [([[0], [1], [2], [3]], [[0], [1], [2], [3], [4], [5], [6], [7]]),
                          ([[0]], [[0], [1], [2], [3], [4], [5], [6], [7]])])
def test_norm(base_distributed_args, old_device_meshes, update_device_meshes):
    """
    Test normalization operation with device mesh resharing.

    Args:
        base_distributed_args: Distributed environment fixture
        old_device_meshes (list): Original device mesh configurations
        update_device_meshes (list): Updated device mesh configurations
    """
    executor = BatchP2PExecutor()

    ptensor_list = []
    for ptensor_device_mesh in old_device_meshes:
        # Create local tensor for the current rank
        local_tensor = None
        for rank in ptensor_device_mesh:
            if torch.distributed.get_rank() == rank:
                local_tensor = torch.nn.LayerNorm(1024, device=torch.npu.current_device()).weight
                logging.info(f"[SRC WEIGHT]{torch.distributed.get_rank()} norm_weight: {torch.sum(local_tensor)}")
            else:
                # For other ranks, create empty tensor with correct shape
                local_tensor = torch.empty((1024,), dtype=torch.float32, device=torch.npu.current_device())

        ptensor_list.append(
            PTensor(
                tensor=local_tensor,
                dtype=torch.float32,
                ndim=1,
                device_mesh=ptensor_device_mesh,
                global_size=(1024,),
                shard_dim=-1,
                rank=torch.distributed.get_rank(),
                backend='hccl'
            )
        )
    ptensor_set = PTensorSet(ptensor_list)
    ptensor_set.transfer_map(update_device_meshes)
    op = ptensor_set.get_transfer_list()
    executor.execute(op)
    executor.wait()

    tensor_list = ptensor_set.collect_tensor()
    tensor = tensor_list[0] if len(tensor_list) > 0 else None

    logging.info(f"[DST WEIGHT]{torch.distributed.get_rank()}" 
           f"{torch.sum(tensor)if tensor is not None else None}")


@pytest.mark.parametrize("old_device_meshes, update_device_meshes",
                         [([[0, 1, 2, 3]], [[0, 1], [2, 3], [4, 5], [6, 7]])])
def test_qkv(base_distributed_args, old_device_meshes, update_device_meshes):
    """
    Test QKV projection operation with device mesh resharing.

    Args:
        base_distributed_args: Distributed environment fixture
        old_device_meshes (list): Original device mesh configurations
        update_device_meshes (list): Updated device mesh configurations
    """
    executor = BatchP2PExecutor()

    ptensor_list = []
    for ptensor_device_mesh in old_device_meshes:
        # Create local tensor for the current rank
        local_tensor = None
        for rank in ptensor_device_mesh:
            if torch.distributed.get_rank() == rank:
                local_tensor = torch.nn.Linear(1024, 1536, device=torch.npu.current_device()).weight
                logging.info(f"[SRC WEIGHT]{torch.distributed.get_rank()} \
                      qkv: {torch.sum(local_tensor)} {local_tensor.size()}")
            else:
                # For other ranks, create empty tensor with correct shape
                local_tensor = torch.empty((1536, 1024), dtype=torch.float32, device=torch.npu.current_device())

        ptensor_list.append(
            PTensor(
                tensor=local_tensor,
                dtype=torch.float32,
                ndim=2,
                device_mesh=ptensor_device_mesh,
                global_size=(6144, 1024),
                shard_dim=0,
                rank=torch.distributed.get_rank(),
                backend='hccl'
            )
        )
    ptensor_set = PTensorSet(ptensor_list)
    ptensor_set.transfer_map(update_device_meshes)

    executor.execute(ptensor_set.get_transfer_list())
    executor.wait()
    tensor_list = ptensor_set.collect_tensor()

    tensor = tensor_list[0] if len(tensor_list) > 0 else None

    logging.info(f"[DST WEIGHT]{torch.distributed.get_rank()} {torch.sum(tensor)if tensor is not None else None}")


@pytest.mark.parametrize("old_device_meshes, update_device_meshes", [([[0, 1]], [[0, 1, 2, 3], [4, 5, 6, 7]])])
def test_o_proj(base_distributed_args, old_device_meshes, update_device_meshes):
    """
    Test O-projection operation with device mesh resharing.

    Args:
        base_distributed_args: Distributed environment fixture
        old_device_meshes (list): Original device mesh configurations
        update_device_meshes (list): Updated device mesh configurations
    """
    executor = BatchP2PExecutor()
    ptensor_list = []
    for ptensor_device_mesh in old_device_meshes:
        # Create local tensor for the current rank
        local_tensor = None
        for rank in ptensor_device_mesh:
            if torch.distributed.get_rank() == rank:
                local_tensor = torch.nn.Linear(1024, 1024, device=torch.npu.current_device()).weight
                logging.info(f"[SRC WEIGHT]{torch.distributed.get_rank()} \
                      qkv: {torch.sum(local_tensor)} {local_tensor.size()}")
            else:
                # For other ranks, create empty tensor with correct shape
                local_tensor = torch.empty((1024, 1024), dtype=torch.float32, device=torch.npu.current_device())

        ptensor_list.append(
            PTensor(
                tensor=local_tensor,
                dtype=torch.float32,
                ndim=2,
                device_mesh=ptensor_device_mesh,
                global_size=(1024, 2048),
                shard_dim=1,
                rank=torch.distributed.get_rank(),
                backend='hccl'
            )
        )
    ptensor_set = PTensorSet(ptensor_list)
    ptensor_set.transfer_map(update_device_meshes)

    executor.execute(ptensor_set.get_transfer_list())
    executor.wait()

    tensor_list = ptensor_set.collect_tensor()
    tensor = tensor_list[0] if len(tensor_list) > 0 else None

    logging.info(f"[DST WEIGHT]{torch.distributed.get_rank()} {torch.sum(tensor)if tensor is not None else None}")


@pytest.mark.parametrize("old_device_meshes, update_device_meshes",
                         [([[0], [1], [2], [3]], [[0], [1], [2], [3], [4], [5], [6], [7]])])
def test_reshard_etp_expert_weight(base_distributed_args, old_device_meshes, update_device_meshes):
    """
    Test ETP expert weight resharding operation.

    Args:
        base_distributed_args: Distributed environment fixture
        old_device_meshes (list): Original device mesh configurations
        update_device_meshes (list): Updated device mesh configurations
    """
    executor = BatchP2PExecutor()

    ptensor_list = []
    for ptensor_device_mesh in old_device_meshes:
        # Create local tensor for the current rank
        local_tensor = None
        for rank in ptensor_device_mesh:
            if torch.distributed.get_rank() == rank:
                local_tensor = torch.randn((1024, 2048), device=torch.npu.current_device())
                logging.info(f"[SRC WEIGHT]{torch.distributed.get_rank()} expert_weight: {torch.sum(local_tensor)}")
            else:
                # For other ranks, create empty tensor with correct shape
                local_tensor = torch.empty((1024, 2048), dtype=torch.float32, device=torch.npu.current_device())

        ptensor_list.append(
            PTensor(
                tensor=local_tensor,
                dtype=torch.float32,
                ndim=2,
                device_mesh=ptensor_device_mesh,
                global_size=(1024, 2048),
                shard_dim=-1,
                rank=torch.distributed.get_rank(),
                backend='hccl'
            )
        )
    ptensor_set = PTensorSet(ptensor_list)
    ptensor_set.transfer_map(update_device_meshes)
    op = ptensor_set.get_transfer_list()
    executor.execute(op)
    executor.wait()

    tensor_list = ptensor_set.collect_tensor()
    tensor = tensor_list[0] if len(tensor_list) > 0 else None
    logging.info(f"[DST WEIGHT]{torch.distributed.get_rank()} {torch.sum(tensor)if tensor is not None else None}")


@pytest.mark.parametrize("old_device_meshes, update_device_meshes",
                         [([[0], [1], [2], [3]], [[4], [0], [3], [7], [1], [2], [6], [5]])])
def test_shuffle_etp_expert_weight(base_distributed_args, old_device_meshes, update_device_meshes):
    """
    Test ETP expert weight shuffling operation.

    Args:
        base_distributed_args: Distributed environment fixture
        old_device_meshes (list): Original device mesh configurations
        update_device_meshes (list): Updated device mesh configurations
    """
    executor = BatchP2PExecutor()

    ptensor_list = []
    for ptensor_device_mesh in old_device_meshes:
        # Create local tensor for the current rank
        local_tensor = None
        for rank in ptensor_device_mesh:
            if torch.distributed.get_rank() == rank:
                local_tensor = torch.randn((1024, 2048), device=torch.npu.current_device())
                logging.info(f"[SRC WEIGHT]{torch.distributed.get_rank()} expert_weight: {torch.sum(local_tensor)}")
            else:
                # For other ranks, create empty tensor with correct shape
                local_tensor = torch.empty((1024, 2048), dtype=torch.float32, device=torch.npu.current_device())

        ptensor_list.append(
            PTensor(
                tensor=local_tensor,
                dtype=torch.float32,
                ndim=2,
                device_mesh=ptensor_device_mesh,
                global_size=(1024, 2048),
                shard_dim=-1,
                rank=torch.distributed.get_rank(),
                backend='hccl'
            )
        )
    ptensor_set = PTensorSet(ptensor_list)
    ptensor_set.transfer_map(update_device_meshes)
    op = ptensor_set.get_transfer_list()
    executor.execute(op)
    executor.wait()

    tensor_list = ptensor_set.collect_tensor()
    tensor = tensor_list[0] if len(tensor_list) > 0 else None
    logging.info(f"[DST WEIGHT]{torch.distributed.get_rank()} {torch.sum(tensor)if tensor is not None else None}")


@pytest.mark.parametrize("old_device_meshes, update_device_meshes",
                         [([[0, 1], [2, 3], [4, 5], [6, 7]],
                           [[0, 1, 2, 3], [0, 1, 2, 3], [4, 5, 6, 7], [4, 5, 6, 7]])])
def test_ep_expert_weight(base_distributed_args, old_device_meshes, update_device_meshes):
    """
    Test EP expert weight operation with continuous mode.

    Args:
        base_distributed_args: Distributed environment fixture
        old_device_meshes (list): Original device mesh configurations
        update_device_meshes (list): Updated device mesh configurations
    """
    executor = BatchP2PExecutor()

    ptensor_list = []
    for ptensor_device_mesh in old_device_meshes:
        # Create local tensor for the current rank
        local_tensor = None
        for rank in ptensor_device_mesh:
            if torch.distributed.get_rank() == rank:
                local_tensor = torch.randn((1024, 1024), device=torch.npu.current_device())
                logging.info(f"[SRC WEIGHT]{torch.distributed.get_rank()} expert_weight: {torch.sum(local_tensor)}")
            else:
                # For other ranks, create empty tensor with correct shape
                local_tensor = torch.empty((1024, 1024), dtype=torch.float32, device=torch.npu.current_device())

        ptensor_list.append(
            PTensor(
                tensor=local_tensor,
                dtype=torch.float32,
                ndim=2,
                device_mesh=ptensor_device_mesh,
                global_size=(1024, 2048),
                shard_dim=1,
                rank=torch.distributed.get_rank(),
                backend='hccl'
            )
        )
    ptensor_set = PTensorSet(ptensor_list)
    ptensor_set.transfer_map(update_device_meshes)
    op = ptensor_set.get_transfer_list()
    executor.execute(op)
    executor.wait()

    tensor_list = ptensor_set.collect_tensor()
    tensor = tensor_list[0] if len(tensor_list) > 0 else None
    logging.info(f"[DST WEIGHT]{torch.distributed.get_rank()} {torch.sum(tensor)if tensor is not None else None}")


@pytest.mark.parametrize("old_device_meshes, update_device_meshes", [
    ([[0], [1], [2], [3]], [[0], [1], [2], [3]]),
])
def test_all2allv_executor_basic(base_distributed_args, old_device_meshes, update_device_meshes):
    """
    Test All2AllVExcutor basic functionality - maintaining original complexity.

    Args:
        base_distributed_args: Distributed environment fixture
        old_device_meshes (list): Original device mesh configurations
        update_device_meshes (list): Updated device mesh configurations
    """
    executor = All2AllVExcutor()
    ptensor_list = []

    for ptensor_device_mesh in old_device_meshes:
        # Create local tensor for the current rank
        local_tensor = None
        for rank in ptensor_device_mesh:
            if torch.distributed.get_rank() == rank:
                local_tensor = torch.randn((256, 256), device=torch.npu.current_device())
                logging.info(f"[SRC TENSOR]{torch.distributed.get_rank()} tensor shape: {local_tensor.shape}")
            else:
                # For other ranks, create empty tensor with correct shape
                local_tensor = torch.empty((256, 256), dtype=torch.float32, device=torch.npu.current_device())

        ptensor_list.append(
            PTensor(
                tensor=local_tensor,
                dtype=torch.float32,
                ndim=2,
                device_mesh=ptensor_device_mesh,
                global_size=(256, 256),
                shard_dim=0,
                rank=torch.distributed.get_rank(),
                backend='hccl'
            )
        )

    ptensor_set = PTensorSet(ptensor_list)
    ptensor_set.transfer_map(update_device_meshes)

    # Get communication task list
    task_list = ptensor_set.get_transfer_list()
    if len(task_list) > 0:
        # Execute All2AllV communication
        try:
            recv_tensor, _ = executor.execute(task_list)
            executor.wait()
            logging.info(f"[DST TENSOR]{torch.distributed.get_rank()} recv tensor shape:\
                   {recv_tensor.shape if recv_tensor is not None else None}")
        except Exception as e:
            logging.info(f"[ERROR] Basic test failed: {e}")
    else:
        logging.info(f"[NO TASKS]{torch.distributed.get_rank()} No tasks to execute")


@pytest.mark.parametrize("old_device_meshes, update_device_meshes",
                         [([[0], [1], [2], [3]], [[0], [1], [2], [3]])])
def test_all2allv_executor_async(base_distributed_args, old_device_meshes, update_device_meshes):
    """
    Test All2AllVExcutor asynchronous execution functionality - maintaining original complexity.

    Args:
        base_distributed_args: Distributed environment fixture
        old_device_meshes (list): Original device mesh configurations
        update_device_meshes (list): Updated device mesh configurations
    """
    executor = All2AllVExcutor()
    ptensor_list = []

    for ptensor_device_mesh in old_device_meshes:
        # Create local tensor for the current rank
        local_tensor = None
        for rank in ptensor_device_mesh:
            if torch.distributed.get_rank() == rank:
                local_tensor = torch.randn((256, 256), device=torch.npu.current_device())
                logging.info(f"[SRC TENSOR]{torch.distributed.get_rank()} tensor shape: {local_tensor.shape}")
            else:
                # For other ranks, create empty tensor with correct shape
                local_tensor = torch.empty((256, 256), dtype=torch.float32, device=torch.npu.current_device())

        ptensor_list.append(
            PTensor(
                tensor=local_tensor,
                dtype=torch.float32,
                ndim=2,
                device_mesh=ptensor_device_mesh,
                global_size=(256, 256),
                shard_dim=0,
                rank=torch.distributed.get_rank(),
                backend='hccl'
            )
        )

    ptensor_set = PTensorSet(ptensor_list)
    ptensor_set.transfer_map(update_device_meshes)

    # Get communication task list
    task_list = ptensor_set.get_transfer_list()
    if len(task_list) > 0:
        # Asynchronous execution
        try:
            recv_tensor, _ = executor.execute_async(task_list, task_list)
            executor.wait()
            logging.info(f"[DST TENSOR]{torch.distributed.get_rank()} async recv tensor shape:\
                   {recv_tensor.shape if recv_tensor is not None else None}")
        except Exception as e:
            logging.info(f"[ERROR] Async test failed: {e}")
    else:
        logging.info(f"[NO TASKS]{torch.distributed.get_rank()} No tasks to execute")


@pytest.mark.parametrize("old_device_meshes, update_device_meshes",
                         [([[0], [1], [2], [3]], [[0], [1], [2], [3], [4], [5], [6], [7]]),
                          ([[0]], [[0], [1], [2], [3], [4], [5], [6], [7]])])
def test_norm_mf(base_distributed_args, old_device_meshes, update_device_meshes):
    if bm is None:
        return 
    bm_config = MfBmConfig(
        mem_type=bm.BmMemType.DEVICE,
        data_op_type=bm.BmDataOpType.SDMA,
        local_dram_size=0,
        local_hbm_size=1024 * 1024 * 128
    )
    executor = MemoryFabricExecutor(bm_config, handle_id=0)
    torch.distributed.barrier()
    ptensor_list = []
    for ptensor_device_mesh in old_device_meshes:
        for rank in ptensor_device_mesh:
            if torch.distributed.get_rank() == rank:
                local_tensor = torch.nn.LayerNorm(1024, device=torch.npu.current_device()).weight
                break
            else:
                local_tensor = torch.empty((1024,), dtype=torch.float32, device=torch.npu.current_device())

        ptensor_list.append(PTensor(
            tensor=local_tensor,
            dtype=torch.float32,
            ndim=1,
            device_mesh=ptensor_device_mesh,
            global_size=(1024,),
            shard_dim=-1,
            rank=torch.distributed.get_rank(),
            backend='mf'))
    ptensor_set = PTensorSet(ptensor_list)
    ptensor_set.transfer_map(update_device_meshes)
    executor.execute(ptensor_set.get_transfer_list())
    tensor = ptensor_set.collect_tensor()
    logging.info(f"[TensorRTL] RANK:{torch.distributed.get_rank()} {len(tensor)=}"
          f"RECV{torch.sum(tensor[0]) if tensor is not None else None}")


@pytest.mark.parametrize("old_device_meshes, update_device_meshes",
                         [([[0, 1, 2, 3]], [[0, 1], [2, 3], [4, 5], [6, 7]])])
def test_qkv_mf(base_distributed_args, old_device_meshes, update_device_meshes):
    if bm is None:
        return 
    bm_config = MfBmConfig(
        mem_type=bm.BmMemType.DEVICE,
        data_op_type=bm.BmDataOpType.SDMA,
        local_dram_size=0,
        local_hbm_size=1024 * 1024 * 512
    )
    executor = MemoryFabricExecutor(bm_config, handle_id=0)
    torch.distributed.barrier()
    ptensor_list = []
    for ptensor_device_mesh in old_device_meshes:
        for rank in ptensor_device_mesh:
            if torch.distributed.get_rank() == rank:
                local_tensor = torch.nn.Linear(2048, 1536, device=torch.npu.current_device()).weight
                logging.info(f"[SRC WEIGHT]{torch.distributed.get_rank()} \
                    qkv: {torch.sum(local_tensor)} {local_tensor.size()}")
                break
            else:
                local_tensor = torch.empty((1536, 2048), dtype=torch.float32, device=torch.npu.current_device())

        ptensor_list.append(PTensor(
            tensor=local_tensor,
            dtype=torch.float32,
            ndim=2,
            device_mesh=ptensor_device_mesh,
            global_size=(6144, 2048),
            shard_dim=0,
            rank=torch.distributed.get_rank(),
            backend='mf'))

    ptensor_set = PTensorSet(ptensor_list)
    ptensor_set.transfer_map(update_device_meshes)
    executor.execute(ptensor_set.get_transfer_list())

    tensor = ptensor_set.collect_tensor()
    logging.info(
        f"[TensorRTL] RANK:{torch.distributed.get_rank()}  RECV{torch.sum(tensor) if tensor is not None else None}"
    )
