# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os
import ast
import concurrent.futures
import dataclasses
import inspect
import logging
import random
import socket
import string
import sys
import time
import threading
import traceback
from functools import wraps
from typing import Callable, Dict, List, Any
from ctypes import CDLL

import torch
import torch_npu
import acl

import memfabric_hybrid
from memfabric_hybrid import bm, shm, TransferEngine, create_config_store, set_log_level

globals_device_id = -1
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')


@dataclasses.dataclass
class CliCommand:
    def __init__(self, cmd_name: str, cmd_description: str, func: Callable):
        self.cmd_name = cmd_name
        self.cmd_description = cmd_description
        self.func = func
        self.args_num = self._get_user_param_count(func)

    @staticmethod
    def _get_user_param_count(func):
        sig = inspect.signature(func)
        params = sig.parameters
        return len(params)


@dataclasses.dataclass
class TransferEngineData:
    buffer: torch.Tensor
    buffer_bytes: int
    batch_buffers: List[torch.Tensor]
    batch_bytes: List[int]


class TestServer:
    def __init__(self, socket_id: int):
        self.socket_file = f"/home/CI_HOME/memfabric_hybrid/bin/mb_{socket_id}.socket"
        self.server_socket = None
        self._commands: Dict[str:CliCommand] = {}
        self.generated_keys = set()
        self.thread_local = threading.local()
        self.thread_local.client_socket = None
        self._register_inner_command()
        self._stream = 0

    def _register_inner_command(self):
        self._commands = {
            "help": CliCommand("help", "show command list information", self._help),
            "getServerCommands": CliCommand("getServerCommands", "ServerCommands, get the registered Commands",
                                            self._get_server_commands),
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
            # 处理更复杂的类型
            elif param_type == bool:
                return arg_str.lower() in ['true', '1', 'yes']
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
        param_iter = iter(signature.parameters.values())
        for _, arg in enumerate(arg_strs):
            param = next(param_iter)
            param_type = param.annotation
            parsed_args.append(TestServer._convert_argument(arg, param_type))
        return parsed_args

    def _execute(self, cmd):
        """执行命令。"""
        parts = cmd.split()
        if not parts:
            return
        cmd_str = parts[0]
        params = parts[1:]
        if cmd_str in self._commands:
            command = self._commands[cmd_str]
            if len(params) == command.args_num:  # 允许使用默认参数
                parsed_params = self._parse_arguments(command.func, params)  # 转换参数
                command.func(*parsed_params)
                self._cli_end_line()
                return
            self.cli_print(f"Invalid input args num:{len(params)}, need:{command.args_num}.")
        else:
            self.cli_print(f"Unknown command: {cmd_str}")
        self._help()
        self._cli_end_line()

    def start(self):
        # 创建一个 Unix 域套接字
        self.server_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        # 删除旧的套接字文件（如果有）
        try:
            os.unlink(self.socket_file)
        except OSError as e:
            pass
        # 绑定套接字到文件
        self.server_socket.bind(self.socket_file)
        # 监听传入连接
        self.server_socket.listen(5)
        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as executor:
            while True:
                client_socket, _ = self.server_socket.accept()
                executor.submit(self._handle_client, client_socket)

    def _handle_client(self, client_socket: socket):
        self.thread_local.client_socket = client_socket
        if self._stream == 0:
            self._stream = set_device(globals_device_id)
        current_thread = threading.current_thread()
        logging.info(f"current thread: {current_thread.ident}")
        buffer = b""
        try:
            while True:
                # 接收客户端发送的命令
                data = client_socket.recv(1024)
                if not data:
                    self.thread_local.client_socket = None
                    break  # 如果没有接收到数据，退出循环
                buffer += data
                if b"\0" in data:
                    # 处理命令
                    msg = buffer.decode('utf-8').replace("\0", "")
                    buffer = b""
                    logging.info(f"[mftest] received command: {msg}")
                    try:
                        self._execute(msg.strip())
                    except Exception:
                        traceback.print_exc()
        finally:
            # 关闭客户端连接
            client_socket.close()

    def cli_print(self, msg: str):
        self.thread_local.client_socket.send(f"{msg}\n".encode('utf-8'))

    def _cli_end_line(self):
        self.thread_local.client_socket.send("\0".encode('utf-8'))

    def _help(self):
        """显示帮助信息。"""
        col_widths = max(len(item) for item in self._commands.keys()) + 1
        self.cli_print("Available commands:")
        for cmd in self._commands.values():
            self.cli_print(f":  {cmd.cmd_name: >{col_widths}}: {cmd.cmd_description}")

    def _get_server_commands(self):
        msg = ",".join(self._commands.keys())
        self.cli_print(f"{msg}")

    def generate_unique_key(self) -> str:
        while True:
            key = ''.join(random.choices(string.ascii_letters, k=26))
            if key not in self.generated_keys:
                self.generated_keys.add(key)
                return key


def result_handler(func):
    @wraps(func)
    def wrapper(self, *args, **kwargs):
        try:
            func(self, *args, **kwargs)
            self.cli_print(f"{func.__name__} success")
        except Exception as e:
            self.cli_print(f"{func.__name__} raised exception: {e}")

    return wrapper


def set_device(device_id: int):
    torch.npu.set_device(device=device_id)
    _stream = acl.rt.create_stream()
    return _stream


class MfTest(TestServer):
    def __init__(self, socket_id: int):
        super().__init__(socket_id)
        self._init_cmds()
        self._config_dic = {}  # 存储config对象
        self._bm_handle_dic = {}  # 存储bm handle对象
        self._shm_handle_dic = {}  # 存储shm handle对象D
        self._cpu_src_tensor_dic = {}  # 存储tensor对象
        self._npu_tensor_dic = {}  # 存储tensor对象
        self._transfer_engine_dic = {}  # 存储transfer engine对象
        self._transfer_engine_data_dic = {}  # 存储TransferEngineData对象

    def _init_cmds(self):
        cmds = [
            CliCommand("show",
                       "Show local object directory, show",
                       self.show),
            CliCommand("clear_all",
                       "Clear local object directory, clear",
                       self.clear_all),
            CliCommand("clear_object",
                       "Clear specify id object directory, clear_object [id]",
                       self.clear_object),
            CliCommand("generate_bm_config",
                       "Generate a bm config, generate_bm_config "
                       "[init_timeout] [create_timeout] [operation_timeout] [start_store] [start_store_only] "
                       "[dynamic_world_size] [unified_address_space] [rank_id] [auto_ranking] [flags]",
                       self.generate_bm_config),
            CliCommand("generate_bm_config_default",
                       "Generate a bm config, generate_bm_config_default [rank_id] [auto_ranking]",
                       self.generate_bm_config_default),
            CliCommand("generate_tensor_addr",
                       "Generate cpu and npu tensor, generate_tensor_addr [copy_size]",
                       self.generate_tensor_addr),
            CliCommand("generate_shm_config",
                       "Generate a shm config, generate_shm_config "
                       "[init_timeout] [create_timeout] [operation_timeout] [start_store] [flags]",
                       self.generate_shm_config),
            CliCommand("generate_shm_config_default",
                       "Generate a shm config, generate_shm_config [start_store]",
                       self.generate_shm_config_default),
            CliCommand("mf_smem_init",
                       "Initialize the smem running environment, mf_smem_init",
                       self.mf_smem_init),
            CliCommand("smem_set_log_level",
                       "Set log print level, smem_set_log_level [level]",
                       self.smem_set_log_level),
            CliCommand("mf_smem_uninit",
                       "Uninitialize the smem running environment, mf_smem_uninit",
                       self.mf_smem_uninit),
            CliCommand("bm_init",
                       "Initialize smem big memory library, bm_init "
                       "[store_url] [world_size] [device_id] [config_id]",
                       self.bm_init),
            CliCommand("bm_uninitialize",
                       "uninitialize smem big memory library, bm_uninitialize [flags]",
                       self.bm_uninitialize),
            CliCommand("bm_rank_id",
                       "Get the rank id assigned during initialize, bm_rank_id ",
                       self.bm_rank_id),
            CliCommand("bm_create",
                       "Create a big memory object locally after initialized, bm_create "
                       "[mem_id] [local_dram_size] [local_hbm_size] [data_op_type] [flags]",
                       self.bm_create),
            CliCommand("bm_destroy",
                       "Destroy big memory by a big memory handle. bm_destroy [handle_id]",
                       self.bm_destroy),
            CliCommand("bm_join",
                       "Join to global Big Memory space actively, after this, "
                       "we operate on the global space, bm_join [handle_id] [flags]",
                       self.bm_join),
            CliCommand("bm_leave",
                       "Leave the global Big Memory space actively, after this, "
                       "we cannot operate on the global space any more, bm_leave [handle_id] [flags]",
                       self.bm_leave),
            CliCommand("bm_local_mem_size",
                       "Get size of local memory that contributed to global space, "
                       "bm_local_mem_size [handle_id] [mem_type]",
                       self.bm_local_mem_size),
            CliCommand("bm_peer_rank_ptr",
                       "Get peer gva by rank id, bm_peer_rank_ptr [handle_id] [peer_rank] [mem_type]",
                       self.bm_peer_rank_ptr),
            CliCommand("bm_get_rank_id_by_gva",
                       "Get rank id of gva that belongs to, bm_get_rank_id_by_gva [handle_id] [gva]",
                       self.bm_get_rank_id_by_gva),
            CliCommand("bm_register",
                       "Register user mem, bm_register [handle_id] [addr] [size]",
                       self.bm_register),
            CliCommand("bm_unregister",
                       "Unregister user mem, bm_unregister [handle_id] [addr]",
                       self.bm_unregister),
            CliCommand("bm_copy_data",
                       "Data operation on Big Memory object, copy_data "
                       "[handle_id] [src_ptr] [dst_ptr] [size] [copy_type] [flags]",
                       self.bm_copy_data),
            CliCommand("bm_copy_data_batch",
                       "Data operation on Big Memory object, copy_data_batch "
                       "[handle_id] [src_addrs_str] [dst_addrs_str] [sizes] [count] [copy_type] [flags]",
                       self.bm_copy_data_batch),
            CliCommand("bm_wait",
                       "Wait all issued async copy(s) finish, bm_wait [handle_id]",
                       self.bm_wait),
            CliCommand("delete_bm_handle", "delete a bm handle, delete_bm_handle [handle_id]",
                       self.delete_bm_handle),
            CliCommand("shm_init",
                       "Initialize smem shm library, shm_init "
                       "[store_url] [world_size] [rank_id] [device_id] [config_id]",
                       self.shm_init),
            CliCommand("shm_uninit",
                       "Uninitialize smem shm library, shm_uninit [flags]",
                       self.shm_uninit),
            CliCommand("shm_create",
                       "Create a shm handle, shm_create "
                       "[shm_id] [rank_size] [rank_id] [local_mem_size] [data_op_type] [flags]",
                       self.shm_create),
            CliCommand("delete_shm_handle", "delete a bm handle, delete_shm_handle [handle_id]",
                       self.delete_shm_handle),
            CliCommand("shm_destroy",
                       "Destroy shm by a shm handle.shm_destroy [handle_id] [flags]",
                       self.shm_destroy),
            CliCommand("shm_set_context",
                       "Set user extra context of shm object, shm_set_context [handle_id] [context]",
                       self.shm_set_context),
            CliCommand("shm_local_rank",
                       "Get local rank of a shm object, shm_local_rank [handle_id]",
                       self.shm_local_rank),
            CliCommand("rank_size",
                       "Get rank size of a shm object, rank_size [handle_id]",
                       self.rank_size),
            CliCommand("shm_barrier",
                       "Do barrier on a shm object, using control network, shm_barrier [handle_id]",
                       self.shm_barrier),
            CliCommand("shm_all_gather",
                       "Do all gather on a shm object, using control network, shm_all_gather [handle_id] [local_data]",
                       self.shm_all_gather),
            CliCommand("shm_gva",
                       "Get global virtual address created, "
                       "it can be passed to kernel to data operations, sh_gva [handle_id]",
                       self.shm_gva),
            CliCommand("generate_transfer_engine_npu_tensor",
                       "Generate the buffer which is to transfer data between engines, "
                       "generate_transfer_engine_npu_tensor [is_src]",
                       self.generate_transfer_engine_npu_tensor),
            CliCommand("generate_transfer_engine_npu_buffer_sum",
                       "Calculate the sum of buffer, generate_transfer_engine_npu_buffer_sum "
                       "[transfer_data_id] [is_batch]",
                       self.generate_transfer_engine_npu_buffer_sum),
            CliCommand("generate_transfer_engine_cpu_tensor",
                       "Generate the buffer which is to transfer data between engines, "
                       "generate_transfer_engine_cpu_tensor [is_src]",
                       self.generate_transfer_engine_cpu_tensor),
            CliCommand("generate_transfer_engine_cpu_buffer_sum",
                       "Calculate the sum of buffer, generate_transfer_engine_cpu_buffer_sum "
                       "[transfer_data_id] [is_batch]",
                       self.generate_transfer_engine_cpu_buffer_sum),
            CliCommand("transfer_engine_get_rpc_port",
                       "Generate a random port [handle_id]",
                       self.transfer_engine_get_rpc_port),
            CliCommand("transfer_engine_create_config_store",
                       "Create the config store server, transfer_engine_create_config_store [store_url]",
                       self.transfer_engine_create_config_store),
            CliCommand("transfer_engine_set_log_level",
                       "Set log level for transfer engine, transfer_engine_set_log_level [level]",
                       self.transfer_engine_set_log_level),
            CliCommand("transfer_engine_initialize",
                       "Initialize the transfer engine, transfer_engine_initialize "
                       "[store_url] [session_id] [role] [device_id] [op_type]",
                       self.transfer_engine_initialize),
            CliCommand("transfer_engine_register_memory",
                       "Register memory for transfer engine, "
                       "transfer_engine_register_memory [handle_id] [transfer_data_id]",
                       self.transfer_engine_register_memory),
            CliCommand("transfer_engine_transfer_sync_write",
                       "Transfer data between transfer engine, transfer_engine_transfer_sync_write "
                       "[handle_id] [transfer_data_id] [peer_buffer_addresses] [destSession]",
                       self.transfer_engine_transfer_sync_write),
            CliCommand("transfer_engine_transfer_sync_read",
                       "Transfer data between transfer engine, transfer_engine_transfer_sync_read "
                       "[handle_id] [transfer_data_id] [peer_buffer_addresses] [destSession]",
                       self.transfer_engine_transfer_sync_read),
            CliCommand("transfer_engine_batch_register_memory",
                       "Register batch memory for transfer engine, "
                       "transfer_engine_batch_register_memory [handle_id] [transfer_data_id]",
                       self.transfer_engine_batch_register_memory),
            CliCommand("transfer_engine_batch_transfer_sync_write",
                       "Transfer batch data between transfer engine, transfer_engine_batch_transfer_sync_write "
                       "[handle_id] [transfer_data_id] [peer_buffer_addresses] [destSession]",
                       self.transfer_engine_batch_transfer_sync_write),
            CliCommand("transfer_engine_batch_transfer_sync_read",
                       "Transfer batch data between transfer engine, transfer_engine_batch_transfer_sync_read "
                       "[handle_id] [transfer_data_id] [peer_buffer_addresses] [destSession]",
                       self.transfer_engine_batch_transfer_sync_read),
            CliCommand("transfer_engine_transfer_async_write_submit",
                       "Transfer data between transfer engine, transfer_engine_transfer_async_write_submit "
                       "[handle_id] [transfer_data_id] [peer_buffer_address] [destSession] [stream]",
                       self.transfer_engine_transfer_async_write_submit),
            CliCommand("transfer_engine_transfer_async_read_submit",
                       "Transfer data between transfer engine, transfer_engine_transfer_async_read_submit "
                       "[handle_id] [transfer_data_id] [peer_buffer_address] [destSession] [stream]",
                       self.transfer_engine_transfer_async_read_submit),
            CliCommand("transfer_engine_unregister_memory",
                       "Destroy transfer engine handle, "
                       "transfer_engine_unregister_memory [handle_id] [transfer_data_id]",
                       self.transfer_engine_unregister_memory),
            CliCommand("transfer_engine_destroy",
                       "Destroy transfer engine handle, transfer_engine_destroy [handle_id]",
                       self.transfer_engine_destroy),
            CliCommand("transfer_engine_uninitialize",
                       "Uninitialize transfer engine, transfer_engine_uninitialize [handle_id]",
                       self.transfer_engine_uninitialize),
            CliCommand("delete_trans_handle", "delete a delete_trans_handle handle, delete_bm_handle [handle_id]",
                       self.delete_trans_handle),
        ]
        self.register_command(cmds)

    def show(self):
        self.cli_print("config objects:")
        for key, value in self._config_dic.items():
            self.cli_print(f":\tid:{key}-{value}")
        self.cli_print("bm handle objects:")
        for key, value in self._bm_handle_dic.items():
            self.cli_print(f":\tid:{key}")
        self.cli_print("shm handle objects:")
        for key, value in self._shm_handle_dic.items():
            self.cli_print(f":\tid:{key}")
        self.cli_print("cpu tensor objects:")
        for key, value in self._cpu_src_tensor_dic.items():
            self.cli_print(f":\tdevice:{value.device}, id:{key},addr:{value.data_ptr()}")
        self.cli_print("npu tensor objects:")
        for key, value in self._npu_tensor_dic.items():
            self.cli_print(f":\tdevice:{value.device}, id:{key},addr:{value.data_ptr()}")
        self.cli_print("transfer engine objects:")
        for key, value in self._transfer_engine_dic.items():
            self.cli_print(f":\tid:{key}")

    @result_handler
    def clear_all(self):
        self._config_dic.clear()
        self._bm_handle_dic.clear()
        self._shm_handle_dic.clear()
        self._cpu_src_tensor_dic.clear()
        self._npu_tensor_dic.clear()
        self._transfer_engine_dic.clear()
        self._transfer_engine_data_dic.clear()

    @result_handler
    def clear_object(self, obj_id):
        if obj_id in self._config_dic:
            self._config_dic.pop(obj_id, None)
            return
        if obj_id in self._bm_handle_dic:
            self._bm_handle_dic.pop(obj_id, None)
            return
        if obj_id in self._shm_handle_dic:
            self._shm_handle_dic.pop(obj_id, None)
            return
        if obj_id in self._cpu_src_tensor_dic:
            self._cpu_src_tensor_dic.pop(obj_id, None)
            return
        if obj_id in self._npu_tensor_dic:
            self._npu_tensor_dic.pop(obj_id, None)
            return
        if obj_id in self._transfer_engine_dic:
            self._transfer_engine_dic.pop(obj_id, None)

    @result_handler
    def generate_tensor_addr(self, copy_size: int):
        count = copy_size // 4
        src_tensor = torch.empty([count], dtype=torch.int32)
        base = globals_device_id
        mod = 32767
        for i in range(0, count):
            base = (base * 23 + 17) % mod
            if ((i + globals_device_id) % 3) == 0:
                src_tensor[i] = -base
            else:
                src_tensor[i] = base
        src_tensor_id = id(src_tensor)
        self._cpu_src_tensor_dic[src_tensor_id] = src_tensor
        npu_tensor = src_tensor.npu(globals_device_id)
        npu_tensor_id = id(npu_tensor)
        self._npu_tensor_dic[npu_tensor_id] = npu_tensor
        self.cli_print(f"cpu src tensor addr:{src_tensor.data_ptr()}, npu tensor addr:{npu_tensor.data_ptr()}")

    def _get_config(self, config_id: int) -> Any:
        """
        :param config_id: 保存config的key值
        """
        config = self._config_dic.get(config_id, None)
        return config

    def _get_transfer_engine_data(self, data_id: int) -> Any:
        """
        :param data_id: 保存transfer_data的key值
        """
        transfer_data = self._transfer_engine_data_dic.get(data_id, None)
        return transfer_data

    @result_handler
    def generate_bm_config(self, init_timeout: int, create_timeout: int, operation_timeout: int, start_store: bool,
                           start_store_only: bool, dynamic_world_size: bool, unified_address_space: bool,
                           auto_ranking: bool, rank_id: int, flags: int, hcom_url: str):
        config = bm.BmConfig()
        config.init_timeout = init_timeout
        config.create_timeout = create_timeout
        config.operation_timeout = operation_timeout
        config.start_store = start_store
        config.start_store_only = start_store_only
        config.dynamic_world_size = dynamic_world_size
        config.unified_address_space = unified_address_space
        config.auto_ranking = auto_ranking
        config.rank_id = rank_id
        config.flags = flags
        config.set_nic(hcom_url)
        config_id = id(config)
        self._config_dic[config_id] = config
        logging.info(f"generate bm config id:{config_id}")
        self.cli_print(f"generate bm config id:{config_id}")

    @result_handler
    def generate_bm_config_default(self, rank_id: int, auto_ranking: bool):
        config = bm.BmConfig()
        config.rank_id = rank_id
        config.auto_ranking = auto_ranking
        config_id = id(config)
        self._config_dic[config_id] = config
        self.cli_print(f"generate bm config id:{config_id}")

    @result_handler
    def generate_shm_config(self, init_timeout: int, create_timeout: int, operation_timeout: int, start_store: bool,
                            flags: int):
        config = shm.ShmConfig()
        config.init_timeout = init_timeout
        config.create_timeout = create_timeout
        config.operation_timeout = operation_timeout
        config.start_store = start_store
        config.flags = flags
        config_id = id(config)
        self._config_dic[config_id] = config
        self.cli_print(f"generate shm config id:{config_id}")

    @result_handler
    def generate_shm_config_default(self, start_store: bool):
        config = shm.ShmConfig()
        config.start_store = start_store
        config_id = id(config)
        self._config_dic[config_id] = config
        self.cli_print(f"generate shm config id:{config_id}")

    @result_handler
    def mf_smem_init(self, flags: int = 0):
        ret = memfabric_hybrid.initialize(flags)
        self.cli_print(f"mf smem init ret({ret})")

    @result_handler
    def smem_set_log_level(self, level: int):
        ret = memfabric_hybrid.set_log_level(level)
        memfabric_hybrid.set_conf_store_tls(0, "0")
        self.cli_print(f"set log level{level} ret({ret})")

    @result_handler
    def mf_smem_uninit(self):
        memfabric_hybrid.uninitialize()

    @result_handler
    def bm_init(self, store_url: str, world_size: int, device_id: int, config_id: int):
        logging.info("============================bm_init============")
        config = self._get_config(config_id)
        logging.info(f"config={config} config_id={config_id}")
        ret = bm.initialize(store_url=store_url, world_size=world_size, device_id=device_id, config=config)
        # ret = 0
        self.cli_print(f"smem BM initialize ret({ret})")

    @result_handler
    def bm_uninitialize(self, flags: int):
        bm.uninitialize(flags)

    @result_handler
    def bm_rank_id(self):
        rank_id = bm.bm_rank_id()
        self.cli_print(f"bm rank id:{rank_id}")

    @result_handler
    def bm_create(self, mem_id: int, local_dram_size: int, local_hbm_size: int, data_op_type: int, flags: int):
        handle = bm.create(id=mem_id, local_dram_size=local_dram_size,
                           local_hbm_size=local_hbm_size, data_op_type=bm.BmDataOpType(data_op_type), flags=flags)
        self.cli_print(f"id={mem_id}, local_dram_size={local_dram_size}, local_hbm_size={local_hbm_size}, "
                       f"data_op_type={bm.BmDataOpType(data_op_type),} flags={flags}")
        if handle is not None:
            addr = id(handle)
            self._bm_handle_dic[addr] = handle
        else:
            addr = 0
        self.cli_print(f"create a big memory object:{addr}")

    @result_handler
    def bm_destroy(self, handle_id: int):
        handle = self._bm_handle_dic[handle_id]
        handle.destroy()

    @result_handler
    def bm_join(self, handle_id: int, flags: int):
        handle = self._bm_handle_dic[handle_id]
        ret = handle.join(flags=flags)
        self.cli_print(f"bm join, ret:{ret}")

    @result_handler
    def bm_leave(self, handle_id: int, flags: int):
        handle = self._bm_handle_dic[handle_id]
        ret = handle.leave(flags)
        self.cli_print(f"bm leave, ret:{ret}")

    @result_handler
    def bm_local_mem_size(self, handle_id: int, mem_type: int):
        handle = self._bm_handle_dic[handle_id]
        mem_size = handle.local_mem_size(mem_type=bm.BmMemType(mem_type))
        self.cli_print(f"local memory size:{mem_size}, type:{bm.BmMemType(mem_type)}")

    @result_handler
    def bm_peer_rank_ptr(self, handle_id: int, peer_rank: int, mem_type: int):
        handle = self._bm_handle_dic[handle_id]
        ptr = handle.peer_rank_ptr(peer_rank=peer_rank, mem_type=bm.BmMemType(mem_type))
        self.cli_print(f"peer rack ptr16:{hex(ptr)}, type:{bm.BmMemType(mem_type)}")
        self.cli_print(f"peer rack ptr:{ptr}, type:{bm.BmMemType(mem_type)}")

    @result_handler
    def bm_get_rank_id_by_gva(self, handle_id: int, gva: int):
        handle = self._bm_handle_dic[handle_id]
        rank_id = handle.get_rank_id_by_gva(gva)
        self.cli_print(f"bm rank id:{rank_id}")

    @result_handler
    def bm_register(self, handle_id: int, addr: int, size: int):
        handle = self._bm_handle_dic[handle_id]
        ret = handle.register(addr=addr, size=size)
        self.cli_print(f"bm register mem, ret:{ret}")

    @result_handler
    def bm_unregister(self, handle_id: int, addr: int):
        handle = self._bm_handle_dic[handle_id]
        ret = handle.unregister(addr=addr)
        self.cli_print(f"bm unregister mem, ret:{ret}")

    @result_handler
    def bm_copy_data(self, handle_id: int, src_ptr: int, dst_ptr: int, size: int, copy_type: int, flags: int):
        handle = self._bm_handle_dic[handle_id]
        self.cli_print(
            f"src_ptr={src_ptr}, dst_ptr={hex(dst_ptr)}, size={size}, type={bm.BmCopyType(copy_type)}, flags={flags}")
        handle.copy_data(src_ptr=src_ptr, dst_ptr=dst_ptr, size=size, type=bm.BmCopyType(copy_type), flags=flags)

    @result_handler
    def bm_copy_data_batch(self, handle_id: int, src_addrs_str: int, dst_addrs_str: int, sizes_str: int,
                           count: int, copy_type: int, flags: int):
        handle = self._bm_handle_dic[handle_id]
        src_addrs = list(map(int, src_addrs_str.split(",")))
        dst_addrs = list(map(int, dst_addrs_str.split(",")))
        sizes = list(map(int, sizes_str.split(",")))
        ret = handle.copy_data_batch(src_addrs=src_addrs, dst_addrs=dst_addrs, sizes=sizes, count=count,
                                     type=bm.BmCopyType(copy_type), flags=flags)
        self.cli_print(f"bm copy_data_abtch, ret:{ret}")

    @result_handler
    def bm_wait(self, handle_id: int):
        handle = self._bm_handle_dic[handle_id]
        ret = handle.wait()
        self.cli_print(f"bm wait, ret:{ret}")

    @result_handler
    def delete_bm_handle(self, handle_id: int):
        handle = self._bm_handle_dic[handle_id]
        del handle

    @result_handler
    def delete_shm_handle(self, handle_id: int):
        handle = self._shm_handle_dic[handle_id]
        del handle

    @result_handler
    def shm_init(self, store_url: str, world_size: int, rank_id: int, device_id: int, config_id: int):
        config = self._get_config(config_id)
        ret = shm.initialize(store_url=store_url, world_size=world_size, rank_id=rank_id, device_id=device_id,
                             config=config)
        self.cli_print(f"smem SHM initialize ret({ret})")

    @result_handler
    def shm_uninit(self, flags: int):
        shm.uninitialize(flags)

    @result_handler
    def shm_create(self, shm_id: int, rank_size: int, rank_id: int, local_mem_size: int, data_op_type: int, flags: int):
        shm_handle = shm.create(id=shm_id, rank_size=rank_size, rank_id=rank_id, local_mem_size=local_mem_size,
                                data_op_type=shm.ShmDataOpType(data_op_type), flags=flags)
        if shm_handle is not None:
            addr = id(shm_handle)
            self._shm_handle_dic[addr] = shm_handle
        else:
            addr = 0
        self.cli_print(f"create shm object:{addr}")

    @result_handler
    def shm_destroy(self, handle_id: int, flags: int):
        handle = self._shm_handle_dic[handle_id]
        self.cli_print(f"before shm_destroy")
        handle.destroy(flags)

    @result_handler
    def shm_set_context(self, handle_id: int, context: str):
        handle = self._shm_handle_dic[handle_id]
        handle.set_context(bytes(context, encoding="utf-8"))

    @result_handler
    def shm_local_rank(self, handle_id: int):
        handle = self._shm_handle_dic[handle_id]
        rank_id = handle.local_rank()
        self.cli_print(f"shm object local rank:(\d+):{rank_id}")

    @result_handler
    def rank_size(self, handle_id: int):
        handle = self._shm_handle_dic[handle_id]
        size = handle.local_rank()
        self.cli_print(f"shm object size:{size}")

    @result_handler
    def shm_barrier(self, handle_id: int):
        handle = self._shm_handle_dic[handle_id]
        handle.barrier()

    @result_handler
    def shm_all_gather(self, handle_id: int, local_data: str):
        handle = self._shm_handle_dic[handle_id]
        handle.all_gather(local_data.encode())

    @result_handler
    def shm_gva(self, handle_id: int):
        handle = self._shm_handle_dic[handle_id]
        addr = handle.gva()
        self.cli_print(f"get shm object gva:{addr}")

    @result_handler
    def generate_transfer_engine_npu_tensor(self, is_src: bool):
        # 手动分配内存再切分
        total_size_bytes = 4 * 1024 * 1024  # 4MB
        device = 'npu'
        if is_src:
            big_buffer = torch.ones((total_size_bytes,), dtype=torch.uint8, device=device)
        else:
            big_buffer = torch.zeros((total_size_bytes,), dtype=torch.uint8, device=device)

        buffer = big_buffer[0:1 * 1024 * 1024].reshape(1024, 1024)
        buffer_bytes = buffer.element_size() * buffer.numel()

        batch_buffers = [
            big_buffer[2 * 1024 * 1024:3 * 1024 * 1024].reshape(1024, 1024),
            big_buffer[3 * 1024 * 1024:4 * 1024 * 1024].reshape(1024, 1024)
        ]
        batch_bytes = [b.element_size() * b.numel() for b in batch_buffers]

        transfer_data = TransferEngineData(
            buffer=buffer,
            buffer_bytes=buffer_bytes,
            batch_buffers=batch_buffers,
            batch_bytes=batch_bytes
        )
        transfer_data_id = id(transfer_data)
        self._transfer_engine_data_dic[transfer_data_id] = transfer_data
        buffer_addrs = [b.data_ptr() for b in transfer_data.batch_buffers]
        self.cli_print(f"generate transfer engine data id:{transfer_data_id}")
        self.cli_print(f"generate transfer engine data buffer addr is:{buffer.data_ptr()}")
        self.cli_print(f"generate transfer engine data buffer bytes is:{buffer_bytes}")
        self.cli_print(f"generate transfer engine data batch buffer addr is:{buffer_addrs[0]},{buffer_addrs[1]}")
        self.cli_print(f"generate transfer engine data batch buffer bytes is:{batch_bytes[0]},{batch_bytes[1]}")

    @result_handler
    def generate_transfer_engine_cpu_tensor(self, is_src: bool):
        # 手动分配内存再切分
        total_size_bytes = 4 * 1024 * 1024  # 4MB
        device = 'cpu'
        if is_src:
            big_buffer = torch.ones((total_size_bytes,), dtype=torch.uint8, device=device)
        else:
            big_buffer = torch.zeros((total_size_bytes,), dtype=torch.uint8, device=device)

        buffer = big_buffer[0:1 * 1024 * 1024].reshape(1024, 1024)
        buffer_bytes = buffer.element_size() * buffer.numel()

        batch_buffers = [
            big_buffer[2 * 1024 * 1024:3 * 1024 * 1024].reshape(1024, 1024),
            big_buffer[3 * 1024 * 1024:4 * 1024 * 1024].reshape(1024, 1024)
        ]
        batch_bytes = [b.element_size() * b.numel() for b in batch_buffers]

        transfer_data = TransferEngineData(
            buffer=buffer,
            buffer_bytes=buffer_bytes,
            batch_buffers=batch_buffers,
            batch_bytes=batch_bytes
        )
        transfer_data_id = id(transfer_data)
        self._transfer_engine_data_dic[transfer_data_id] = transfer_data
        buffer_addrs = [b.data_ptr() for b in transfer_data.batch_buffers]
        self.cli_print(f"generate transfer engine data id:{transfer_data_id}")
        self.cli_print(f"generate transfer engine data buffer addr is:{buffer.data_ptr()}")
        self.cli_print(f"generate transfer engine data buffer bytes is:{buffer_bytes}")
        self.cli_print(f"generate transfer engine data batch buffer addr is:{buffer_addrs[0]},{buffer_addrs[1]}")
        self.cli_print(f"generate transfer engine data batch buffer bytes is:{batch_bytes[0]},{batch_bytes[1]}")

    @result_handler
    def generate_transfer_engine_npu_buffer_sum(self, transfer_data_id: int, is_batch: bool):
        if transfer_data_id not in self._transfer_engine_data_dic:
            self.cli_print(f"Transfer data with ID {transfer_data_id} not found")
            return
        transfer_data = self._transfer_engine_data_dic[transfer_data_id]
        if is_batch:
            buffer_sum = torch.sum(torch.cat(transfer_data.batch_buffers))
        else:
            buffer_sum = torch.sum(transfer_data.buffer)
        self.cli_print(f"Calculate the buffer sum is:{buffer_sum}")

    @result_handler
    def generate_transfer_engine_cpu_buffer_sum(self, transfer_data_id: int, is_batch: bool):
        if transfer_data_id not in self._transfer_engine_data_dic:
            self.cli_print(f"Transfer data with ID {transfer_data_id} not found")
            return
        transfer_data = self._transfer_engine_data_dic[transfer_data_id]
        if is_batch:
            buffer_sum = torch.sum(torch.cat(transfer_data.batch_buffers))
        else:
            buffer_sum = torch.sum(transfer_data.buffer)
        self.cli_print(f"Calculate the buffer sum is:{buffer_sum}")

    @result_handler
    def transfer_engine_get_rpc_port(self, handle_id: int):
        engine = self._transfer_engine_dic[handle_id]
        port = engine.get_rpc_port()
        self.cli_print(f"generate rpc port is:{port}")

    @result_handler
    def transfer_engine_create_config_store(self, store_url: str):
        create_config_store(store_url)
        self.cli_print(f"*********store_url is : {store_url}")
        self.cli_print("create transfer engine successfully.")

    @result_handler
    def transfer_engine_set_log_level(self, level: int):
        set_log_level(level)
        memfabric_hybrid.set_conf_store_tls(0, "0")
        self.cli_print("set log level for transfer engine successfully.")

    @result_handler
    def transfer_engine_initialize(self, store_url: str, session_id: str, role: str, device_id: int, op_type: int):
        engine = TransferEngine()
        # 只掉接口暂时不用
        port = engine.get_rpc_port()
        if op_type == 1:
            data_op_type = TransferEngine.TransDataOpType.SDMA
        elif op_type == 2:
            data_op_type = TransferEngine.TransDataOpType.DEVICE_RDMA
        else:
            logging.error("Invalid optype: %d", op_type)
            return
        self.cli_print(f"TransDataOpType:{data_op_type}")
        ret_value = engine.initialize(
            store_url,
            session_id,
            role,
            device_id,
            data_op_type
        )
        if ret_value == 0:
            addr = id(engine)
            self._transfer_engine_dic[addr] = engine
        else:
            addr = 0
        self.cli_print(f"transfer engine initialize finish:{addr}")

    @result_handler
    def transfer_engine_register_memory(self, handle_id: int, buffers: int, capacities: int):
        engine = self._transfer_engine_dic[handle_id]
        ret_value = engine.register_memory(buffers, capacities)
        self.cli_print(f"register memory for transfer engine result is:{ret_value}")

    @result_handler
    def transfer_engine_transfer_sync_write(self, handle_id: int, dest_session: str, buffers: int,
                                            peer_buffer_addresses: int, length: int):
        engine = self._transfer_engine_dic[handle_id]
        ret_value = engine.transfer_sync_write(dest_session, buffers, peer_buffer_addresses, length)
        time.sleep(5)
        self.cli_print(f"write data for transfer engine result is:{ret_value}")

    @result_handler
    def transfer_engine_transfer_sync_read(self, handle_id: int, dest_session: str, buffers: int,
                                            peer_buffer_addresses: int, length: int):
        engine = self._transfer_engine_dic[handle_id]
        ret_value = engine.transfer_sync_read(dest_session, buffers, peer_buffer_addresses, length)
        time.sleep(5)
        self.cli_print(f"read data for transfer engine result is:{ret_value}")

    @result_handler
    def transfer_engine_batch_register_memory(self, handle_id: int, buffers_str: str, capacities_str: str):
        engine = self._transfer_engine_dic[handle_id]
        buffers = list(map(int, buffers_str.split(",")))
        capacities = list(map(int, capacities_str.split(",")))
        ret_value = engine.batch_register_memory(buffers, capacities)
        self.cli_print(f"register batch memory for transfer engine result is:{ret_value}")

    @result_handler
    def transfer_engine_batch_transfer_sync_write(self, handle_id: int, dest_session: str, buffers_str: str,
                                                  peer_buffer_addresses_str: str, lengths_str: str):
        buffers = list(map(int, buffers_str.split(",")))
        peer_buffer_addresses = list(map(int, peer_buffer_addresses_str.split(",")))
        lengths = list(map(int, lengths_str.split(",")))
        engine = self._transfer_engine_dic[handle_id]
        ret_value = engine.batch_transfer_sync_write(dest_session, buffers, peer_buffer_addresses, lengths)
        time.sleep(5)
        self.cli_print(f"write batch data for transfer engine result is:{ret_value}")

    @result_handler
    def transfer_engine_batch_transfer_sync_read(self, handle_id: int, dest_session: str, buffers_str: str,
                                                  peer_buffer_addresses_str: str, lengths_str: str):
        buffers = list(map(int, buffers_str.split(",")))
        peer_buffer_addresses = list(map(int, peer_buffer_addresses_str.split(",")))
        lengths = list(map(int, lengths_str.split(",")))
        engine = self._transfer_engine_dic[handle_id]
        ret_value = engine.batch_transfer_sync_read(dest_session, buffers, peer_buffer_addresses, lengths)
        time.sleep(5)
        self.cli_print(f"read batch data for transfer engine result is:{ret_value}")

    @result_handler
    def transfer_engine_transfer_async_write_submit(self, handle_id: int, dest_session: str, buffers: int,
                                            peer_buffer_addresses: int, length: int, stream: int):
        engine = self._transfer_engine_dic[handle_id]
        if stream != 0:
            stream = self._stream
        ret_value = engine.transfer_async_write_submit(dest_session, buffers, peer_buffer_addresses, length, stream)
        time.sleep(5)
        self.cli_print(f"async write data submit for transfer engine result is:{ret_value}")

    @result_handler
    def transfer_engine_transfer_async_read_submit(self, handle_id: int, dest_session: str, buffers: int,
                                            peer_buffer_addresses: int, length: int, stream: int):
        engine = self._transfer_engine_dic[handle_id]
        if stream != 0:
            stream = self._stream
        ret_value = engine.transfer_async_read_submit(dest_session, buffers, peer_buffer_addresses, length, stream)
        time.sleep(5)
        self.cli_print(f"async read data submit for transfer engine result is:{ret_value}")

    @result_handler
    def transfer_engine_unregister_memory(self, handle_id: int, buffers: int):
        engine = self._transfer_engine_dic[handle_id]
        ret_value = engine.unregister_memory(buffers)
        self.cli_print(f"unregister memory for transfer engine result is:{ret_value}")

    @result_handler
    def transfer_engine_destroy(self, handle_id: int):
        self.cli_print("start transfer_engine_destroy")
        engine = self._transfer_engine_dic[handle_id]
        engine.destroy()
        self.cli_print("finish transfer_engine_destroy")

    @result_handler
    def transfer_engine_uninitialize(self, handle_id: int):
        self.cli_print("start transfer_engine_uninitialize")
        engine = self._transfer_engine_dic[handle_id]
        engine.unInitialize()
        self.cli_print("finish transfer_engine_uninitialize")

    @result_handler
    def delete_trans_handle(self, handle_id: int):
        handle = self._transfer_engine_dic[handle_id]
        del handle


if __name__ == "__main__":
    if len(sys.argv) <= 1:
        logging.error("Please input app_id and device_id when starting the process"
                      "usage: memfabric_test.py [app_id] [device_id].")
        sys.exit(1)
    app_id = int(sys.argv[1])
    globals_device_id = int(sys.argv[2])
    logging.info(f"Start app_id: {app_id}, device:{globals_device_id}")
    server = MfTest(app_id)
    server.start()
