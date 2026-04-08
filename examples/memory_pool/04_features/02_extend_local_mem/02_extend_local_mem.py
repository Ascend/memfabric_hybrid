# Copyright: (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
import multiprocessing
import logging
import argparse
from typing import List

import torch

import memfabric_hybrid
from memfabric_hybrid import bm
from memfabric_hybrid import set_log_level

GVA_SIZE = 1024 * 1024 * 1024
MAX_GVA_SIZE = GVA_SIZE * 8


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


def copy_data(bm_handle, rank_id: int, world_size: int, offset: int = 0):
    local_host_ptr = bm_handle.peer_rank_ptr(peer_rank=rank_id, mem_type=bm.BmMemType.HOST) + offset
    remote_host_ptr = bm_handle.peer_rank_ptr(peer_rank=((rank_id + 1) % world_size),
                                              mem_type=bm.BmMemType.HOST) + offset
    logging.info(f'==================== get local:{rank_id} GVA:0x{local_host_ptr:X} '
                 f'remote:{((rank_id + 1) % world_size)} GVA:0x{remote_host_ptr:X}')

    local_host_ptrs = []
    remote_host_ptrs = []
    count = 100
    src_ptrs = []
    sizes = []
    addr_offset = 0
    src_tensor = torch.ones([count, 1024], dtype=torch.int32)
    for i in range(count):
        src_ptrs.append(src_tensor[i].data_ptr())
        local_host_ptrs.append(local_host_ptr + addr_offset)
        size = src_tensor[i].nelement() * src_tensor[i].element_size()
        sizes.append(size)
        addr_offset += size
    for i in range(count):
        remote_host_ptrs.append(remote_host_ptr + addr_offset)
        size = src_tensor[i].nelement() * src_tensor[i].element_size()
        addr_offset += size

    # H2RG
    result = bm_handle.copy_data_batch(src_addrs=src_ptrs, dst_addrs=remote_host_ptrs, sizes=sizes, count=count,
                                       type=bm.BmCopyType.H2G, flags=0)
    assert result == 0, f"copy_data_batch H2RG failed: {result=}"
    # RG2G
    result = bm_handle.copy_data_batch(src_addrs=remote_host_ptrs, dst_addrs=local_host_ptrs, sizes=sizes, count=count,
                                       type=bm.BmCopyType.G2G, flags=0)
    assert result == 0, f"copy_data_batch RG2G failed: {result=}"

    # G2H
    dst_tensor = torch.empty([count, 1024], dtype=torch.int32)
    dst_ptrs = [dst_tensor[i].data_ptr() for i in range(count)]
    result = bm_handle.copy_data_batch(src_addrs=local_host_ptrs, dst_addrs=dst_ptrs, sizes=sizes, count=count,
                                       type=bm.BmCopyType.G2H, flags=0)
    assert result == 0, f"copy_data_batch H2RG failed: {result=}"
    logging.info("copy_data_batch success")
    if not torch.equal(src_tensor, dst_tensor):
        logging.error(f'check G2H data failed for rank: {rank_id}')
        return
    logging.info('==================== finished for copy data')


def child_process(protocol: str, rank_id: int, device_id: int, local_ranks: int, world_size: int, url: str, nic,
                  auto_ranking: bool, is_second_mapping: bool,
                  barriers: List[multiprocessing.Barrier]):
    ret = child_init(device_id=device_id, rank_id=rank_id, world_size=world_size, url=url, nic=nic,
                     auto_ranking=auto_ranking)
    if ret != 0:
        logging.error(f'child process rank: {rank_id}, world_size: {world_size} initialize failed: {ret}')
        return

    bm_protocol = get_bm_protocol(protocol)
    bm_handle = bm.create2(id=0, local_dram_size=GVA_SIZE, max_dram_size=MAX_GVA_SIZE, local_hbm_size=0, max_hbm_size=0,
                           data_op_type=bm_protocol, is_second_mapping=is_second_mapping)
    bm_handle.join()
    logging.info('==================== waiting at bm create')
    barriers[0].wait()
    logging.info('==================== all bm create finished')

    copy_data(bm_handle, rank_id, world_size)
    logging.info('==================== waiting at copy data')
    barriers[1].wait()
    logging.info('==================== all bm copy data finished')

    ret = bm_handle.extend_local_mem(bm.BmMemType.HOST, GVA_SIZE)
    assert ret == 0, f"failed to alloca extend memory: {ret}"
    logging.info('==================== waiting at alloc extend memory')
    barriers[2].wait()
    logging.info('==================== all bm alloc extend memory finished')

    copy_data(bm_handle, rank_id, world_size, GVA_SIZE)
    logging.info('==================== waiting at copy data')
    barriers[3].wait()
    logging.info('==================== all bm copy data finished')

    ret = bm_handle.extend_local_mem(bm.BmMemType.HOST, GVA_SIZE)
    assert ret == 0, f"failed to alloca extend memory: {ret}"
    logging.info('==================== waiting at alloc extend memory')
    barriers[4].wait()
    logging.info('==================== all bm alloc extend memory finished')

    copy_data(bm_handle, rank_id, world_size, GVA_SIZE * 2)
    logging.info('==================== waiting at copy data')
    barriers[5].wait()
    logging.info('==================== all bm copy data finished')

    del bm_handle
    logging.info('==================== waiting at bm del')
    barriers[6].wait()
    logging.info('==================== all bm del finished.')
    logging.info(f'==================== rank:{rank_id}, alloc extend memory test ok.')


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
python3 02_extend_local_mem.py \
        --world_size 8 \
        --local_ranks 8 \
        --rank_start 0 \
        --protocol device_rdma \
        --url tcp://127.0.0.1:7432 \

2. device_sdma: 
python3 02_extend_local_mem.py \
        --world_size 8 \
        --local_ranks 8 \
        --rank_start 0 \
        --protocol device_sdma \
        --url tcp://127.0.0.1:7432 \
"""


def main_process():
    parser = argparse.ArgumentParser(description='Example for BigMemory in SMEM.')
    parser.add_argument('--protocol', type=str, help='Protocol for memfaric (default: device_rdma).',
                        choices=['device_rdma', 'device_sdma', 'host_rdma', 'host_urma', 'host_tcp'],
                        default='device_sdma', required=False)
    parser.add_argument('--world_size', type=int,
                        help='Number of cards used by the entire cluster.', required=False, default=8)
    parser.add_argument('--local_ranks', type=int, help='Number of cards used on the local node.',
                        required=False, default=8)
    parser.add_argument('--rank_start', type=int, required=False, default=0,
                        help='Start value of the rank ID of the node. The value range of the rank ID of the node is'
                             ' [RANK_START, RANK_START + LOCAL_RANK_SIZE).')
    parser.add_argument('--url', type=str,
                        help='Listening IP address and port number of the configStore server, for example,'
                             ' tcp://<ip>:<port>.',
                        required=False, default='tcp://127.0.0.1:8570')
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
