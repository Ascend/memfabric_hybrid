#!/usr/bin/env python3.10
#  Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.

import ast
import concurrent.futures
import dataclasses
import inspect
import json
import socket
import sys
import threading
import traceback
from functools import wraps
from pickletools import uint8
from time import sleep
from typing import Callable, Dict, List
from enum import Enum

import re
from typing import Dict, Any, Optional

import torch
import torch_npu

import memfabric_hybrid
from memfabric_hybrid import bm


class MmcDirect(Enum):
    COPY_L2G = 0
    COPY_G2L = 1
    COPY_G2H = 2
    COPY_H2G = 3
    COPY_G2G = 3


@dataclasses.dataclass
class CliCommand:
    cmd_name: str
    cmd_desc: str
    func: Callable
    required_args_num: int


@dataclasses.dataclass
class SmemBmClientConfig:
    config_store_url: str
    world_size: int
    protocol: str
    dram_size: int
    hbm_size: int
    nic: str


def _convert_value(value: str) -> Any:
    value = value.strip()
    if value.lower() == 'true':
        return True
    elif value.lower() == 'false':
        return False
    if value.isdigit():
        return int(value)
    try:
        return float(value)
    except ValueError:
        pass
    return value


class FlatConfigParser:
    def __init__(self, filepath: str):
        self.filepath = filepath
        self.config: Dict[str, Any] = {}
        self._parse()

    def __getitem__(self, key: str) -> Any:
        """支持 dict 风格访问：config['key']"""
        if key not in self.config:
            raise KeyError(f"未找到配置项：{key}")
        return self.config[key]

    def __contains__(self, key: str) -> bool:
        return key in self.config

    def __repr__(self):
        return f"FlatConfigParser({self.config})"

    def get(self, key: str, default: Any = None) -> Any:
        """获取配置值，支持默认值"""
        return str(self.config.get(key, default))

    def _parse(self):
        pattern = re.compile(r'^\s*([^#\s]+)\s*=\s*(.+?)(?:\s*#.*|;\s*|$)')
        try:
            with open(self.filepath, 'r', encoding='utf-8') as f:
                for _, line in enumerate(f, 1):
                    line = line.strip()
                    if not line or line.startswith('#') or line.startswith(';'):
                        continue

                    match = pattern.match(line)
                    if match:
                        key = match.group(1).strip()
                        value = match.group(2).strip()
                        self.config[key] = _convert_value(value)
        except FileNotFoundError as e:
            raise FileNotFoundError(f"配置文件未找到：{self.filepath}") from e
        except Exception as e:
            raise RuntimeError(f"读取配置文件失败：{e}") from e


