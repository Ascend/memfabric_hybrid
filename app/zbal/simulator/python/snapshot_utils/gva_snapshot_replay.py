import sys
import torch
import torch_npu
from zbal import switch_to_allocator, init_shmem, record_memory_history, dump_snapshot


def _malloc(size, stream):
    with torch.npu.stream(stream):
        return torch.npu.caching_allocator_alloc(size)


def _free(addr):
    torch.npu.caching_allocator_delete(addr)


def replay_snapshot(snapshot, device_id=0, shmem_size=62277025792, dump_replay_snapshot=True):
    device_traces = snapshot['device_traces'][device_id]
    switch_to_allocator()
    torch.npu.set_device(0)
    torch.npu.init()
    init_shmem(0, 1, shmem_size, 0, 'tcp://127.0.0.1:3399', is_simulation=True)

    if dump_replay_snapshot:
        record_memory_history("all", sys.maxsize)

    addr_map = {}
    stream_map = {}

    for idx, trace_entry in enumerate(device_traces):
        action = trace_entry['action']
        size = trace_entry['size']
        stream_id = trace_entry['stream']
        addr = trace_entry['addr']

        if action == "alloc" or action == 'empty_cache' or action == 'free_completed':
            if action == 'alloc':
                stream = stream_map.setdefault(stream_id, torch.npu.Stream())
                addr_map[addr] = _malloc(size, stream)
            elif action == 'free_completed':
                _free(addr_map[addr])
                del addr_map[addr]
            elif action == 'empty_cache':
                torch.npu.empty_cache()

    if dump_replay_snapshot:
        replay_snapshot_result = dump_snapshot()
        record_memory_history(None, 0)
        return replay_snapshot_result
    return None


