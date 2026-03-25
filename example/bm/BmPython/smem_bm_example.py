# Copyright: (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
import multiprocessing
import logging
import argparse
from typing import List

import torch

import memfabric_hybrid
from memfabric_hybrid import bm
from memfabric_hybrid import set_log_level

COPY_SIZE = 4096
GVA_SIZE = 1024 * 1024 * 1024


def generate_host_tensor(seed: int):
    count = COPY_SIZE // 4
    t = torch.empty([count], dtype=torch.int32)
    base = seed
    mod = 32767
    for i in range(0, count):
        base = (base * 23 + 17) % mod
        if ((i + seed) % 3) == 0:
            t[i] = -base
        else:
            t[i] = base
    return t


def get_bm_protocol(protocol):
    if protocol == "device_rdma":
        return bm.BmDataOpType.DEVICE_RDMA
    elif protocol == "device_sdma":
        return bm.BmDataOpType.SDMA
    elif protocol == "host_rdma":
        return bm.BmDataOpType.HOST_RDMA
    elif protocol == "host_urma":
        return bm.BmDataOpType.HOST_URMA
    elif protocol == "host_tcp":
        return bm.BmDataOpType.HOST_TCP
    raise RuntimeError(f"Not support {protocol}")


def child_init(device_id: int, rank_id: int, world_size: int, url: str, nic: str, auto_ranking: bool):
    ret = memfabric_hybrid.initialize()
    if ret != 0:
        logging.error(f'rank: {rank_id}, world_size: {world_size}, url: {url} initialize failed: {ret}')
        return ret

    config = bm.BmConfig()
    config.auto_ranking = auto_ranking
    if not auto_ranking:
        config.rank_id = rank_id
    config.set_nic(f"tcp://{nic}:1234")  # for device port
    ret = bm.initialize(store_url=url, world_size=world_size, device_id=device_id, config=config)
    if ret != 0:
        logging.error(f'smem BM initialize failed: {ret}')
        return ret

    return 0


