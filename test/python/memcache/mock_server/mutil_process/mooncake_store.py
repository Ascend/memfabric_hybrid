# Standard
from dataclasses import dataclass
from mooncake.store import MooncakeDistributedStore, ReplicateConfig  # type: ignore
from mooncake.engine import TransferEngine  # type: ignore

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
        self.transfer_engine_ = TransferEngine()
        ret_value = self.transfer_engine_.initialize(config.local_hostname,
                                               "P2PHANDSHAKE",
                                               "ascend",
                                               config.device_name)
        if ret_value != 0:
            raise RuntimeError(
                f"TransferEngine initialization failed with ret_value: {ret_value}"
            )
        self.local_hostname_ = config.local_hostname + ":" + str(self.transfer_engine_.get_rpc_port())
        self.store = MooncakeDistributedStore()
        ret = self.store.setup(self.local_hostname_,
                               config.metadata_server,
                               config.global_segment_size,
                               config.local_buffer_size,
                               config.protocol,
                               config.device_name,
                               config.master_server_address,
                               self.transfer_engine_.get_engine())
        if ret != 0:
            msg = "Initialize mooncake failed."
            raise RuntimeError(msg)

    def exists(self, key: str) -> bool:
        return self.store.is_exist(key) == 1

    def register(self, ptr, length):
        print("Registering KV cache: ptr=0x%x, length=%d", ptr, length)
        try:
            self.store.register_buffer(ptr, length)
        except Exception as e:
            raise f"Mooncake memory registration failed. Error is: {e}"

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

    def get_batch(self, keys: list[str], addrs: list[list[int]], sizes: list[list[int]]):
        try:
            res = self.store.batch_get_into_multi_buffers(keys, addrs, sizes, True)
            for value in res:
                if value < 0:
                    raise f"Failed to get key {keys},res:{res}"
            return res
        except Exception as e:
            raise f"Failed to get key {keys}. {e}"

    def put_batch(self, keys: list[str], addrs: list[list[int]], sizes: list[list[int]]):
        try:
            config = ReplicateConfig()
            config.preferred_segment = self.local_hostname_
            config.prefer_alloc_in_same_node = True
            res = self.store.batch_put_from_multi_buffers(
                keys, addrs, sizes, config)
            for value in res:
                if value < 0:
                    raise f"Failed to put key {keys},res:{res}"
            return res
        except Exception as e:
            raise f"Failed to put key {keys},error:{e}"

    def close(self):
        self.store.close()
        print("Closed the mooncake store connection")