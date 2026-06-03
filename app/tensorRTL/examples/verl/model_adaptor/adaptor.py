
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

from abc import ABC, abstractmethod


class ModelAdatpor(ABC):
    def __init__(self, train_config, param_name_mapping=None, tp_partition_dict=None):
        self.train_config = train_config
        self.build_param_name_mapping(param_name_mapping)
        self.build_i2t_param_name_mapping()
        self.build_tp_partition_dict(tp_partition_dict)


    @abstractmethod
    def build_param_name_mapping(self, param_name_mapping):
        pass
        
    @abstractmethod
    def build_tp_partition_dict(self, tp_partition_dict):
        pass

    def build_i2t_param_name_mapping(self):
        if self.param_name_mapping is None:
            raise RuntimeError("[TensorRTL] Model param name mapping is None")
        i2t_param_name_mapping = {}
        for train_name, infer_name in self.param_name_mapping.items():
            i2t_param_name_mapping.setdefault(infer_name, train_name)
        self.i2t_param_name_mapping = i2t_param_name_mapping