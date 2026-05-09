import os
import sys
import pickle
from pathlib import Path

import torch
import torch_npu
import zbal
from zbal import record_memory_history, dump_snapshot, simulate_init


def init(use_sim=False):
    # This will allocate memory in the device using the new allocator
    local_rank = int(os.environ.get("LOCAL_RANK", 0))
    world_size = int(os.environ.get("WORLD_SIZE", 1))
    device_id = local_rank

    zbal.zbal_set_logger_level(2)
    mem = 1024 * 1024 * 1024
    if use_sim:
        zbal.switch_to_allocator()
        torch.npu.set_device(device_id)

        simulate_init(0x80000, mem)
    else:
        if not zbal.zbal_init(world_size, device_id, local_rank, mem):
            print(f"zbal_init failed on rank {local_rank}.")
            exit(-1)
        else:
            print(f"zbal_init success on rank {local_rank}")


def malloc(size, stream):
    with torch.npu.stream(stream):
        return torch.npu.caching_allocator_alloc(size)


def free(addr):
    torch.npu.caching_allocator_delete(addr)


def load_pickle_snapshot(filename):
    with open(filename, 'rb') as f:
        data = pickle.load(f)
    return data


if __name__ == '__main__':
    ori_pickle_path = Path(sys.argv[1])
    new_pickle_path = f"{ori_pickle_path.stem}_replay{ori_pickle_path.suffix}"

    addr_map = {}
    stream_map = {}

    ori_snapshot = load_pickle_snapshot(ori_pickle_path.resolve())
    device_traces = next((traces for traces in ori_snapshot['device_traces'] if len(traces) > 2000), None)

    init(use_sim=True)  # init zbal(including switch to dma/sma)

    record_memory_history("all", sys.maxsize)

    for idx, te in enumerate(device_traces):
        action = te['action']
        size = te['size']
        stream_id = te['stream']
        addr = te['addr']

        if action == "alloc" or action == 'empty_cache' or action == 'free_completed':
            if action == 'alloc':
                stream = stream_map.setdefault(stream_id, torch.npu.Stream())
                addr_map[addr] = malloc(size, stream)
            elif action == 'free_completed':
                free(addr_map[addr])
                del addr_map[addr]
            elif action == 'empty_cache':
                torch.npu.empty_cache()

    new_snapshot = dump_snapshot()
    record_memory_history(None, 1)
    with open(new_pickle_path, 'wb') as f:
        pickle.dump(new_snapshot, f)

