# Copyright: (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
import multiprocessing
import logging
import argparse
from typing import List

import torch
import torch_npu

import mf_smem
from mf_smem import bm

COPY_SIZE = 4096
GVA_SIZE = 1024 * 1024 * 1024
HYBM_INIT_GVM_FLAG = 2


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


def child_init(device_id: int, rank_id: int, rank_size: int, url: str, auto_ranking: bool):
    ret = mf_smem.initialize()
    if ret != 0:
        logging.error(f'rank: {rank_id}, rank_size: {rank_size}, url: {url} initialize failed: {ret}')
        return ret

    config = bm.BmConfig()
    config.auto_ranking = auto_ranking
    if not auto_ranking:
        config.rank_id = rank_id
    config.flags = HYBM_INIT_GVM_FLAG
    config.set_nic("tcp://127.0.0.1:1234")  # for device port
    ret = bm.initialize(store_url=url, world_size=rank_size, device_id=device_id, config=config)
    if ret != 0:
        logging.error(f'smem BM initialize failed: {ret}')
        return ret

    return 0


def child_process(device_id: int, rank_id: int, rank_size: int, url: str, auto_ranking: bool,
                  barriers: List[multiprocessing.Barrier]):
    torch.npu.set_device(device=device_id)
    _stream = torch_npu.npu.Stream(device=torch.npu.current_device())

    ret = child_init(device_id=device_id, rank_id=rank_id, rank_size=rank_size, url=url, auto_ranking=auto_ranking)
    if ret != 0:
        logging.error(f'child process rank: {rank_id}, rank_size: {rank_size} initialize failed: {ret}')
        return

    bm_handle = bm.create(id=0, local_dram_size=GVA_SIZE, local_hbm_size=0, data_op_type=bm.BmDataOpType.DEVICE_RDMA)
    bm_handle.join()

    logging.info('==================== waiting at barrier 1')
    barriers[0].wait()
    logging.info('==================== barrier 1 finished, start test')

    address = bm_handle.peer_rank_ptr(peer_rank=rank_id, mem_type=bm.BmMemType.HOST)
    logging.info(f'==================== got local GVA: {address}')
    remote = bm_handle.peer_rank_ptr(peer_rank=((rank_id + 1) % rank_size), mem_type=bm.BmMemType.HOST)
    cpu_src_tensor = generate_host_tensor(rank_id)
    npu_tensor = cpu_src_tensor.npu(device_id)
    bm_handle.copy_data(src_ptr=cpu_src_tensor.data_ptr(), dst_ptr=remote, size=COPY_SIZE, type=bm.BmCopyType.H2G)
    bm_handle.copy_data(src_ptr=npu_tensor.data_ptr(), dst_ptr=remote + COPY_SIZE, size=COPY_SIZE,
                        type=bm.BmCopyType.L2G)

    logging.info('==================== waiting at barrier 2')
    barriers[1].wait()
    logging.info('==================== barrier 2 finished for copy data')

    cpu_dst_tensor = generate_host_tensor((rank_id + rank_size - 1) % rank_size)
    bm_handle.copy_data(src_ptr=address, dst_ptr=cpu_src_tensor.data_ptr(), size=COPY_SIZE, type=bm.BmCopyType.G2H)
    if not torch.equal(cpu_src_tensor, cpu_dst_tensor):
        logging.error(f'check G2H data failed for rank: {rank_id}')
        return

    bm_handle.copy_data(src_ptr=address + COPY_SIZE, dst_ptr=npu_tensor.data_ptr(), size=COPY_SIZE,
                        type=bm.BmCopyType.G2L)
    if not torch.equal(cpu_src_tensor, npu_tensor.cpu()):
        logging.error(f'check G2L data failed for rank: {rank_id}')
        return

    del bm_handle

    logging.info('==================== waiting at barrier 3')
    barriers[2].wait()
    logging.info('==================== barrier 3 finished for all test.')


def str_to_bool(v):
    if isinstance(v, bool):
        return v
    if v.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif v.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')


def main_process():
    parser = argparse.ArgumentParser(description='Example for BigMemory in SMEM.')
    parser.add_argument('--world_size', type=int, help='Number of cards used by the entire cluster.', required=True)
    parser.add_argument('--local_ranks', type=int, help='Number of cards used on the local node.', required=True)
    parser.add_argument('--rank_start', type=int, default=0,
                        help='Start value of the rank ID of the node. The value range of the rank ID of the node is'
                             ' [RANK_START, RANK_START + LOCAL_RANK_SIZE).')
    parser.add_argument('--url', type=str,
                        help='Listening IP address and port number of the configStore server, for example,'
                             ' tcp://<ip>:<port>.',
                        required=True)
    parser.add_argument('--auto_ranking', type=str_to_bool,
                        help='If autorank is enabled, the BM automatically generates a global rank ID, which does '
                             'not need to be specified. The default value is false.',
                        default=False)

    args = parser.parse_args()
    logging.info(f'example for BM, world_size:{args.world_size}, local_ranks:{args.local_ranks}, '
                 f'rank_start:{args.rank_start}, url={args.url}, auto-ranking={args.auto_ranking}')

    barriers = [multiprocessing.Barrier(args.local_ranks) for i in range(3)]

    children = []
    for i in range(0, args.local_ranks):
        p = multiprocessing.Process(target=child_process,
                                    args=(i, i + args.rank_start, args.world_size, args.url, args.auto_ranking,
                                          barriers))
        p.start()
        children.append(p)

    for p in children:
        p.join()

    logging.info('main process exited.')


if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG,
                        format='%(process)d - %(asctime)s - %(levelname)s - %(message)s - %(lineno)d')
    main_process()
