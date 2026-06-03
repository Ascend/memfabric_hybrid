
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.

class BufferPool:
    """
    To avoid create communication tensor repeatively, we create buffer pool to store buffer.
    """

    def __init__(self):
        self.send_buffer_dict = {}

    def register(self, length_tuple, tensor_slice, buffer):
        self.send_buffer_dict.setdefault(length_tuple, {})
        self.send_buffer_dict[length_tuple].setdefault(tensor_slice, buffer)

    def get(self, length_tuple, tensor_slice):
        tensor = self.send_buffer_dict.get(length_tuple, {}).get(tensor_slice, None)
        return tensor
    
    def get_keys(self):
        return self.send_buffer_dict.keys()