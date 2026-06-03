
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
import unittest
import logging
from unittest.mock import patch
import torch
from tensor_rtl.core.executor import BatchP2PExecutor, All2AllVExcutor
from tensor_rtl.core.comm_task import P2PCommTask


class Handle:
    def wait(self):
        pass


class Op:
    def __init__(self, is_send=True, buffer=None, rank=0):
        self.is_send = is_send
        self.buffer = buffer
        self.rank = rank


class TestBatchP2PExecutor(unittest.TestCase):
    def test_batchp2pexcutor_execute_single_send_task(self):
        with patch('torch.distributed.batch_isend_irecv', return_value=[Handle()]):
            with patch('torch.distributed.get_rank', return_value=0):
                with patch('torch.distributed.P2POp', return_value=Op()):
                    executor = BatchP2PExecutor()
                    tensor = torch.tensor([1, 2, 3])
                    try:
                        task = P2PCommTask(idx=0,
                                        numel=tensor.numel(),
                                        is_send=True,
                                        src_rank=0,
                                        dst_rank=1,
                                        buffer=tensor)
                        executor.execute([task])
                        executor.wait()
                        logging.info("[BatchP2PExecutor][VALIDATION] BatchP2PExecutor execute test passed")
                    except Exception as e:
                        self.fail(f"BatchP2PExecutor send task execute test failed: {e}")

    def test_batchp2pexcutor_execute_single_recv_task(self):
        with patch('torch.distributed.batch_isend_irecv', return_value=[Handle()]):
            with patch('torch.distributed.get_rank', return_value=0):
                with patch('torch.distributed.P2POp', return_value=Op()):
                    executor = BatchP2PExecutor()
                    tensor = torch.tensor([1, 2, 3])
                    try:
                        task = P2PCommTask(idx=0,
                                        numel=tensor.numel(),
                                        is_send=False,
                                        src_rank=0,
                                        dst_rank=1,
                                        buffer=tensor)
                        executor.execute([task])
                        executor.wait()
                        logging.info("[BatchP2PExecutor][VALIDATION] BatchP2PExecutor execute test passed")
                    except Exception as e:
                        self.fail(f"BatchP2PExecutor recv task execute test failed: {e}")
    
    def test_batchp2pexcutor_execute_self_task(self):
        with patch('torch.distributed.batch_isend_irecv', return_value=[Handle()]):
            with patch('torch.distributed.get_rank', return_value=0):
                with patch('torch.distributed.P2POp', return_value=Op()):
                    executor = BatchP2PExecutor()
                    tensor = torch.tensor([1, 2, 3])
                    try:
                        task = P2PCommTask(idx=0,
                                        numel=tensor.numel(),
                                        is_send=True,
                                        src_rank=0,
                                        dst_rank=0,
                                        buffer=tensor)
                        executor.execute([task])
                        executor.wait()
                        logging.info("[BatchP2PExecutor][VALIDATION] BatchP2PExecutor execute empty_task test passed")
                    except Exception as e:
                        self.fail(f"BatchP2PExecutor self task execute test failed: {e}")
                    
    def test_batchp2pexcutor_execute_no_task(self):
        with patch('torch.distributed.batch_isend_irecv', return_value=[Handle()]):
            with patch('torch.distributed.get_rank', return_value=0):
                with patch('torch.distributed.P2POp', return_value=Op()):
                    try:
                        executor = BatchP2PExecutor()
                        executor.execute([])
                        executor.wait()
                        logging.info("[BatchP2PExecutor][VALIDATION] BatchP2PExecutor execute empty_task test passed")
                    except Exception as e:
                        self.fail(f"BatchP2PExecutor no task execute test failed: {e}")


class TestAll2ALLExecutor(unittest.TestCase):
    def test_all2allexcutor_execute_task(self):
        with patch('torch.distributed.all_to_all_single', return_value=[Handle()]):
            with patch('torch.distributed.get_rank', return_value=0):
                with patch('torch.distributed.get_world_size', return_value=8):
                    executor = All2AllVExcutor()
                    tensor1 = torch.tensor([1, 2, 3])
                    tensor2 = torch.tensor([1, 2, 3])
                    tensor3 = torch.tensor([1, 2, 3])
                    tasks = []
                    send = True
                    for rank, tensor in enumerate([tensor1, tensor2, tensor3]):
                        tasks.append(P2PCommTask(idx=0,
                                        numel=tensor.numel(),
                                        is_send=send,
                                        src_rank=0,
                                        dst_rank=rank,
                                        buffer=tensor))
                        send = not send
                    try:
                        tensors, tasks = executor.execute(tasks)
                        executor.wait()
                        logging.info("[All2AllVExcutor][VALIDATION] All2AllVExcutor execute  test passed")
                    except Exception as e:
                        self.fail(f"All2AllVExcutor execute test failed: {e}")

    def test_all2allexcutor_execute_async_task(self):
        with patch('torch.distributed.all_to_all_single', return_value=[Handle()]):
            with patch('torch.distributed.get_rank', return_value=0):
                with patch('torch.distributed.get_world_size', return_value=8):
                    executor = All2AllVExcutor()
                    tensor1 = torch.tensor([1, 2, 3])
                    tensor2 = torch.tensor([1, 2, 3])
                    tensor3 = torch.tensor([1, 2, 3])
                    tasks = []
                    send = True
                    for rank, tensor in enumerate([tensor1, tensor2, tensor3]):
                        tasks.append(P2PCommTask(idx=0,
                                        numel=tensor.numel(),
                                        is_send=send,
                                        src_rank=0,
                                        dst_rank=rank,
                                        buffer=tensor))
                        send = not send
                    try:
                        tensors, tasks = executor.execute_async(tasks, [])
                        executor.wait()
                        logging.info("[All2AllVExcutor][VALIDATION] All2AllVExcutor execute async test passed")
                    except Exception as e:
                        self.fail(f"All2AllVExcutor execute test failed: {e}")      

if __name__ == '__main__':
    unittest.main()
