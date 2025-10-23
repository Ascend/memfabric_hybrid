# Standard
from dataclasses import dataclass
import os

METADATA_BYTES_LEN = 24
BASE_PORT = 8790

@dataclass
class MooncakeConfig:
    device: int
    protocol: str
    device_name: str
    local_hostname: str
    metadata_server: str
    global_segment_size: int
    local_buffer_size: int
    master_server_address: str


class Mooncakestore():
    def __init__(self, config: MooncakeConfig):
        try:
            from mooncake.store import MooncakeDistributedStore  # type: ignore
        except ImportError as e:
            raise ImportError(
                "Please install mooncake by following the instructions at "
                "https://github.com/kvcache-ai/Mooncake/blob/main/doc/en/build.md "  # noqa: E501
                "to run vLLM with MooncakeConnector.") from e
        all_device_ids = os.getenv("ASCEND_RT_VISIBLE_DEVICES", None)
        if not all_device_ids:
            device_ids_list = list(range(0, 16))
        else:
            device_ids_list = list(map(int, all_device_ids.split(',')))
        device_id = device_ids_list[config.device]
        local_hostname = config.local_hostname + ":" + str(BASE_PORT + int(device_id)) + \
                         ":npu_" + str(device_id)
        self.store = MooncakeDistributedStore()
        ret = self.store.setup(local_hostname, config.metadata_server,
                               config.global_segment_size,
                               config.local_buffer_size,
                               config.protocol,
                               config.device_name,
                               config.master_server_address)
        if ret != 0:
            msg = "Initialize mooncake failed."
            raise RuntimeError(msg)

    def exists(self, key: str) -> bool:
        return self.store.is_exist(key) == 1

    def batch_exists(self, keys: list[str]) -> list[bool]:
        return self.store.batch_is_exist(keys)

    def get(self, key: str, addr: list[int], size: list[int]):
        expect_res = sum(size)
        key_str = key
        try:
            res = self.store.batch_get_into_ascend(key_str, addr, size)
            if res[0] != expect_res:
                raise f"Failed to get key: [{key_str}] ."
        except Exception:
            raise f"Failed to get key: [{key_str}] ."
        return res

    def put(self, key: str, addr: list[int], size: list[int]):
        key_str = key
        try:
            ret = self.store.batch_put_from_ascend(key_str, addr, size)
            if ret[0] != 0:
                raise f"Failed to put key {key_str}."
        except Exception:
            raise f"Failed to put key {key_str}."

        return ret

    def close(self):
        self.store.close()
        print("Closed the mooncake store connection")