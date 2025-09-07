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
from typing import Callable, Dict, List
from enum import Enum

import torch

from memcache import DistributedObjectStore


class MmcDirect(Enum):
    COPY_L2G = 0
    COPY_G2L = 1
    COPY_G2H = 2
    COPY_H2G = 3


@dataclasses.dataclass
class CliCommand:
    cmd_name: str
    cmd_desc: str
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
            self.cli_print(f":  {cmd.cmd_name: >{col_widths}}: {cmd.cmd_desc}")

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

def tensor_sum(tensor, sizes: List[int] = None):
    if tensor is None:
        return 0
    if sizes is None:
        return tensor.sum().item()

    return sum(layer[:size].sum().item() for layer, size in zip(tensor, sizes))


class MmcTest(TestServer):
    def __init__(self, ip, port, device_id=0):
        super().__init__(ip, port)
        self._device_id = device_id
        self._init_cmds()
        self._store = None

    def _init_cmds(self):
        cmds = [
            CliCommand("init_mmc", "initialize memcache", self.init_mmc, 0),
            CliCommand("close_mmc", "destruct memcache", self.close_mmc, 0),
            CliCommand("put", "put data in bytes format: [key] [data]", self.put, 2),
            CliCommand("get", "get data in bytes format: [key]", self.get, 1),
            CliCommand("put_from", "put data from a buffer: [key] [size] [media(0:cpu 1:npu)]", self.put_from, 3),
            CliCommand("get_into", "get data into a buffer: [key] [size] [media(0:cpu 1:npu)]", self.get_into, 3),
            CliCommand("batch_get_into", "batch put data: [keys] [sizes] [media(0:cpu 1:npu)]", self.batch_get_into, 3),
            CliCommand("batch_put_from", "batch get data: [keys] [sizes] [media(0:cpu 1:npu)]", self.batch_put_from, 3),
            CliCommand("is_exist", "check if a key exist: [key]", self.is_exist, 1),
            CliCommand("batch_is_exist", "check if a batch of keys exist: [keys]", self.batch_is_exist, 1),
            CliCommand("remove", "remove data: [key]", self.remove, 1),
            CliCommand("remove_batch", "remove a batch of data: [keys]", self.remove_batch, 1),
            CliCommand("get_key_info", "get data info of: [key]", self.get_key_info, 1),
            CliCommand("batch_get_key_info", "batch get data info of: [keys]", self.batch_get_key_info, 1),
            CliCommand("put_from_layers", "put data from multiple buffers [key] [sizes] [media(0:cpu 1:npu)]",
                       self.put_from_layers, 3),
            CliCommand("get_into_layers", "get data into multiple buffers [key] [sizes] [media(0:cpu 1:npu)]",
                       self.get_into_layers, 3),
            CliCommand("batch_put_from_layers", func=self.batch_put_from_layers, args_num=3,
                       cmd_desc="batch put data from multiple buffers [keys] [sizes] [media(0:cpu 1:npu)]"),
            CliCommand("batch_get_into_layers", func=self.batch_get_into_layers, args_num=3,
                       cmd_desc="batch get data into multiple buffers [keys] [sizes] [media(0:cpu 1:npu)]")
        ]
        self.register_command(cmds)

    @result_handler
    def print(self):
        self.cli_print("test print info")

    @result_handler
    def init_mmc(self):
        self._store = DistributedObjectStore()
        res = self._store.init(self._device_id)
        self.cli_return(res)

    @result_handler
    def close_mmc(self):
        res = self._store.close()
        self.cli_return(res)

    @result_handler
    def put(self, key: str, data: bytes):
        res = self._store.put(key, data)
        self.cli_return(res)

    @result_handler
    def put_from(self, key: str, size: int, media: int):
        if media == 0:
            direct = int(MmcDirect.COPY_H2G.value)
            tensor = self.malloc_tensor(mini_block_size=size, device='cpu')
        else:
            direct = int(MmcDirect.COPY_L2G.value)
            tensor = self.malloc_tensor(mini_block_size=size, device='npu')
        if size <= 0:
            res = self._store.put_from(key, 0, 0, direct)
            value = 0
        else:
            res = self._store.put_from(key, tensor.data_ptr(), size, direct)
            value = tensor_sum(tensor)
        self.cli_return(str([res, value]))

    @result_handler
    def get(self, key: str):
        res = self._store.get(key)
        self.cli_return(res)

    @result_handler
    def get_into(self, key: str, size: int, media: int):
        if media == 0:
            direct = int(MmcDirect.COPY_G2H.value)
            tensor = self.malloc_tensor(mini_block_size=size, device='cpu')
        else:
            direct = int(MmcDirect.COPY_G2L.value)
            tensor = self.malloc_tensor(mini_block_size=size, device='npu')
        if size <= 0:
            res = self._store.get_into(key, 0, 0, direct)
            value = 0
        else:
            res = self._store.get_into(key, tensor[0].data_ptr(), size, direct)
            value = tensor_sum(tensor)
        self.cli_return(str([res, value]))

    @result_handler
    def batch_get_into(self, keys: list, sizes: list, media: int):
        data_ptrs = []
        blocks = []
        if media == 0:
            direct = int(MmcDirect.COPY_G2H.value)
            for i in range(len(sizes)):
                blocks.append(self.malloc_tensor(mini_block_size=sizes[i], device='cpu'))
        else:
            direct = int(MmcDirect.COPY_G2L.value)
            for i in range(len(sizes)):
                blocks.append(self.malloc_tensor(mini_block_size=sizes[i], device='npu'))
        for i in range(len(sizes)):
            if blocks[i] is None:
                data_ptrs.append(0)
            else:
                data_ptrs.append(blocks[i].data_ptr())
        res = self._store.batch_get_into(keys, data_ptrs, sizes, direct)
        values = []
        for i in range(len(sizes)):
            values.append(tensor_sum(blocks[i]))
        self.cli_return(str([res, values]))

    @result_handler
    def batch_put_from(self, keys: list, sizes: list, media: int):
        data_ptrs = []
        blocks = []
        if media == 0:
            direct = int(MmcDirect.COPY_H2G.value)
            for i in range(len(sizes)):
                blocks.append(self.malloc_tensor(mini_block_size=sizes[i], device='cpu'))
        else:
            direct = int(MmcDirect.COPY_L2G.value)
            for i in range(len(sizes)):
                blocks.append(self.malloc_tensor(mini_block_size=sizes[i], device='npu'))
        for i in range(len(sizes)):
            if blocks[i] is None:
                data_ptrs.append(0)
            else:
                data_ptrs.append(blocks[i].data_ptr())
        res = self._store.batch_put_from(keys, data_ptrs, sizes, direct)
        values = []
        for i in range(len(sizes)):
            values.append(tensor_sum(blocks[i]))
        self.cli_return(str([res, values]))

    @result_handler
    def is_exist(self, key: str):
        res = self._store.is_exist(key)
        self.cli_return(res)

    @result_handler
    def batch_is_exist(self, keys: list[str]):
        res = self._store.batch_is_exist(keys)
        self.cli_return(res)

    @result_handler
    def remove(self, key: str):
        res = self._store.remove(key)
        self.cli_return(res)

    @result_handler
    def remove_batch(self, keys: list[str]):
        res = self._store.remove_batch(keys)
        self.cli_return(res)

    @result_handler
    def get_key_info(self, key: str):
        res = self._store.get_key_info(key)
        self.cli_return(res)

    @result_handler
    def batch_get_key_info(self, keys: list[str]):
        res = self._store.batch_get_key_info(keys)
        self.cli_return(res)

    @result_handler
    def put_from_layers(self, key: str, sizes: List[int], media: int):
        layers_num = len(sizes)
        mini_block_size = max(sizes, default=0)
        if media == 0:
            direct = MmcDirect.COPY_H2G.value
            device = 'cpu'
        else:
            direct = MmcDirect.COPY_L2G.value
            device = 'npu'
        tensor = self.malloc_tensor(layer_num=layers_num, mini_block_size=mini_block_size, device=device)
        res = self._store.put_from_layers(key,
                                          [] if tensor is None else [layer.data_ptr() for layer in tensor],
                                          sizes,
                                          direct)
        value = tensor_sum(tensor, sizes)
        self.cli_return(str([res, value]))

    @result_handler
    def get_into_layers(self, key: str, sizes: List[int], media: int):
        layers_num = len(sizes)
        mini_block_size = max(sizes, default=0)
        if media == 0:
            direct = MmcDirect.COPY_G2H.value
            device = 'cpu'
        else:
            direct = MmcDirect.COPY_G2L.value
            device = 'npu'
        tensor = self.malloc_tensor(layer_num=layers_num, mini_block_size=mini_block_size, device=device)
        res = self._store.get_into_layers(key,
                                          [] if tensor is None else [layer.data_ptr() for layer in tensor],
                                          sizes,
                                          direct)
        value = tensor_sum(tensor, sizes)
        self.cli_return(str([res, value]))

    @result_handler
    def batch_put_from_layers(self, keys: List[str], sizes: List[List[int]], media: int):
        if media == 0:
            direct = MmcDirect.COPY_H2G.value
            device = 'cpu'
        else:
            direct = MmcDirect.COPY_L2G.value
            device = 'npu'
        blocks = []
        for sizes_ in sizes:
            tensor = self.malloc_tensor(layer_num=len(sizes_), mini_block_size=max(sizes_, default=0), device=device)
            self._store.register_buffer(tensor.data_ptr(), max(sizes_, default=0) * len(sizes_))
            blocks.append(tensor)
        results = self._store.batch_put_from_layers(
            keys,
            [[]
             if block is None
             else [layer.data_ptr() for layer in block]
             for block in blocks],
            sizes,
            direct
        )
        if device == 'npu':
            self.sync_stream()
        tensor_sums = [tensor_sum(block, sizes_) for block, sizes_ in zip(blocks, sizes)]
        self.cli_return(str([results, tensor_sums]))

    @result_handler
    def batch_get_into_layers(self, keys: List[str], sizes: List[List[int]], media: int):
        if media == 0:
            direct = MmcDirect.COPY_G2H.value
            device = 'cpu'
        else:
            direct = MmcDirect.COPY_G2L.value
            device = 'npu'
        blocks = []
        for sizes_ in sizes:
            tensor = self.malloc_tensor(layer_num=len(sizes_), mini_block_size=max(sizes_, default=0), device=device)
            self._store.register_buffer(tensor.data_ptr(), max(sizes_, default=0) * len(sizes_))
            blocks.append(tensor)
        results = self._store.batch_get_into_layers(
            keys,
            [[]
             if block is None
             else [layer.data_ptr() for layer in block]
             for block in blocks],
            sizes,
            direct
        )
        if device == 'npu':
            self.sync_stream()
        tensor_sums = [tensor_sum(block, sizes_) for block, sizes_ in zip(blocks, sizes)]
        self.cli_return(str([results, tensor_sums]))

    def set_device(self):
        import acl
        acl.init()
        ret = acl.rt.set_device(self._device_id)
        if ret != 0:
            raise RuntimeError("acl set device failed")

    def sync_stream(self):
        import torch_npu
        torch_npu.npu.current_stream().synchronize()


    def malloc_tensor(self, layer_num: int = 1, mini_block_size: int = 1024, device='cpu'):
        if device not in ('cpu', 'npu'):
            raise RuntimeError(f"invalid device: {device}")
        if mini_block_size <= 0:
            return None

        if device == 'npu':
            import torch_npu
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


if __name__ == "__main__":
    if len(sys.argv) == 3:
        _, ip_str, port_str = sys.argv
        server = MmcTest(ip_str, port_str)
    elif len(sys.argv) == 4:
        _, ip_str, port_str, device_id_str = sys.argv
        server = MmcTest(ip_str, port_str, int(device_id_str))
    else:
        print("Please input ip and port when starting the process.")
        sys.exit(1)
    print(f"Start app_id: {ip_str}:{port_str}")
    server.start()
