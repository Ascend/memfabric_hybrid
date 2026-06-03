
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import List


@dataclass
class BaseCommTask(ABC):
    pass


class Executor(ABC):
    def __init__(self):
        self.batch_comm_list: List = []

    @abstractmethod
    def execute(self, tasks: List[BaseCommTask]):
        """执行顺序通信

        Args:
            task: 通信算子列表
        Returns:
            通信handle
        """
        pass

    @abstractmethod
    def wait(self):
        """完成所有的task"""
        pass


class Planner(ABC):
    """P2P通信策略容器, 需要存储通信状态如P2P流, Planner的抽象类"""
    def __init__(self, topology=None):
        self.topology = topology

    @abstractmethod
    def build_ordered_comm_ops(self, op_list: List[BaseCommTask]) -> List[BaseCommTask]:
        """接受无序CommOps，优化通信

        Args:
            op_list: 通信算子列表
        Returns:
            编排过的通信算子列表

        Raises:
            ValueError: op的rank超出范围
        """
        pass