class TestServer:
    def __init__(self, ip, port):
        self._ip_port = (ip, int(port))
        self._server_socket = None
        self._commands: Dict[str:CliCommand] = {}
        self._thread_local = threading.local()
        self._thread_local.client_socket = None
        self._register_inner_command()

    def __del__(self):
        self._server_socket.close()

    @staticmethod
    # 解析参数并根据目标函数的参数类型进行转换
    def _parse_arguments(func, arg_strs):
        signature = inspect.signature(func)
        parsed_args = []
        for param, arg in zip(signature.parameters.values(), arg_strs):
            parsed_args.append(TestServer._convert_argument(arg, param.annotation))
        return parsed_args

    @staticmethod
    def _convert_argument(arg_str: str, param_type):
        try:
            if arg_str == "__NONE__":
                return None
            elif param_type == int:
                return int(arg_str)
            elif param_type == float:
                return float(arg_str)
            elif param_type == str:
                return str(arg_str)
            elif param_type == bool:
                return arg_str.lower() in ['true', '1', 'yes']
            elif param_type == bytes:
                return bytes(arg_str, 'utf-8')
            else:
                # 如果是其他类型，可以使用 ast.literal_eval
                return ast.literal_eval(arg_str)
        except (ValueError, SyntaxError):
            # 如果无法转换，返回原始字符串
            return arg_str

    def start(self):
        self._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server_socket.bind(self._ip_port)
        self._server_socket.listen(5)
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            while True:
                client_socket, _ = self._server_socket.accept()
                executor.submit(self._handle_client, client_socket)

    def register_command(self, cmds: List[CliCommand]):
        for cmd in cmds:
            self._commands[cmd.cmd_name] = cmd

    def cli_print(self, msg: str):
        if self._thread_local.client_socket is not None:
            self._thread_local.client_socket.send(f"{msg}\n".encode('utf-8'))

    def cli_return(self, obj):
        obj_type = type(obj)
        if obj_type is int:
            data = str(obj).encode('utf-8')
        elif obj_type is bytes:
            data = obj
        else:
            data = str(obj).encode('utf-8')
        if self._thread_local.client_socket is not None:
            self._thread_local.client_socket.send(data)

    def _register_inner_command(self):
        self._commands = {
            "help": CliCommand("help", "show command list information", self._help, 0),
            "getServerCommands": CliCommand("getServerCommands", "getServerCommands, get the registered Commands",
                                            self._get_server_commands, 0),
        }

    def _cli_end_line(self):
        print("send command result")
        self._thread_local.client_socket.send("\0".encode('utf-8'))

    def _help(self):
        """显示帮助信息。"""
        col_widths = max(len(item) for item in self._commands.keys()) + 1
        self.cli_print("Available commands:")
        for cmd in self._commands.values():
            self.cli_print(f":  {cmd.cmd_name: >{col_widths}}: {cmd.cmd_desc}")

    def _get_server_commands(self):
        msg = ",".join(self._commands.keys())
        self.cli_print(f"{msg}")

    def _execute(self, request):
        """执行命令。"""
        cmd_str = request.get("cmd")
        args = request.get("args")
        command = self._commands.get(cmd_str)
        if not command:
            self.cli_print(f"Unknown command: {cmd_str}")
            self._help()
            self._cli_end_line()
            return
        if len(args) < command.required_args_num:
            self.cli_print(f"Invalid input args num: {len(args)}, required args num: {command.required_args_num}")
            self._help()
            self._cli_end_line()
            return
        parsed_params = self._parse_arguments(command.func, args)
        command.func(*parsed_params)
        self._cli_end_line()

    def _handle_client(self, client_socket: socket):
        self._thread_local.client_socket = client_socket
        buffer_list = []
        try:
            while True:
                data = client_socket.recv(1024)
                if not data:
                    self._thread_local.client_socket = None
                    break
                buffer_list.append(data)
                if not data.endswith(b"\0"):
                    continue
                msg = b''.join(buffer_list).decode('utf-8').replace("\0", "").strip()
                request = json.loads(msg)
                print(f"received request: {request}")

                try:
                    self._execute(request)
                except Exception:
                    traceback.print_exc()
                finally:
                    buffer_list.clear()
        finally:
            client_socket.close()


def result_handler(func):
    @wraps(func)
    def wrapper(self, *args, **kwargs):
        try:
            func(self, *args, **kwargs)
        except Exception as e:
            self.cli_print(f"{func.__name__} raised exception: {e}")

    return wrapper


def tensor_sum(tensor, sizes: List[int] = None):
    if tensor is None:
        return 0
    if sizes is None:
        return tensor.sum().item()

    return sum(layer[:size].sum().item() for layer, size in zip(tensor, sizes))


def size_to_bytes(size_str: str) -> int:
    units = {
        'B': 1,
        'KB': 1024,
        'MB': 1024 ** 2,
        'GB': 1024 ** 3,
        'TB': 1024 ** 4,
    }
    size_str = size_str.strip().upper()
    match = re.match(r'^(\d+)([KMGTP]?B?)$', size_str)
    if not match:
        raise ValueError(f"无法解析大小格式：{size_str}")

    num, unit = match.groups()
    num = int(num)
    if not unit:
        unit = 'B'
    elif unit.endswith('B'):
        unit = unit
    elif unit == 'K':
        unit = 'KB'
    elif unit == 'M':
        unit = 'MB'
    elif unit == 'G':
        unit = 'GB'
    elif unit == 'T':
        unit = 'TB'
    else:
        raise ValueError(f"不支持的单位：{unit}")
    return num * units.get(unit)


