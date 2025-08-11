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
        res = cls.store.init()
        print(f"object store init res: {res}")

        cls.npu_tensor = torch.empty(
            size=(cls.layer_number, cls.block_number, cls.block_size),
            dtype=torch.uint8,
            device=torch.device('npu')
        )
        cls.npu_blocks = []
        for block_id in range(cls.block_number):
            block = [cls.npu_tensor[i][block_id] for i in range(cls.layer_number)]
            cls.npu_blocks.append(block)

    def test_equidistant_batch(self):
        self.npu_tensor[0][2] = 2
        self.npu_tensor[0][3] = 3
        print(self.npu_tensor[0][2])
        print(self.npu_tensor[0][3])
        print(self.npu_tensor[0][4])
        print(self.npu_tensor[0][5])
        res = self.store.batch_put_from_layers(
            ["2d-0", "2d-1"],
            [
                [layer.data_ptr() for layer in self.npu_blocks[2]],
                [layer.data_ptr() for layer in self.npu_blocks[3]]
            ],
            [
                [self.block_size for _ in range(self.layer_number)],
                [self.block_size for _ in range(self.layer_number)]
            ],
            MmcDirect.COPY_L2G.value
        )
        self.assertTrue(all(i==0 for i in res))
        res = self.store.batch_get_into_layers(
            ["2d-0", "2d-1"],
            [
                [layer.data_ptr() for layer in self.npu_blocks[4]],
                [layer.data_ptr() for layer in self.npu_blocks[5]]
            ],
            [
                [self.block_size for _ in range(self.layer_number)],
                [self.block_size for _ in range(self.layer_number)]
            ],
            MmcDirect.COPY_G2L.value
        )
        self.assertTrue(all(i==0 for i in res))
        self.assertTrue(self.npu_tensor[0][2].eq(self.npu_tensor[0][4]).all())
        self.assertTrue(self.npu_tensor[0][3].eq(self.npu_tensor[0][5]).all())
        print(self.npu_tensor[0][2])
        print(self.npu_tensor[0][3])
        print(self.npu_tensor[0][4])
        print(self.npu_tensor[0][5])

    def test_non_equidistant(self):
        src_blocks = [
            [
                torch.full(size=(2,), fill_value=2, dtype=torch.uint8),
                torch.full(size=(3,), fill_value=3, dtype=torch.uint8),
            ],
            [
                torch.full(size=(3,), fill_value=3, dtype=torch.uint8),
                torch.full(size=(4,), fill_value=4, dtype=torch.uint8),
                torch.full(size=(5,), fill_value=5, dtype=torch.uint8),
            ]
        ]
        dst_blocks = [
            [
                torch.zeros(size=(2,), dtype=torch.uint8),
                torch.zeros(size=(3,), dtype=torch.uint8),
            ],
            [
                torch.zeros(size=(3,), dtype=torch.uint8),
                torch.zeros(size=(4,), dtype=torch.uint8),
                torch.zeros(size=(5,), dtype=torch.uint8),
            ]
        ]
        print(src_blocks)
        print(dst_blocks)
        res = self.store.batch_put_from_layers(
            ["1d-0", "1d-1"],
            [[layer.data_ptr() for layer in block] for block in src_blocks],
            [
                [2, 3],
                [3, 4, 5]
            ],
            MmcDirect.COPY_H2G.value
        )
        self.assertTrue(all(i==0 for i in res))
        res = self.store.batch_get_into_layers(
            ["1d-0", "1d-1"],
            [[layer.data_ptr() for layer in block] for block in dst_blocks],
            [
                [2, 3],
                [3, 4, 5]
            ],
            MmcDirect.COPY_G2H.value
        )
        self.assertTrue(all(i==0 for i in res))
        print(src_blocks)
        print(dst_blocks)

    @classmethod
    def tearDownClass(cls):
        cls.store.close()
        print("object store destroyed")


if __name__ == '__main__':
    unittest.main()
