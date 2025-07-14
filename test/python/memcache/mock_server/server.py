#!/usr/bin/env python3.10
#  Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.

import ast
import concurrent.futures
import dataclasses
import inspect
import os
import socket
import sys
import threading
import traceback
from functools import wraps
from typing import Callable, Dict, List


@dataclasses.dataclass
class CliCommand:
    cmd_name: str
    cmd_description: str
    func: Callable
    args_num: int


class TestServer:
    def __init__(self, socket_id: int):
        self._socket_path = os.path.join(f"{os.path.dirname(os.path.abspath(__file__))}", f"mmc_{socket_id}.socket")
        self._server_socket = None
        self._commands: Dict[str:CliCommand] = {}
        self._thread_local = threading.local()
        self._thread_local.client_socket = None
        self._register_inner_command()

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
            if len(params) >= command.args_num:  # 允许使用默认参数
                parsed_params = self._parse_arguments(command.func, params)  # 转换参数
                command.func(*parsed_params)
                self._cli_end_line()
                return
            self.cli_print(f"Invalid input args num:{len(params)}.")
        else:
            self.cli_print(f"Unknown command: {cmd_str}")
        self._help()
        self._cli_end_line()

    def start(self):
        # 创建一个 Unix 域套接字
        self._server_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        # 删除旧的套接字文件（如果有）
        try:
            os.unlink(self._socket_path)
        except OSError:
            pass
        # 绑定套接字到文件
        self._server_socket.bind(self._socket_path)
        # 监听传入连接
        self._server_socket.listen(5)
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            while True:
                client_socket, _ = self._server_socket.accept()
                executor.submit(self._handle_client, client_socket)

    def _handle_client(self, client_socket: socket):
        self._thread_local.client_socket = client_socket
        buffer = b""
        try:
            while True:
                # 接收客户端发送的命令
                data = client_socket.recv(1024)
                if not data:
                    self._thread_local.client_socket = None
                    break  # 如果没有接收到数据，退出循环
                buffer += data
                if b"\0" in data:
                    # 处理命令
                    msg = buffer.decode('utf-8').replace("\0", "")
                    buffer = b""
                    print("received command: {}".format(msg))
                    try:
                        self._execute(msg.strip())
                    except Exception:
                        traceback.print_exc()
        finally:
            # 关闭客户端连接
            client_socket.close()

    def cli_print(self, msg: str):
        self._thread_local.client_socket.send(f"{msg}\n".encode('utf-8'))

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
            self.cli_print(f"{func.__name__} success")
        except Exception as e:
            self.cli_print(f"{func.__name__} raised exception: {e}")

    return wrapper


class MmcTest(TestServer):
    def __init__(self, socket_id: int):
        super().__init__(socket_id)
        self._init_cmds()

    def _init_cmds(self):
        cmds = [
            # CliCommand("initmb", "init memory bridge, initmb [block_size] [layer_num] [block_num]", self.initmb, 3),
        ]
        self.register_command(cmds)

    @result_handler
    def print(self):
        self.cli_print("test print info")


if __name__ == "__main__":
    if len(sys.argv) <= 1:
        print("Please input app_id when starting the process.")
        sys.exit(1)
    app_id = int(sys.argv[1])
    print(f"Start app_id: {app_id}")
    server = MmcTest(app_id)
    server.start()
