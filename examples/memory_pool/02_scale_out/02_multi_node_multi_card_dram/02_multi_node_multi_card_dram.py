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
import sys
import time
import torch

import memfabric_hybrid as mf
from memfabric_hybrid import bm

STORE_PORT = 8572
ONE_GIB = 1 << 30  # 1GB
COPY_BYTES = 4 * 1024 * 1024  # 4MB
WORLD_SIZE = 2

# After join, give rank 0 time to finish H2G before rank 1 reads peer (no barrier).
POST_JOIN_RANK1_SLEEP_SEC = 3.0


def _run_rank0(head_node_ip: str) -> None:
    store_url = f"tcp://{head_node_ip}:{STORE_PORT}"
    mf.set_log_level(3)
    assert mf.initialize() == 0, "mf.initialize failed"
    bm_inited = False
    try:
        cfg = bm.BmConfig()
        cfg.rank_id = 0
        cfg.start_store = True
        cfg.set_nic(f"tcp://127.0.0.1:10005")
        assert bm.initialize(store_url, WORLD_SIZE, 0, cfg) == 0, "bm.initialize failed"
        bm_inited = True

        handle = bm.create2(
            id=0,
            local_dram_size=ONE_GIB,
            max_dram_size=ONE_GIB,
            data_op_type=bm.BmDataOpType.DEVICE_RDMA,
        )
        print(f"[rank 0] join() (store={store_url}) — may return before rank 1 starts", flush=True)
        assert handle.join() == 0, "join failed"

        gva_me = handle.peer_rank_ptr(0, bm.BmMemType.HOST)
        assert gva_me != 0, "peer_rank_ptr HOST"
        src = torch.arange(COPY_BYTES // 4, dtype=torch.int32).contiguous()
        assert handle.copy_data(src.data_ptr(), gva_me, COPY_BYTES, bm.BmCopyType.H2G, 0) == 0, "H2G r0"
        print(
            "[rank 0] H2G done; pool has payload. Sleeping until Ctrl+C — start rank 1 on node B for verify.",
            flush=True,
        )
        try:
            while True:
                time.sleep(60)
        except KeyboardInterrupt:
            print("[rank 0] interrupted, leaving group", flush=True)

        assert handle.leave() == 0, "leave"
        assert mf.get_last_err_msg() == "", mf.get_last_err_msg()
        handle.destroy()
    finally:
        if bm_inited:
            bm.uninitialize(0)
        mf.uninitialize()
    print("[rank 0] cleanup done", flush=True)


def _run_rank1(head_node_ip: str) -> None:
    store_url = f"tcp://{head_node_ip}:{STORE_PORT}"
    mf.set_log_level(3)
    assert mf.initialize() == 0, "mf.initialize failed"
    bm_inited = False
    try:
        cfg = bm.BmConfig()
        cfg.rank_id = 1
        cfg.start_store = False
        cfg.set_nic(f"tcp://127.0.0.1:10005")
        assert bm.initialize(store_url, WORLD_SIZE, 0, cfg) == 0, "bm.initialize failed"
        bm_inited = True

        handle = bm.create2(
            id=0,
            local_dram_size=ONE_GIB,
            max_dram_size=ONE_GIB,
            data_op_type=bm.BmDataOpType.DEVICE_RDMA,
        )
        print(f"[rank 1] joining (store={store_url})", flush=True)
        assert handle.join() == 0, "join failed"

        time.sleep(POST_JOIN_RANK1_SLEEP_SEC)

        gva_me = handle.peer_rank_ptr(1, bm.BmMemType.HOST)
        gva_peer0 = handle.peer_rank_ptr(0, bm.BmMemType.HOST)
        assert gva_me != 0 and gva_peer0 != 0, "peer_rank_ptr HOST"

        warmup = torch.zeros(COPY_BYTES // 4, dtype=torch.int32).contiguous()
        assert (
            handle.copy_data(warmup.data_ptr(), gva_me, COPY_BYTES, bm.BmCopyType.H2G, 0) == 0
        ), "H2G host to global pool"

        exp = torch.arange(COPY_BYTES // 4, dtype=torch.int32).contiguous()
        got = torch.empty(COPY_BYTES // 4, dtype=torch.int32)
        assert (
            handle.copy_data(gva_peer0, got.data_ptr(), COPY_BYTES, bm.BmCopyType.G2H, 0) == 0
        ), "G2H global pool to host"
        assert torch.equal(got, exp), "data mismatch"

        print("[rank 1] G2H verify OK; leaving", flush=True)
        print("[rank 1] 02_multi_node_multi_card_dram OK", flush=True)
        assert handle.leave() == 0, "leave"
        assert mf.get_last_err_msg() == "", mf.get_last_err_msg()
        handle.destroy()
    finally:
        if bm_inited:
            bm.uninitialize(0)
        mf.uninitialize()


def main() -> None:
    if len(sys.argv) < 2 or sys.argv[1] not in ("0", "1"):
        raise RuntimeError("usage: python3 02_multi_node_multi_card_dram.py <0|1> [head_ip]")
    head_ip = (sys.argv[2] if len(sys.argv) > 2 else input("Head node IP: ")).strip()
    if not head_ip:
        raise RuntimeError("head node IP required")
    if int(sys.argv[1]) == 0:
        _run_rank0(head_ip)
    else:
        _run_rank1(head_ip)


if __name__ == "__main__":
    main()
