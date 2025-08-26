import json
import socket
import time
from typing import List


class TestClient:
    def __init__(self, ip, port):
        self._client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._client.connect((ip, int(port)))

    def __del__(self):
        self._client.close()

    def execute(self, cmd: str, args: list=None):
        request = {
            "cmd": cmd,
            "args": args if args else []
        }
        self._send_request(json.dumps(request))
        response = self._read_response()
        print(f"command: {cmd}\n"
              f"response: {response}\n")
        return response

    def init_mmc(self):
        return self.execute("init_mmc")

    def close_mmc(self):
        return self.execute("close_mmc")

    def put(self, key, value):
        return self.execute("put", [key, value.decode('utf-8')])

    def get(self, key):
        return self.execute("get", [key]).encode('utf-8')

    def put_from(self, key, size: int, media: int):
        return self.execute("put_from", [key, size, media])

    def get_into(self, key, size: int, media: int):
        return self.execute("get_into", [key, size, media])

    def batch_get_into(self, keys: List[str], sizes: List[int], media: int):
        return self.execute("batch_get_into", [keys, sizes, media])

    def batch_put_from(self, keys: List[str], sizes: List[int], media: int):
        return self.execute("batch_put_from", [keys, sizes, media])

    def put_from_layers(self, key: str, sizes: List[int], media: int):
        return self.execute("put_from_layers", [key, sizes, media])

    def get_into_layers(self, key: str, sizes: List[int], media: int):
        return self.execute("get_into_layers", [key, sizes, media])

    def batch_put_from_layers(self, keys: List[str], sizes: List[List[int]], media: int):
        return self.execute("batch_put_from_layers", [keys, sizes, media])

    def batch_get_into_layers(self, keys: List[str], sizes: List[List[int]], media: int):
        return self.execute("batch_get_into_layers", [keys, sizes, media])

    def is_exist(self, key):
        return self.execute("is_exist", [key])

    def batch_is_exit(self, keys: list):
        return self.execute("batch_is_exist", [keys])

    def remove(self, key):
        return self.execute("remove", [key])

    def batch_remove(self, keys: list):
        return self.execute("remove_batch", [keys])

    def get_key_info(self, key):
        return self.execute("get_key_info", [key])

    def batch_get_key_info(self, keys: list):
        return self.execute("batch_get_key_info", [keys])

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
