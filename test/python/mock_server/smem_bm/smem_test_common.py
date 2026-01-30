#!/usr/bin/env python3.10
#  Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
import json
import socket
import time


class TestClient:
    def __init__(self, ip, port):
        self._client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._client.connect((ip, int(port)))

    def __del__(self):
        self._client.close()

    def execute(self, cmd: str, args: list = None):
        request = {
            "cmd": cmd,
            "args": args if args else []
        }
        self._send_request(json.dumps(request))
        response = self._read_response()
        print(f"command: {cmd}\n"
              f"response: {response}\n")
        return response

    def init_smem_bm(self):
        return self.execute("init_smem_bm")

    def close_smem_bm(self):
        return self.execute("close_smem_bm")

    def copy_data(self, src_ptr: int, dst_ptr: int, size: int, op_type: str, flag: int):
        return int(self.execute("copy_data", [src_ptr, dst_ptr, size, op_type, flag]))

    def bm_copy_batch(self, src_addrs: list[int], dst_addrs: list[int],
                      sizes: list[int], count: int, op_type: str, flag: int):
        return int(self.execute("bm_copy_batch", [src_addrs, dst_addrs, sizes, count, op_type, flag]))

    def get_peer_rank_gva(self, rank_id: int, pool_type: str):
        return int(self.execute("get_peer_rank_gva", [rank_id, pool_type]))

    def alloc_local_memory(self, size: int, memory_type: str):
        return int(self.execute("alloc_local_memory", [size, memory_type]))

    def free_local_memory(self, ptr: int):
        return int(self.execute("free_local_memory", [ptr]))

    def register_memory(self, ptr: int, size: int):
        return int(self.execute("register_memory", [ptr, size]))

    def calc_gva_sum(self, ptr: int, size: int):
        return int(self.execute("calc_gva_sum", [ptr, size]))

    def calc_local_memory_sum(self, ptr: int):
        return int(self.execute("calc_local_memory_sum", [ptr]))

    def _send_request(self, request: str):
        self._client.sendall(f"{request}\0".encode('utf-8'))

    def _read_response(self, timeout=60, chunk_size=1024) -> str:
        buffer_list = []
        start_time = time.time()

        while time.time() - start_time < timeout:
            data = self._client.recv(chunk_size)
            if not data:
                continue
            buffer_list.append(data)
            if b'\0' in data:
                return b''.join(buffer_list).decode('utf-8').rstrip('\0')
            time.sleep(0.01)
        raise TimeoutError("未能在指定时间内收到响应")