def child_process(protocol: str, rank_id: int, device_id: int, local_ranks: int, world_size: int, url: str, nic,
                  auto_ranking: bool, is_second_mapping: bool,
                  barriers: List[multiprocessing.Barrier]):
    ret = child_init(device_id=device_id, rank_id=rank_id, world_size=world_size, url=url, nic=nic,
                     auto_ranking=auto_ranking)
    if ret != 0:
        logging.error(f'child process rank: {rank_id}, world_size: {world_size} initialize failed: {ret}')
        return

    bm_protocol = get_bm_protocol(protocol)
    max_dram_size = 257 << 30 if is_second_mapping else 1 << 30
    bm_handle = bm.create2(id=0, local_dram_size=1 << 30, max_dram_size=max_dram_size, local_hbm_size=0, max_hbm_size=0,
                           data_op_type=bm_protocol, is_second_mapping=is_second_mapping)
    bm_handle.join()

    logging.info('==================== waiting at barrier 1')
    barriers[0].wait()
    logging.info('==================== barrier 1 finished, start test')

    address = bm_handle.peer_rank_ptr(peer_rank=rank_id, mem_type=bm.BmMemType.HOST)
    logging.info(f'==================== got local GVA: {address}')

    logging.info(f'step 1: write to rank: {rank_id}')
    local_host_ptr = bm_handle.peer_rank_ptr(peer_rank=rank_id, mem_type=bm.BmMemType.HOST)
    count = 100
    src_tensor = torch.ones([count, 1024], dtype=torch.int32)
    src_ptrs = []
    local_host_ptrs = []
    sizes = []
    addr_offset = 0
    for i in range(count):
        src_ptrs.append(src_tensor[i].data_ptr())
        local_host_ptrs.append(local_host_ptr + addr_offset)
        size = src_tensor[i].nelement() * src_tensor[i].element_size()
        sizes.append(size)
        addr_offset += size

    result = bm_handle.copy_data_batch(src_addrs=src_ptrs, dst_addrs=local_host_ptrs, sizes=sizes, count=count,
                                       type=bm.BmCopyType.H2G, flags=0)
    assert result == 0, f"copy_data_batch failed: {result=}"
    logging.info("copy_data_batch success")

    logging.info('==================== waiting at barrier 1')
    barriers[1].wait()
    logging.info('==================== barrier 1 finished for copy data')

    logging.info(f'step 2: copy data from local rank: {rank_id} to remote rank: {(rank_id + 1) % local_ranks}')
    remote_host_ptr = bm_handle.peer_rank_ptr(peer_rank=((rank_id + 1) % local_ranks), mem_type=bm.BmMemType.HOST)
    remote_host_ptrs = []
    addr_offset = 0
    for i in range(count):
        remote_host_ptrs.append(remote_host_ptr + addr_offset)
        size = src_tensor[i].nelement() * src_tensor[i].element_size()
        addr_offset += size

    result = bm_handle.copy_data_batch(src_addrs=local_host_ptrs, dst_addrs=remote_host_ptrs, sizes=sizes, count=count,
                                       type=bm.BmCopyType.G2G, flags=0)
    assert result == 0, f"copy_data_batch failed: {result=}"
    logging.info("copy_data_batch success")

    # Same G2G batch path via smem_bm_copy_batch_partial_succeed: expect overall ret==0 and per-element results==0.
    ret, per_item = bm_handle.copy_data_batch_partial_succeed(
        src_addrs=local_host_ptrs, dst_addrs=remote_host_ptrs, sizes=sizes, count=count,
        type=bm.BmCopyType.G2G, flags=0)
    assert ret == 0, f"copy_data_batch_partial_succeed failed: {ret=}"
    assert len(per_item) == count, f"per_item length mismatch: {len(per_item)} != {count=}"
    assert all(r == 0 for r in per_item), f"unexpected per-item errors: {per_item[:16]}..."
    logging.info("copy_data_batch_partial_succeed success")

    logging.info('==================== waiting at barrier 2')
    barriers[2].wait()
    logging.info('==================== barrier 2 finished for copy data')

    logging.info(f'step 3: copy data from remote rank: {(rank_id + 1) % local_ranks} to local rank: {rank_id}')
    result = bm_handle.copy_data_batch(src_addrs=remote_host_ptrs, dst_addrs=local_host_ptrs, sizes=sizes, count=count,
                                       type=bm.BmCopyType.G2G, flags=0)
    assert result == 0, f"copy_data_batch failed: {result=}"
    logging.info("copy_data_batch success")

    logging.info('==================== waiting at barrier 3')
    barriers[3].wait()
    logging.info('==================== barrier 3 finished for copy data')

    dst1_tensor = torch.empty([count, 1024], dtype=torch.int32)
    dst1_ptrs = [dst1_tensor[i].data_ptr() for i in range(count)]
    result = bm_handle.copy_data_batch(src_addrs=local_host_ptrs, dst_addrs=dst1_ptrs, sizes=sizes, count=count,
                                       type=bm.BmCopyType.G2H, flags=0)
    assert result == 0, f"copy_data_batch failed: {result=}"
    logging.info("copy_data_batch success")
    if not torch.equal(src_tensor, dst1_tensor):
        logging.error(f'check G2H data failed for rank: {rank_id}')
        return

    logging.info('==================== waiting at barrier 4')
    barriers[4].wait()
    logging.info('==================== barrier 4 finished for copy data')

    dst2_tensor = torch.empty([count, 1024], dtype=torch.int32)
    dst2_ptrs = [dst2_tensor[i].data_ptr() for i in range(count)]
    result = bm_handle.copy_data_batch(src_addrs=remote_host_ptrs, dst_addrs=dst2_ptrs, sizes=sizes, count=count,
                                       type=bm.BmCopyType.G2H, flags=0)
    assert result == 0, f"copy_data_batch failed: {result=}"
    logging.info("copy_data_batch success")
    if not torch.equal(src_tensor, dst2_tensor):
        logging.error(f'check G2H data failed for rank: {rank_id}')
        return

    logging.info('==================== waiting at barrier 5')
    barriers[5].wait()
    logging.info('==================== barrier 5 finished for copy data')

    del bm_handle

    logging.info('==================== waiting at barrier 6')
    barriers[6].wait()
    logging.info('==================== barrier 6 finished for all test.')


