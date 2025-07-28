#!/usr/bin/env python3.10
#  Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.

import ast
import concurrent.futures
import dataclasses
import inspect
import json
import socket
import sys
import acl
import torch
import torch_npu
import threading
import traceback
from functools import wraps
from typing import Callable, Dict, List

from pymmc import DistributedObjectStore


@dataclasses.dataclass
class CliCommand:
    cmd_name: str
    cmd_description: str
    func: Callable
    args_num: int

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

    def _register_inner_command(self):
        self._commands = {
            "help": CliCommand("help", "show command list information", self._help, 0),
            "getServerCommands": CliCommand("getServerCommands", "getServerCommands, get the registered Commands",
                                            self._get_server_commands, 0),
        }

    def register_command(self, cmds: List[CliCommand]):
        for cmd in cmds:
            self._commands[cmd.cmd_name] = cmd

    @staticmethod
    def _convert_argument(arg_str: str, param_type):
        try:
            if param_type == int:
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

    @staticmethod
    # 解析参数并根据目标函数的参数类型进行转换
    def _parse_arguments(func, arg_strs):
        signature = inspect.signature(func)
        parsed_args = []
        for param, arg in zip(signature.parameters.values(), arg_strs):
            parsed_args.append(TestServer._convert_argument(arg, param.annotation))
        return parsed_args

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
        if len(args) != command.args_num:
            self.cli_print(f"Invalid input args num: {len(args)}.")
            self._help()
            self._cli_end_line()
            return
        parsed_params = self._parse_arguments(command.func, args)
        command.func(*parsed_params)
        self._cli_end_line()

    def start(self):
        self._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server_socket.bind(self._ip_port)
        self._server_socket.listen(5)
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            while True:
                client_socket, _ = self._server_socket.accept()
                executor.submit(self._handle_client, client_socket)

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

    def cli_print(self, msg: str):
        self._thread_local.client_socket.send(f"{msg}\n".encode('utf-8'))

    def cli_return(self, obj):
        obj_type = type(obj)
        if obj_type is int:
            data = str(obj).encode('utf-8')
        elif obj_type is bytes:
            data = obj
        else:
            data = str(obj).encode('utf-8')
        self._thread_local.client_socket.send(data)

    def _cli_end_line(self):
        print("send command result")
        self._thread_local.client_socket.send("\0".encode('utf-8'))

    def _help(self):
        """显示帮助信息。"""
        col_widths = max(len(item) for item in self._commands.keys()) + 1
        self.cli_print("Available commands:")
        for cmd in self._commands.values():
            self.cli_print(f":  {cmd.cmd_name: >{col_widths}}: {cmd.cmd_description}")

    def _get_server_commands(self):
        msg = ",".join(self._commands.keys())
        self.cli_print(f"{msg}")


def result_handler(func):
    @wraps(func)
    def wrapper(self, *args, **kwargs):
        try:
            func(self, *args, **kwargs)
        except Exception as e:
            self.cli_print(f"{func.__name__} raised exception: {e}")

    return wrapper

from enum import Enum

class MmcDirect(Enum):
    COPY_L2G = 0
    COPY_G2L = 1
    COPY_G2H = 2
    COPY_H2G = 3


