from enum import Enum
import unittest

import torch
import torch_npu

from pymmc import DistributedObjectStore


class MmcDirect(Enum):
    COPY_L2G = 0
    COPY_G2L = 1
    COPY_G2H = 2
    COPY_H2G = 3


class TestExample(unittest.TestCase):
    layer_number = 10
    block_number = 10
    block_size = 1024

    @classmethod
    def setUpClass(cls):
        cls.store = DistributedObjectStore()
        res = cls.store.init(0)
        print(f"object store init res: {res}")

        cls.cpu_tensor = torch.empty(
            size=(cls.layer_number, cls.block_number, cls.block_size),
            dtype=torch.uint8,
            device=torch.device('cpu')
        )
        cls.cpu_blocks = []
        for block_id in range(cls.block_number):
            block = [cls.cpu_tensor[i][block_id] for i in range(cls.layer_number)]
            cls.cpu_blocks.append(block)

        cls.npu_tensor = torch.empty(
            size=(cls.layer_number, cls.block_number, cls.block_size),
            dtype=torch.uint8,
            device=torch.device('npu')
        )
        cls.npu_blocks = []
        for block_id in range(cls.block_number):
            block = [cls.npu_tensor[i][block_id] for i in range(cls.layer_number)]
            cls.npu_blocks.append(block)

    def test_equidistant(self):
        self.npu_tensor[0][0] = 123
        print(self.npu_tensor[0][0])
        res = self.store.put_from_layers("2d",
                                         [layer.data_ptr() for layer in self.npu_blocks[0]],
                                         [self.block_size] * self.layer_number,
                                         MmcDirect.COPY_L2G.value)
        self.assertEqual(res, 0)

        res = self.store.get_into_layers("2d",
                                         [layer.data_ptr() for layer in self.npu_blocks[1]],
                                         [self.block_size] * self.layer_number,
                                         MmcDirect.COPY_G2L.value)
        self.assertEqual(res, 0)

        self.assertTrue(self.npu_tensor[0][0].eq(self.npu_tensor[0][1]).all())
        print(self.npu_tensor[0][1])

    def test_non_equidistant(self):
        src_layers = [
            torch.full(size=(3,), fill_value=3, dtype=torch.uint8),
            torch.full(size=(4,), fill_value=4, dtype=torch.uint8),
            torch.full(size=(5,), fill_value=5, dtype=torch.uint8),
        ]
        dst_layers = [
            torch.zeros(size=(3,), dtype=torch.uint8),
            torch.zeros(size=(4,), dtype=torch.uint8),
            torch.zeros(size=(5,), dtype=torch.uint8),
        ]
        print(src_layers)
        res = self.store.put_from_layers("not-2d",
                                         [layer.data_ptr() for layer in src_layers],
                                         [3, 4, 5],
                                         MmcDirect.COPY_H2G.value)
        self.assertEqual(res, 0)
        res = self.store.get_into_layers("not-2d",
                                         [layer.data_ptr() for layer in dst_layers],
                                         [3, 4, 5],
                                         MmcDirect.COPY_G2H.value)
        self.assertEqual(res, 0)
        print(dst_layers)

    @classmethod
    def tearDownClass(cls):
        cls.store.close()
        print("object store destroyed")


if __name__ == '__main__':
    unittest.main()
