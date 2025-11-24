
from multiprocessing import Process
import os

import torch
import torch_npu

#from mutil_process import process_count, write_worker
from mooncake_mutil_process import process_count, write_worker

if __name__ == "__main__":
    print(f"主进程 PID: {os.getpid()}")
    process = []
    # 创建两个子进程
    for index in range(process_count):
        p = Process(target=write_worker, args=(index,))
        p.start()
        process.append(p)

    for i in range(process_count):
        process[i].join()
    print("所有子进程结束。")