class MmcTest(TestServer):
    def __init__(self, ip, port, device_id=0, rank_id=0):
        super().__init__(ip, port)
        self._dram_gva = None
        self._host_gva = None
        self._rank_id = rank_id
        self._device_id = device_id
        self._init_cmds()
        self._bm_handle = None
        self._tensor_dict: dict = {}

    def set_device(self):
        import acl
        acl.init()
        ret = acl.rt.set_device(self._device_id)
        if ret != 0:
            raise RuntimeError("acl set device failed")

    def sync_stream(self):
        torch_npu.npu.current_stream().synchronize()

    def malloc_tensor(self, layer_num: int = 1, mini_block_size: int = 1024, device='cpu'):
        if device not in ('cpu', 'npu'):
            raise RuntimeError(f"invalid device: {device}")
        if mini_block_size <= 0:
            return None

        if device == 'npu':
            self.set_device()
        raw_blocks = torch.randint(
            low=0, high=256,
            size=(layer_num, mini_block_size),
            dtype=torch.uint8,
            device=torch.device(device)
        )
        if device == 'npu':
            self.sync_stream()
        return raw_blocks

    def read_client_conf(self) -> SmemBmClientConfig:
        import os
        conf_path = os.getenv('MMC_LOCAL_CONFIG_PATH')
        if conf_path is None:
            raise FileNotFoundError(f"配置文件未找到：{conf_path}")
        config = FlatConfigParser(conf_path)
        smem_config = SmemBmClientConfig(
            config_store_url=config.get('ock.mmc.local_service.config_store_url'),
            world_size=int(config.get('ock.mmc.local_service.world_size')),
            protocol=config.get('ock.mmc.local_service.protocol'),
            dram_size=size_to_bytes(config.get('ock.mmc.local_service.dram.size')),
            hbm_size=size_to_bytes(config.get('ock.mmc.local_service.hbm.size')),
            nic=str(config.get('ock.mmc.local_service.hcom_url'))
        )
        return smem_config

    @result_handler
    def init_smem_bm(self):
        # 读取配置文件
        client_config = self.read_client_conf()
        # init
        ret = memfabric_hybrid.initialize()
        if ret != 0:
            self.cli_return(-1)
            raise RuntimeError(f"Failed to init memfabric_hybrid")
        config = bm.BmConfig()
        config.auto_ranking = False
        config.rank_id = self._rank_id
        config.set_nic(client_config.nic)  # for device port
        if client_config.protocol == 'host_rdma':
            op_type = bm.BmDataOpType.HOST_RDMA
            config.flags = 0
        elif client_config.protocol == 'host_urma':
            op_type = bm.BmDataOpType.HOST_URMA
            config.flags = 0
        elif client_config.protocol == 'host_tcp':
            op_type = bm.BmDataOpType.HOST_TCP
            config.flags = 0
        elif client_config.protocol == 'device_rdma':
            op_type = bm.BmDataOpType.DEVICE_RDMA
            config.flags = 2
        elif client_config.protocol == 'device_sdma':
            op_type = bm.BmDataOpType.SDMA
            config.flags = 2
        else:
            op_type = bm.BmDataOpType.DEVICE_RDMA
            config.flags = 2
        ret = bm.initialize(store_url=client_config.config_store_url,
                            world_size=client_config.world_size,
                            device_id=self._device_id,
                            config=config)
        if ret != 0:
            raise RuntimeError(f"Failed to init bm")
        try:
            # create
            self._bm_handle = bm.create(id=0,
                                        local_dram_size=client_config.dram_size,
                                        local_hbm_size=client_config.hbm_size,
                                        data_op_type=op_type)
            # join
            self._bm_handle.join()
        except Exception as e:
            self.cli_return(-1)
            self.cli_print(f"raised exception: {e}")

        self.cli_return(0)

    @result_handler
    def close_smem_bm(self):
        del self._bm_handle
        self.cli_return(0)

    @result_handler
    def copy_data(self, src_ptr: int, dst_ptr: int, size: int, op_type_str: str, flag: int):
        if op_type_str == 'H2G':
            op_type = bm.BmCopyType.H2G
        elif op_type_str == 'L2G':
            op_type = bm.BmCopyType.L2G
        elif op_type_str == 'G2H':
            op_type = bm.BmCopyType.G2H
        elif op_type_str == 'G2L':
            op_type = bm.BmCopyType.G2L
        elif op_type_str == 'G2G':
            op_type = bm.BmCopyType.G2G
        elif op_type_str == 'AUTO':
            op_type = bm.BmCopyType.AUTO
        else:
            raise f"Invalid op type({op_type_str})"
        ret = self._bm_handle.copy_data(src_ptr, dst_ptr, size, op_type, flag)
        self.cli_return(ret)

    @result_handler
    def copy_data_batch(self, src_addrs: list[int], dst_addrs: list[int],
                        sizes: list[int], count: int, op_type_str: str, flag: int):
        if op_type_str == 'H2G':
            op_type = bm.BmCopyType.H2G
        elif op_type_str == 'L2G':
            op_type = bm.BmCopyType.L2G
        elif op_type_str == 'G2H':
            op_type = bm.BmCopyType.G2H
        elif op_type_str == 'G2L':
            op_type = bm.BmCopyType.G2L
        elif op_type_str == 'G2G':
            op_type = bm.BmCopyType.G2G
        else:
            raise f"Invalid op type({op_type_str})"
        ret = self._bm_handle.copy_data_batch(src_addrs, dst_addrs, sizes, count, op_type, flag)
        self.cli_return(ret)

    @result_handler
    def get_peer_rank_gva(self, rank_id: int, pool_type: str):
        mem_type = bm.BmMemType.HOST
        if pool_type == 'npu':
            mem_type = bm.BmMemType.DEVICE
        self.cli_return(self._bm_handle.peer_rank_ptr(rank_id, mem_type))

    @result_handler
    def alloc_local_memory(self, size: int, memory_type: str):
        tensor = self.malloc_tensor(1, size, memory_type)
        self._tensor_dict[tensor.data_ptr()] = tensor
        self.cli_return(tensor.data_ptr())

    @result_handler
    def free_local_memory(self, ptr: int):
        self._tensor_dict.pop(ptr)
        self.cli_return(0)

    @result_handler
    def register_memory(self, ptr: int, size: int):
        ret = self._bm_handle.register(ptr, size)
        self.cli_return(ret)

    @result_handler
    def calc_gva_sum(self, ptr: int, size: int):
        # 写回到本地计算
        tensor = self.malloc_tensor(1, size, 'npu')
        ret = self._bm_handle.copy_data_batch([ptr], [tensor.data_ptr()], [size], 1, bm.BmCopyType.G2L, 0)
        if ret != 0:
            self.cli_return(-1)
        self.cli_return(tensor.sum().item())

    @result_handler
    def calc_local_memory_sum(self, ptr: int):
        tensor = self._tensor_dict[ptr]
        if tensor is None:
            self.cli_return(-1)
        self.cli_return(tensor.sum().item())

    def _init_cmds(self):
        cmds = [
            CliCommand("init_smem_bm", "initialize smem bm", self.init_smem_bm, 0),
            CliCommand("close_smem_bm", "destruct smem bm", self.close_smem_bm, 0),
            CliCommand("bm_copy_batch", "bm_copy_batch", self.copy_data_batch, 6),
            CliCommand("get_peer_rank_gva", "get gva by rank id and pool type", self.get_peer_rank_gva, 2),
            CliCommand("alloc_local_memory", "get local memory by size and memory type", self.alloc_local_memory, 2),
            CliCommand("free_local_memory", "free local memory by ptr", self.free_local_memory, 1),
            CliCommand("register_memory", "register local memory", self.register_memory, 2),
            CliCommand("calc_gva_sum", "calc gva data sum", self.calc_gva_sum, 2),
            CliCommand("calc_local_memory_sum", "calc local memory data sum", self.calc_local_memory_sum, 1),
            CliCommand("copy_data", "copy data", self.copy_data, 5),
        ]
        self.register_command(cmds)


if __name__ == "__main__":
    if len(sys.argv) == 5:
        _, ip_str, port_str, device_id_str, rank_id_str = sys.argv
        server = MmcTest(ip_str, port_str, int(device_id_str), int(rank_id_str))
    else:
        print("Please input ip and port when starting the process.")
        sys.exit(1)
    print(f"Start app_id: {ip_str}:{port_str}")
    server.start()
