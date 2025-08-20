import time
import signal
import torch
import uuid
import acl
from enum import Enum
from pymmc import DistributedObjectStore

"""
This py is used for testing leader election in k8s cluster
"""


class MmcDirect(Enum):
    COPY_L2G = 0
    COPY_G2L = 1
    COPY_G2H = 2
    COPY_H2G = 3


def malloc_cpu(layer_num: int = 1, block_num: int = 1, min_block_size: int = 1024):
    if min_block_size <= 0:
        return None
    raw_blocks = torch.rand(
        size=(layer_num, block_num, min_block_size // 2),  # torch.float16占两个字节所以除以2
        dtype=torch.float16,
        device=torch.device('cpu')
    )
    return raw_blocks


def acl_set_device():
    acl.init()
    ret = acl.rt.set_device(0)
    if ret != 0:
        raise RuntimeError("acl set device failed")


class MemCacheStore:
    def __init__(self):
        acl_set_device()
        self.store = DistributedObjectStore()
        ret = self.store.init(0)
        print(f"init结果: {ret}")

        self.key = "key_loop_" + str(uuid.uuid4())
        self.key_rebuild_prefix = "key_rb_"
        self.kv_size = 1024
        self.rebuild_count = 32
        self.key_batch_rb_prefix = "key_b_rb_"
        self.batch_rebuild_128 = 128
        self.batch_rebuild_1024 = 1024
        self.tensor = malloc_cpu(min_block_size=self.kv_size)
        self.tensor_get = malloc_cpu(min_block_size=self.kv_size)

    def put_from(self):
        direct = int(MmcDirect.COPY_H2G.value)
        ret = self.store.put_from(self.key, self.tensor.data_ptr(), self.kv_size, direct)
        print(f"put_from 结果: {self.key=}, {ret=}", flush=True)

    def put_from_rebuild(self):
        """测试 HA rebuild"""
        direct = int(MmcDirect.COPY_H2G.value)
        for i in range(self.rebuild_count):
            key = self.key_rebuild_prefix + str(i)
            ret = self.store.put_from(key, self.tensor.data_ptr(), self.kv_size, direct)
            print(f"put_from_rebuild 结果: {key=}, {ret=}", flush=True)

    def batch_put_from_rebuild(self):
        direct = int(MmcDirect.COPY_H2G.value)
        sizes = []
        data_ptrs = []
        for _ in range(self.batch_rebuild_1024):
            sizes.append(self.kv_size)
            tensor = malloc_cpu(min_block_size=self.kv_size)
            data_ptrs.append(tensor.data_ptr())

        for j in range(self.batch_rebuild_128):
            keys = []
            for i in range(self.batch_rebuild_1024):
                keys.append(self.key_batch_rb_prefix + str(j * self.batch_rebuild_1024 + i))
            ret = self.store.batch_put_from(keys, data_ptrs, sizes, direct)
            print(f"batch_put_from_rebuild 结果: {j=}, ret0={ret[0]}, ret1023={ret[-1]}", flush=True)

    def get_into(self):
        direct = int(MmcDirect.COPY_G2H.value)
        ret = self.store.get_into(self.key, self.tensor_get.data_ptr(), self.kv_size, direct)
        print(f"get_into 结果: {self.key=}, {ret=}", flush=True)

    def get_into_rebuild(self):
        direct = int(MmcDirect.COPY_G2H.value)
        for i in range(self.rebuild_count):
            key = self.key_rebuild_prefix + str(i)
            ret = self.store.get_into(key, self.tensor_get.data_ptr(), self.kv_size, direct)
            print(f"get_into_rebuild 结果: {key=}, {ret=}", flush=True)

    def batch_get_into_rebuild(self):
        direct = int(MmcDirect.COPY_G2H.value)
        sizes = []
        data_ptrs = []
        for _ in range(self.batch_rebuild_1024):
            sizes.append(self.kv_size)
            tensor = malloc_cpu(min_block_size=self.kv_size)
            data_ptrs.append(tensor.data_ptr())
        success = 0
        for j in range(self.batch_rebuild_128):
            keys = []
            for i in range(self.batch_rebuild_1024):
                keys.append(self.key_batch_rb_prefix + str(j * self.batch_rebuild_1024 + i))
            ret = self.store.batch_get_into(keys, data_ptrs, sizes, direct)
            success += 1 if ret[-1] == 0 else 0
            print(f"batch_get_into_rebuild 结果: {j=}, ret0={ret[0]}, ret1023={ret[-1]}", flush=True)
        return success

    def is_exist(self, prompt):
        ret = self.store.is_exist(self.key)
        print(f"{prompt}, is_exist 结果: {self.key=}, {ret=}", flush=True)

    def is_exist_rebuild(self):
        """测试 HA rebuild"""
        for i in range(self.rebuild_count):
            key = self.key_rebuild_prefix + str(i)
            ret = self.store.is_exist(key)
            print(f"is_exist_rebuild 结果: {key=}, {ret=}", flush=True)

    def remove(self):
        ret = self.store.remove(self.key)
        print(f"remove 结果: {self.key=}, {ret=}", flush=True)

    def test_loop(self):
        loop = 0
        while True:
            print(f"========= is_exist_rebuild start: {loop=} ===============", flush=True)
            self.is_exist_rebuild()
            print(f"========= is_exist_rebuild end: {loop=} ===============", flush=True)
            print(f"========= get_into_rebuild start: {loop=} ===============", flush=True)
            self.get_into_rebuild()
            print(f"========= get_into_rebuild end: {loop=} ===============", flush=True)
            print(f"========= batch_get_into_rebuild start: {loop=} ===============", flush=True)
            success = self.batch_get_into_rebuild()
            print(f"========= batch_get_into_rebuild end: {loop=}, {success=} ===============", flush=True)
            time.sleep(1)
            self.put_from()
            time.sleep(1)
            self.is_exist(f"loop {loop} after put")
            time.sleep(1)
            self.get_into()

            time.sleep(1)
            self.remove()
            time.sleep(1)
            self.is_exist(f"loop {loop} after remove")
            loop += 1

            print(f"========= test_loop end: {loop=} ===============", flush=True)
            time.sleep(30)  # 休眠10秒，减少CPU占用

    def close(self):
        ret = self.store.close()
        print(f"close结果: {ret}")


meta_store = MemCacheStore()


# 定义信号处理函数
def handle_signal(signal_num, frame):
    """
    信号处理函数
    :param signal_num: 信号编号
    :param frame: 当前栈帧（通常用不到）
    """
    signal_names = {
        signal.SIGINT: "SIGINT (Ctrl+C)",
        signal.SIGTERM: "SIGTERM (终止信号)",
        signal.SIGUSR1: "SIGUSR1 (用户自定义信号1)"
    }
    signal_name = signal_names.get(signal_num, f"未知信号 ({signal_num})")
    print(f"\n收到信号: {signal_name}")

    # 示例：收到SIGINT时准备退出
    if signal_num == signal.SIGINT or signal_num == signal.SIGTERM:
        print("准备优雅退出...")
        # 这里可以添加清理操作（如关闭文件、释放资源等）

        meta_store.close()

        time.sleep(1)  # 模拟清理过程
        print("退出程序")
        exit(0)


if __name__ == "__main__":
    # 注册信号处理函数
    # 监听 SIGINT (Ctrl+C)
    signal.signal(signal.SIGINT, handle_signal)
    # 监听 SIGTERM（kill 命令默认发送的信号）
    signal.signal(signal.SIGTERM, handle_signal)
    # 监听用户自定义信号 SIGUSR1
    signal.signal(signal.SIGUSR1, handle_signal)

    # 无限循环保持程序运行
    try:
        meta_store.is_exist_rebuild()
        meta_store.put_from_rebuild()
        print(f"========= batch_put_from_rebuild start ===============", flush=True)
        meta_store.batch_put_from_rebuild()
        time.sleep(10)  # 休眠10秒，减少CPU占用
        print(f"========= batch_put_from_rebuild end ===============", flush=True)
        meta_store.test_loop()
    finally:
        # 捕获 Ctrl+C 中断信号，优雅退出
        print("程序被信号终止")