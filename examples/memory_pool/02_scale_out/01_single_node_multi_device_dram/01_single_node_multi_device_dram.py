#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
import multiprocessing as mp

import torch

import memfabric_hybrid as mf
from memfabric_hybrid import bm

ONE_GIB = 1 << 30  # 1GB
COPY_BYTES = 4 * 1024 * 1024  # 4MB int32 payload
STORE_URL = "tcp://127.0.0.1:8572"
WORLD_SIZE = 2

RANK_0, DEVICE_0 = 0, 0
RANK_1, DEVICE_1 = 1, 1


def _rank_main(rank_id: int, device_id: int, sync: mp.Barrier):
    mf.set_log_level(3)
    assert mf.initialize() == 0, "mf.initialize failed"
    bm_inited = False
    try:
        cfg = bm.BmConfig()
        cfg.rank_id = rank_id
        cfg.start_store = rank_id == RANK_0
        # DEVICE_RDMA: URL is format + listen port; IP unused (device IP comes from runtime).
        cfg.set_nic("tcp://127.0.0.1:10005")
        assert bm.initialize(STORE_URL, WORLD_SIZE, device_id, cfg) == 0, "bm.initialize failed"
        bm_inited = True

        handle = bm.create2(
            id=0,
            local_dram_size=ONE_GIB,
            max_dram_size=ONE_GIB,
            data_op_type=bm.BmDataOpType.DEVICE_RDMA,
        )
        print(f"[rank {rank_id}] BM initialized (device_id={device_id}, store={STORE_URL})", flush=True)
        assert handle.join() == 0, "join failed"

        peer = 1 - rank_id  # WORLD_SIZE == 2
        gva_me = handle.peer_rank_ptr(rank_id, bm.BmMemType.HOST)
        gva_peer = handle.peer_rank_ptr(peer, bm.BmMemType.HOST)
        assert gva_me != 0 and gva_peer != 0, "peer_rank_ptr HOST"

        sync.wait()
        print(f"[rank {rank_id}] (1/4) GVAs ready (peer_rank={peer})", flush=True)

        # Phase A: r0 H2G -> G2G to r1 -> r1 G2H verify
        if rank_id == RANK_0:
            src = torch.arange(COPY_BYTES // 4, dtype=torch.int32).contiguous()
            assert handle.copy_data(src.data_ptr(), gva_me, COPY_BYTES, bm.BmCopyType.H2G, 0) == 0, "H2G r0"
        sync.wait()

        if rank_id == RANK_0:
            assert handle.copy_data(gva_me, gva_peer, COPY_BYTES, bm.BmCopyType.G2G, 0) == 0, "G2G 0→1"
        sync.wait()

        if rank_id == RANK_1:
            exp = torch.arange(COPY_BYTES // 4, dtype=torch.int32).contiguous()
            got = torch.empty(COPY_BYTES // 4, dtype=torch.int32)
            assert handle.copy_data(gva_me, got.data_ptr(), COPY_BYTES, bm.BmCopyType.G2H, 0) == 0, "G2H r1"
            assert torch.equal(got, exp), "round-trip r0→r1"
            print(f"[rank {rank_id}] (2/4) Phase A OK (r0→r1)", flush=True)
        sync.wait()

        # Phase B: symmetric r1 -> r0
        if rank_id == RANK_1:
            src = (torch.arange(COPY_BYTES // 4, dtype=torch.int32) * 17 + 3).contiguous()
            assert handle.copy_data(src.data_ptr(), gva_me, COPY_BYTES, bm.BmCopyType.H2G, 0) == 0, "H2G r1"
        sync.wait()

        if rank_id == RANK_1:
            assert handle.copy_data(gva_me, gva_peer, COPY_BYTES, bm.BmCopyType.G2G, 0) == 0, "G2G 1→0"
        sync.wait()

        if rank_id == RANK_0:
            exp = (torch.arange(COPY_BYTES // 4, dtype=torch.int32) * 17 + 3).contiguous()
            got = torch.empty(COPY_BYTES // 4, dtype=torch.int32)
            assert handle.copy_data(gva_me, got.data_ptr(), COPY_BYTES, bm.BmCopyType.G2H, 0) == 0, "G2H r0"
            assert torch.equal(got, exp), "round-trip r1→r0"
            print(f"[rank {rank_id}] (3/4) Phase B OK (r1→r0)", flush=True)

        assert handle.leave() == 0, "leave"
        assert mf.get_last_err_msg() == "", mf.get_last_err_msg()
        handle.destroy()
    finally:
        if bm_inited:
            bm.uninitialize(0)
        mf.uninitialize()


def main():
    mp.set_start_method("spawn", force=True)
    sync = mp.Barrier(WORLD_SIZE)

    p0 = mp.Process(target=_rank_main, args=(RANK_0, DEVICE_0, sync))
    p1 = mp.Process(target=_rank_main, args=(RANK_1, DEVICE_1, sync))

    p0.start()
    p1.start()
    p0.join()
    p1.join()

    if p0.exitcode != 0 or p1.exitcode != 0:
        raise RuntimeError(f"child rank failed: p0.exitcode={p0.exitcode}, p1.exitcode={p1.exitcode}")
    print("(4/4) 01_single_node_multi_device_dram: all ranks OK", flush=True)


if __name__ == "__main__":
    main()