class MmcTest(TestServer):
    def __init__(self, ip, port, device_id: int, block_num: int, layer_num: int, min_block_size: int):
        super().__init__(ip, port)
        self._init_cmds()
        self.__distributed_store_object = None
        self._device_id = device_id
        self._layer_num = layer_num
        self._block_num = block_num
        self._min_block_size = min_block_size
        self._npu_addr = 0
        self._npu_blocks: dict[int, list[torch.Tensor]] = {}
        self._cpu_addr = 0
        self._cpu_blocks: dict[int, list[torch.Tensor]] = {}

    def _init_cmds(self):
        cmds = [
            CliCommand("init_mmc", "initialize memcache", self.init_mmc, 0),
            CliCommand("close_mmc", "destruct memcache", self.close_mmc, 0),
            CliCommand("put", "put data in bytes format: [key] [data]", self.put, 2),
            CliCommand("put_from", "put data in bytes format: [key] [index] [media(0:cpu 1:npu)]", self.put_from, 3),
            CliCommand("get", "get data in bytes format: [key]", self.get, 1),
            CliCommand("get_into", "put data in bytes format: [key] [index] [media(0:cpu 1:npu)]", self.get_into, 3),
            CliCommand("batch_get_into", "batch put data: [keys] [indexes] [media(0:cpu 1:npu)]", self.batch_get_into, 3),
            CliCommand("batch_put_from", "batch get data: [keys] [indexes] [media(0:cpu 1:npu)]", self.batch_put_from, 3),
            CliCommand("is_exist", "check if a key exist: [key]", self.is_exist, 1),
            CliCommand("batch_is_exist", "check if a batch of keys exist: [keys]", self.batch_is_exist, 1),
            CliCommand("remove", "remove data: [key]", self.remove, 1),
            CliCommand("remove_batch", "remove a batch of data: [keys]", self.remove_batch, 1),
            CliCommand("tensor_sum", "tensor sum data: [index] [media]", self.tensor_sum, 2),
        ]
        self.register_command(cmds)

    @result_handler
    def print(self):
        self.cli_print("test print info")

    def acl_set_device(self):
        ret = acl.rt.set_device(self._device_id)
        if ret != 0:
            raise RuntimeError("acl set device failed")

    def malloc_blocks(self) -> int:
        if self._npu_addr != 0:
            return self._npu_addr
        self._npu_blocks.clear()
        acl.init()
        self.acl_set_device()
        raw_blocks = torch.rand(
            size=(self._layer_num, self._block_num, self._min_block_size),
            dtype=torch.float16,
            device=torch.device('npu')
        )

        for bi in range(self._block_num):
            self._npu_blocks[bi] = []
            for li in range(self._layer_num):
                self._npu_blocks[bi].append(raw_blocks[li][bi])

        cpu_raw_blocks = raw_blocks.to('cpu')
        for bi in range(self._block_num):
            self._cpu_blocks[bi] = []
            for li in range(self._layer_num):
                self._cpu_blocks[bi].append(cpu_raw_blocks[li][bi])

        self._npu_addr = raw_blocks.data_ptr()
        self._cpu_addr = cpu_raw_blocks.data_ptr()
        return raw_blocks.data_ptr()

    @result_handler
    def init_mmc(self):
        self.malloc_blocks()
        self.__distributed_store_object = DistributedObjectStore()
        res = self.__distributed_store_object.init()
        self.cli_return(res)

    @result_handler
    def close_mmc(self):
        res = self.__distributed_store_object.close()
        self.cli_return(res)

    @result_handler
    def put(self, key: str, data: bytes):
        res = self.__distributed_store_object.put(key, data)
        self.cli_return(res)

    @result_handler
    def put_from(self, key: str, index: int, media: int):
        self.cli_print(f"======== put_from({key}, {index}, {media})")
        if media == 0:
            direct = int(MmcDirect.COPY_H2G.value)
            tensor = self._cpu_blocks[index][0]
        else:
            direct = int(MmcDirect.COPY_L2G.value)
            tensor = self._npu_blocks[index][0]
        self.cli_print(f"======== put_from({key}, {tensor.data_ptr()}, {self._min_block_size * 2}, {direct})")
        res = self.__distributed_store_object.put_from(key, tensor.data_ptr(), self._min_block_size * 2, direct)
        self.cli_return(res)

    @result_handler
    def get(self, key: str):
        res = self.__distributed_store_object.get(key)
        self.cli_return(res)

    @result_handler
    def get_into(self, key: str, index: int, media: int):
        self.cli_print(f"======== get_into({key}, {index}, {media})")
        if media == 0:
            direct = int(MmcDirect.COPY_G2H.value)
            tensor = self._cpu_blocks[index][0]
        else:
            direct = int(MmcDirect.COPY_G2L.value)
            tensor = self._npu_blocks[index][0]
        self.cli_print(f"======== get_into({key}, {tensor.data_ptr()}, {self._min_block_size * 2}, {direct})")
        res = self.__distributed_store_object.get_into(key, tensor[0].data_ptr(), self._min_block_size * 2, direct)
        self.cli_return(res)

    @result_handler
    def batch_get_into(self, keys: list, indexes: list, media: int):
        self.cli_print(f"======== batch_get_into({keys}, {indexes}, {media})")
        data_ptrs = []
        sizes = []
        if media == 0:
            direct = int(MmcDirect.COPY_G2H.value)
            for i in range(len(keys)):
                data_ptrs.append(self._cpu_blocks[indexes[i]][0].data_ptr())
                sizes.append(self._min_block_size * 2)
        else:
            direct = int(MmcDirect.COPY_G2L.value)
            for i in range(len(keys)):
                data_ptrs.append(self._npu_blocks[indexes[i]][0].data_ptr())
                sizes.append(self._min_block_size * 2)
        self.cli_print(f"======== batch_get_into({keys}, {data_ptrs}, {sizes}, {direct})")
        res = self.__distributed_store_object.batch_get_into(keys, data_ptrs, sizes, direct)
        self.cli_return(str(res))

    @result_handler
    def batch_put_from(self, keys: list, indexes: list, media: int):
        self.cli_print(f"======== batch_put_from({keys}, {indexes}, {media})")
        data_ptrs = []
        sizes = []
        if media == 0:
            direct = int(MmcDirect.COPY_H2G.value)
            for i in range(len(keys)):
                data_ptrs.append(self._cpu_blocks[indexes[i]][0].data_ptr())
                sizes.append(self._min_block_size * 2)
        else:
            direct = int(MmcDirect.COPY_L2G.value)
            for i in range(len(keys)):
                data_ptrs.append(self._npu_blocks[indexes[i]][0].data_ptr())
                sizes.append(self._min_block_size * 2)
        self.cli_print(f"======== batch_put_from({keys}, {data_ptrs}, {sizes}, {direct})")
        res = self.__distributed_store_object.batch_put_from(keys, data_ptrs, sizes, direct)
        self.cli_return(str(res))

    def tensor_sum(self, index: int, media: int):
        self.acl_set_device()
        if media == 0:
            tensor = self._cpu_blocks[index][0]
        else:
            tensor = self._npu_blocks[index][0]
        ret = torch.sum(tensor, dtype=torch.float32)
        self.cli_return(ret.item())

    @result_handler
    def is_exist(self, key: str):
        res = self.__distributed_store_object.is_exist(key)
        self.cli_return(res)

    @result_handler
    def batch_is_exist(self, keys: list[str]):
        res = self.__distributed_store_object.batch_is_exist(keys)
        self.cli_return(res)

    @result_handler
    def remove(self, key: str):
        res = self.__distributed_store_object.remove(key)
        self.cli_return(res)

    @result_handler
    def remove_batch(self, keys: list[str]):
        res = self.__distributed_store_object.remove_batch(keys)
        self.cli_return(res)


if __name__ == "__main__":
    if len(sys.argv) == 3:
        _, ip, port = sys.argv
        device_id = 0
        block_num = 100
        layer_num = 20
        min_block_size = 512
    elif len(sys.argv) == 7:
        _, ip, port, device_id, block_num, layer_num, min_block_size = sys.argv
    else:
        print("Please input ip and port when starting the process.")
        sys.exit(1)
    print(f"Start app_id: {ip}:{port} {device_id} {block_num} {layer_num} {min_block_size}")
    server = MmcTest(ip, port, device_id, block_num, layer_num, min_block_size)
    server.start()
