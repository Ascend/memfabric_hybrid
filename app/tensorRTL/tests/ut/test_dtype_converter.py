
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
import unittest
import torch
from tensor_rtl.utils.dtype_converter import pack_tensors, unpack_tensors


class TestTensorPacking(unittest.TestCase):
    def test_pack_tensors(self):
        """
        Test the pack_tensors function with normal tensors.

        This test verifies that tensors can be properly packed into a single tensor
        with metadata preservation.
        """
        # Create test tensors
        tensor1 = torch.tensor([1, 2, 3], dtype=torch.float32)
        tensor2 = torch.tensor([4, 5, 6], dtype=torch.float32)
        tensor_list = [tensor1, tensor2]

        # Pack tensors
        packed_tensor, metadata = pack_tensors(tensor_list, target_dtype=torch.int8)

        # Verify packed tensor and metadata
        self.assertEqual(packed_tensor.dtype, torch.int8)
        self.assertEqual(len(metadata['original_shapes']), 2)
        self.assertEqual(metadata['original_shapes'][0], tensor1.shape)
        self.assertEqual(metadata['original_shapes'][1], tensor2.shape)
        self.assertEqual(metadata['original_dtypes'][0], torch.float32)
        self.assertEqual(metadata['original_dtypes'][1], torch.float32)

    def test_unpack_tensors(self):
        """
        Test the unpack_tensors function with packed tensors.

        This test verifies that packed tensors can be properly unpacked back
        to their original form.
        """
        # Create test tensors
        tensor1 = torch.tensor([1, 2, 3], dtype=torch.float32)
        tensor2 = torch.tensor([4, 5, 6], dtype=torch.float32)
        tensor_list = [tensor1, tensor2]

        # Pack tensors
        packed_tensor, metadata = pack_tensors(tensor_list, target_dtype=torch.int8)

        # Unpack tensors
        unpacked_tensors = unpack_tensors(packed_tensor, metadata)

        # Verify unpacked tensors
        self.assertEqual(len(unpacked_tensors), 2)
        self.assertTrue(torch.allclose(unpacked_tensors[0], tensor1))
        self.assertTrue(torch.allclose(unpacked_tensors[1], tensor2))

    def test_empty_tensor_list(self):
        """
        Test pack_tensors with empty tensor list.

        This test verifies that an appropriate error is raised when trying to
        pack an empty list of tensors.
        """
        # Test empty tensor list
        with self.assertRaises(ValueError):
            pack_tensors([], target_dtype=torch.int8)

    def test_non_contiguous_tensor(self):
        """
        Test pack/unpack with non-contiguous tensors.

        This test verifies that the packing/unpacking works correctly with
        non-contiguous tensor views.
        """
        # Test non-contiguous tensor
        tensor1 = torch.tensor([[1, 2], [3, 4]], dtype=torch.float32)
        tensor1 = tensor1[:, 0]  # This creates a non-contiguous tensor
        tensor_list = [tensor1]

        packed_tensor, metadata = pack_tensors(tensor_list, target_dtype=torch.int8)
        unpacked_tensors = unpack_tensors(packed_tensor, metadata)

        self.assertTrue(torch.allclose(unpacked_tensors[0], tensor1))


if __name__ == '__main__':
    unittest.main()