def str_to_bool(v):
    if isinstance(v, bool):
        return v
    if v.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif v.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')


"""
cd example/bm/BmPython

1. device_rdma: 
python3 smem_bm_example.py \
        --world_size 256 \
        --local_ranks 2 \
        --rank_start 0 \
        --url tcp://127.0.0.1:7432 \
        --auto_ranking true

2. device_rdma+second_mapping: 
python3 smem_bm_example.py \
        --world_size 1024 \
        --local_ranks 2 \
        --rank_start 0 \
        --url tcp://127.0.0.1:7432 \
        --auto_ranking true \
        --is_second_mapping true

3. host_rdma: 
python3 smem_bm_example.py \
        --protocol host_rdma \
        --world_size 256 \
        --local_ranks 2 \
        --rank_start 0 \
        --url tcp://127.0.0.1:7432 \
        --auto_ranking true \
        --nic 192.168.100.xxx

4. host_rdma+second_mapping: 
python3 smem_bm_example.py \
        --protocol host_rdma \
        --world_size 1024 \
        --local_ranks 2 \
        --rank_start 0 \
        --url tcp://127.0.0.1:7432 \
        --auto_ranking true \
        --nic 192.168.100.xxx \
        --is_second_mapping true
"""


def main_process():
    parser = argparse.ArgumentParser(description='Example for BigMemory in SMEM.')
    parser.add_argument('--protocol', type=str, help='Protocol for memfaric (default: device_rdma).',
                        choices=['device_rdma', 'device_sdma', 'host_rdma', 'host_urma', 'host_tcp'],
                        default='device_rdma')
    parser.add_argument('--world_size', type=int,
                        help='Number of cards used by the entire cluster.', required=True)
    parser.add_argument('--local_ranks', type=int, help='Number of cards used on the local node.', required=True)
    parser.add_argument('--rank_start', type=int, default=0,
                        help='Start value of the rank ID of the node. The value range of the rank ID of the node is'
                             ' [RANK_START, RANK_START + LOCAL_RANK_SIZE).')
    parser.add_argument('--url', type=str,
                        help='Listening IP address and port number of the configStore server, for example,'
                             ' tcp://<ip>:<port>.',
                        required=True)
    parser.add_argument('--nic', type=str,
                        help='device port nic',
                        required=False,
                        default='127.0.0.1')
    parser.add_argument('--auto_ranking', type=str_to_bool,
                        help='If autorank is enabled, the BM automatically generates a global rank ID, which does '
                             'not need to be specified. The default value is false.',
                        default=False)
    parser.add_argument('--is_second_mapping', type=str_to_bool,
                        help='Is second mapping enabled. (default: false) ',
                        default=False)

    args = parser.parse_args()
    logging.info(
        f'example for BM, protocol:{args.protocol}, world_size:{args.world_size}, local_ranks:{args.local_ranks}, '
        f'rank_start:{args.rank_start}, url={args.url}, auto_ranking={args.auto_ranking}, '
        f'is_second_mapping={args.is_second_mapping}')

    barriers = [multiprocessing.Barrier(args.local_ranks) for i in range(7)]

    children = []
    for i in range(0, args.local_ranks):
        p = multiprocessing.Process(target=child_process,
                                    args=(args.protocol, i, i + args.rank_start, args.local_ranks, args.world_size,
                                          args.url, args.nic, args.auto_ranking, args.is_second_mapping, barriers))
        p.start()
        children.append(p)

    for p in children:
        p.join()

    logging.info('main process exited.')


if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG,
                        format='%(process)d - %(asctime)s - %(levelname)s - %(message)s - %(lineno)d')
    set_log_level(1)  # info
    main_process()
