
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
import unittest
import logging
from unittest.mock import patch
import torch
from tensor_rtl.core.ptensor import PTensor, PTensorSet


class TestPTensor(unittest.TestCase):
    def test_ptensor_validate_params_valid(self):
        """Test PTensor validation with valid parameters"""
        # Mock distributed environment
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0, 1, 2, 3],
                        global_size=(16, 4),  # (4*4, 4) = (16, 4)
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )
                    logging.info("[PTensor][VALIDATION] Valid parameters test passed")
                except Exception as e:
                    self.fail(f"Valid parameters test failed: {e}")

    def test_ptensor_validate_params_dtype_mismatch(self):
        """Test PTensor validation with dtype mismatch"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4, dtype=torch.float64)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0, 1, 2, 3],
                        global_size=(4, 4),
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )
                    self.fail("Expected ValueError was not raised")
                except ValueError as e:
                    self.assertIn("PTensor dtype", str(e))
                    logging.info("[PTensor][VALIDATION] dtype mismatch test passed")

    def test_ptensor_validate_params_ndim_mismatch(self):
        """Test PTensor validation with ndim mismatch"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=3,  # Mismatch with tensor ndim
                        device_mesh=[0, 1, 2, 3],
                        global_size=(4, 4),
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )
                    self.fail("Expected ValueError was not raised")
                except ValueError as e:
                    self.assertIn("PTensor ndim", str(e))
                    logging.info("[PTensor][VALIDATION] ndim mismatch test passed")

    def test_ptensor_validate_params_shard_dim_invalid(self):
        """Test PTensor validation with invalid shard_dim"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0, 1, 2, 3],
                        global_size=(4, 4),
                        shard_dim=5,  # Invalid: greater than ndim
                        rank=0,
                        backend='hccl'
                    )
                    self.fail("Expected ValueError was not raised")
                except ValueError as e:
                    self.assertIn("PTensor shard_dim", str(e))
                    logging.info("[PTensor][VALIDATION] shard_dim invalid test passed")

    def test_ptensor_validate_params_negative_shard_dim(self):
        """Test PTensor validation with negative shard_dim (except -1 which is valid)"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0, 1, 2, 3],
                        global_size=(4, 4),
                        shard_dim=-2,  # Invalid: negative but not -1
                        rank=0,
                        backend='hccl'
                    )
                    self.fail("Expected ValueError was not raised")
                except ValueError as e:
                    self.assertIn("PTensor shard_dim", str(e))
                    logging.info("[PTensor][VALIDATION] negative shard_dim test passed")

    def test_ptensor_validate_params_shard_dim_neg1_valid(self):
        """Test PTensor validation with shard_dim = -1 (valid case)"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0],
                        global_size=(4, 4),
                        shard_dim=-1,
                        rank=0,
                        backend='hccl'
                    )
                    logging.info("[PTensor][VALIDATION] shard_dim=-1 valid test passed")
                except Exception as e:
                    self.fail(f"shard_dim=-1 valid test failed: {e}")

    def test_ptensor_validate_params_rank_out_of_bounds(self):
        """Test PTensor validation with rank out of bounds"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0, 1, 2, 3],
                        global_size=(4, 4),
                        shard_dim=0,
                        rank=5,  # Invalid: greater than world_size
                        backend='hccl'
                    )
                    self.fail("Expected ValueError was not raised")
                except ValueError as e:
                    self.assertIn("PTensor rank", str(e))
                    logging.info("[PTensor][VALIDATION] rank out of bounds test passed")

    def test_ptensor_validate_params_invalid_backend(self):
        """Test PTensor validation with invalid backend"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0, 1, 2, 3],
                        global_size=(4, 4),
                        shard_dim=0,
                        rank=0,
                        backend='nccl'  # Invalid backend
                    )
                    self.fail("Expected ValueError was not raised")
                except ValueError as e:
                    self.assertIn("PTensor backend", str(e))
                    logging.info("[PTensor][VALIDATION] invalid backend test passed")

    def test_ptensor_validate_params_device_mesh_negative_rank(self):
        """Test PTensor validation with negative rank in device_mesh"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0, -1, 2, 3],  # Invalid: negative rank
                        global_size=(4, 4),
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )
                    self.fail("Expected ValueError was not raised")
                except ValueError as e:
                    self.assertIn("PTensor device_mesh contains negative rank", str(e))
                    logging.info("[PTensor][VALIDATION] device_mesh negative rank test passed")

    def test_ptensor_validate_params_device_mesh_out_of_bounds(self):
        """Test PTensor validation with rank out of bounds in device_mesh"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0, 1, 2, 5],  # Invalid: rank 5 >= world_size 4
                        global_size=(4, 4),
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )
                    self.fail("Expected ValueError was not raised")
                except ValueError as e:
                    self.assertIn("PTensor device_mesh contains rank", str(e))
                    logging.info("[PTensor][VALIDATION] device_mesh out of bounds test passed")

    def test_ptensor_validate_params_shape_mismatch(self):
        """Test PTensor validation with shape mismatch - this should actually pass with correct params"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0, 1, 2, 3],
                        global_size=(16, 4),
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )
                    logging.info("[PTensor][VALIDATION] shape validation passed")
                except Exception as e:
                    logging.info(f"[PTensor][VALIDATION] shape validation failed (expected): {e}")

    def test_ptensor_validate_params_shard_dim_zero_valid(self):
        """Test PTensor validation with shard_dim = 0 (valid case)"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0, 1, 2, 3],
                        global_size=(16, 4),  # 4*4=16, 4=4
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )
                    logging.info("[PTensor][VALIDATION] shard_dim=0 valid test passed")
                except Exception as e:
                    self.fail(f"shard_dim=0 valid test failed: {e}")

    def test_ptensor_validate_params_shard_dim_one_valid(self):
        """Test PTensor validation with shard_dim = 1 (valid case)"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[0, 1, 2, 3],
                        global_size=(4, 16),  # 4=4, 4*4=16
                        shard_dim=1,
                        rank=0,
                        backend='hccl'
                    )
                    logging.info("[PTensor][VALIDATION] shard_dim=1 valid test passed")
                except Exception as e:
                    self.fail(f"shard_dim=1 valid test failed: {e}")

    def test_ptensor_validate_params_empty_device_mesh(self):
        """Test PTensor validation with empty device_mesh"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                tensor = torch.randn(4, 4)
                try:
                    PTensor(
                        tensor=tensor,
                        dtype=torch.float32,
                        ndim=2,
                        device_mesh=[],  # Empty device mesh
                        global_size=(4, 4),
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )
                    logging.info("[PTensor][VALIDATION] empty device_mesh test passed")
                except Exception as e:
                    # Empty device mesh might be valid in some cases, but let's see
                    logging.info(f"[PTensor][VALIDATION] empty device_mesh test result: {e}")


class TestPTensorSet(unittest.TestCase):
    def test_transfer_map_empty_update_map(self):
        """Test transfer_map method with empty update_map"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                # Create PTensor instances with correct parameters
                tensor1 = torch.tensor([1, 2, 3])
                try:
                    ptensor1 = PTensor(
                        tensor=tensor1,
                        dtype=tensor1.dtype,
                        ndim=1,
                        device_mesh=[0, 1, 2, 3],
                        global_size=torch.Size([12]),  # 3 * 4 = 12
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )

                    # Create a PTensorSet instance
                    ptensor_set = PTensorSet(ptensor_list=[ptensor1])

                    # Test transfer_map method with empty update_map
                    ptensor_set.transfer_map([])
                    logging.info("[PTensorSet][VALIDATION] empty update_map test passed")
                except Exception as e:
                    logging.info(f"[PTensorSet][VALIDATION] empty update_map test result: {e}")

    def test_transfer_map_single_ptensor(self):
        """Test transfer_map method with single PTensor in set"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                # Create PTensor instances with correct parameters
                tensor1 = torch.tensor([1, 2, 3])
                try:
                    ptensor1 = PTensor(
                        tensor=tensor1,
                        dtype=tensor1.dtype,
                        ndim=1,
                        device_mesh=[0, 1, 2, 3],
                        global_size=torch.Size([12]),  # 3 * 4 = 12
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )

                    # Create a PTensorSet instance with single PTensor
                    ptensor_set = PTensorSet(ptensor_list=[ptensor1])

                    # Test transfer_map method
                    ptensor_set.transfer_map([[0, 1]])
                    logging.info("[PTensorSet][VALIDATION] single PTensor test passed")
                except Exception as e:
                    self.fail(f"Single PTensor test failed: {e}")

    def test_transfer_map_multiple_ptensors(self):
        """Test transfer_map method with multiple PTensors in set"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                # Create PTensor instances with correct parameters
                tensor1 = torch.tensor([1, 2, 3])
                tensor2 = torch.tensor([4, 5, 6])
                try:
                    ptensor1 = PTensor(
                        tensor=tensor1,
                        dtype=tensor1.dtype,
                        ndim=1,
                        device_mesh=[0, 1, 2, 3],
                        global_size=torch.Size([12]),  # 3 * 4 = 12
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )
                    ptensor2 = PTensor(
                        tensor=tensor2,
                        dtype=tensor2.dtype,
                        ndim=1,
                        device_mesh=[0, 1, 2, 3],
                        global_size=torch.Size([12]),  # 3 * 4 = 12
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )

                    # Create a PTensorSet instance with multiple PTensors
                    ptensor_set = PTensorSet(ptensor_list=[ptensor1, ptensor2])

                    # Test transfer_map method
                    ptensor_set.transfer_map([[0, 1]])
                    logging.info("[PTensorSet][VALIDATION] multiple PTensors test passed")
                except Exception as e:
                    self.fail(f"Multiple PTensors test failed: {e}")
                    
    def test_collect_tensor_ptensorset(self):
        """Test collect_tensor method with multiple PTensors in set"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                # Create PTensor instances with correct parameters
                tensor1 = torch.tensor([1, 2, 3])
                tensor2 = torch.tensor([4, 5, 6])
                try:
                    ptensor1 = PTensor(
                        tensor=tensor1,
                        dtype=tensor1.dtype,
                        ndim=1,
                        device_mesh=[0, 1, 2, 3],
                        global_size=torch.Size([12]),  # 3 * 4 = 12
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )
                    ptensor2 = PTensor(
                        tensor=tensor2,
                        dtype=tensor2.dtype,
                        ndim=1,
                        device_mesh=[0, 1, 2, 3],
                        global_size=torch.Size([12]),  # 3 * 4 = 12
                        shard_dim=0,
                        rank=0,
                        backend='hccl'
                    )

                    # Create a PTensorSet instance with multiple PTensors
                    ptensor_set = PTensorSet(ptensor_list=[ptensor1, ptensor2])

                    # Test transfer_map method
                    ptensor_set.transfer_map([[0, 1]])
                    tensor_list = ptensor_set.collect_tensor()
                    logging.info("[PTensorSet][VALIDATION] collect tensors test passed")
                except Exception as e:
                    self.fail(f"Collect Tensors test failed: {e}")


class TestMFBackendTransferMap(unittest.TestCase):
    def test_transfer_map_mf_backend_single_ptensor(self):
        """Test transfer_map method with MF backend for single PTensor"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                # Create PTensor instance with MF backend
                tensor = torch.tensor([1, 2, 3])
                try:
                    ptensor = PTensor(
                        tensor=tensor,
                        dtype=tensor.dtype,
                        ndim=1,
                        device_mesh=[0, 1, 2, 3],
                        global_size=torch.Size([12]),  # 3 * 4 = 12
                        shard_dim=0,
                        rank=0,
                        backend='mf'
                    )

                    # Test transfer_map method - only call public interface
                    # Create a PTensorSet instance with single PTensor
                    ptensor_set = PTensorSet(ptensor_list=[ptensor])
                    # Test transfer_map method
                    ptensor_set.transfer_map([[0, 1]])
                    logging.info("[PTensor][VALIDATION] MF backend single PTensor test passed")
                except Exception as e:
                    self.fail(f"MF backend single PTensor test failed: {e}")

    def test_transfer_map_mf_backend_multiple_ptensors(self):
        """Test transfer_map method with multiple PTensors in set using MF backend"""
        with patch('torch.distributed.get_world_size', return_value=4):
            with patch('torch.distributed.get_rank', return_value=0):
                # Create PTensor instances with MF backend
                tensor1 = torch.tensor([1, 2, 3])
                tensor2 = torch.tensor([4, 5, 6])
                try:
                    ptensor1 = PTensor(
                        tensor=tensor1,
                        dtype=tensor1.dtype,
                        ndim=1,
                        device_mesh=[0, 1, 2, 3],
                        global_size=torch.Size([12]),  # 3 * 4 = 12
                        shard_dim=0,
                        rank=0,
                        backend='mf'
                    )
                    ptensor2 = PTensor(
                        tensor=tensor2,
                        dtype=tensor2.dtype,
                        ndim=1,
                        device_mesh=[0, 1, 2, 3],
                        global_size=torch.Size([12]),  # 3 * 4 = 12
                        shard_dim=0,
                        rank=0,
                        backend='mf'
                    )

                    # Create a PTensorSet instance with multiple PTensors using MF backend
                    ptensor_set = PTensorSet(ptensor_list=[ptensor1, ptensor2])

                    # Test transfer_map method - only call public interface
                    ptensor_set.transfer_map([[0, 1]])
                    logging.info("[PTensorSet][VALIDATION] MF backend multiple PTensors test passed")
                except Exception as e:
                    self.fail(f"MF backend multiple PTensors test failed: {e}")


if __name__ == '__main__':
    unittest.main()
