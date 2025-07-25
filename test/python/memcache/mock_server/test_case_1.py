import json
import os
import socket
import time


class TestClient:
    def __init__(self, socket_id):
        self._socket_path = os.path.join(f"{os.path.dirname(os.path.abspath(__file__))}", f"mmc_{socket_id}.socket")
        self._client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._client.connect(self._socket_path)

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
        self.execute("close_mmc")

    def put(self, key, value):
        return self.execute("put", [key, value.decode('utf-8')])

    def get(self, key):
        return self.execute("get", [key]).encode('utf-8')

    def put_from(self, key, index: int, media: int):
        return self.execute("put_from", [key, index, media])

    def get_into(self, key, index: int, media: int):
        return self.execute("get_into", [key, index, media])

    def tensor_sum(self, index: int, media: int):
        return self.execute("tensor_sum", [index, media])

    def is_exist(self, key):
        return self.execute("is_exist", [key])

    def remove(self, key):
        return self.execute("remove", [key])

    def _send_request(self, request: str):
        self._client.sendall(f"{request}\0".encode('utf-8'))

    def _read_response(self, timeout=10, chunk_size=1024) -> str:
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


if __name__ == "__main__":
    client = TestClient(123)
    client.execute("help")
    client.execute("getServerCommands")

    test_key = "test"
    media = 0

    print("============ test cpu =========== ")
    client.init_mmc()
    client.is_exist(test_key)
    client.put_from(test_key, 0, media)
    client.is_exist(test_key)
    client.get_into(test_key, 1, media)
    client.tensor_sum(0, media)
    client.tensor_sum(1, media)
    client.remove(test_key)
    client.is_exist(test_key)

    print("============ test npu =========== ")
    media = 1
    client.put_from(test_key, 0, media)
    client.is_exist(test_key)
    client.get_into(test_key, 1, media)
    client.tensor_sum(0, media)
    client.tensor_sum(1, media)
    client.remove(test_key)
    client.is_exist(test_key)

    client.close_mmc()
