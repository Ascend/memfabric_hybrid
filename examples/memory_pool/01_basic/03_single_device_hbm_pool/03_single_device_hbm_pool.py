#!/usr/bin/env python3
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
import torch

import memfabric_hybrid as mf
from memfabric_hybrid import bm

ONE_GIB = 1 << 30  # 1 GB
COPY_BYTES = (4 * 1024, 64 * 1024, 1024 * 1024)  # 4 KB, 64 KB, 1 MB
STORE_URL = "tcp://127.0.0.1:8570"
WORLD_SIZE = 1
DEVICE_ID = 0


def main():
    mf.set_log_level(1)  # Set log level to info.
    assert mf.initialize() == 0, "mf.initialize failed"
    bm_inited = False
    try:
        cfg = bm.BmConfig()
        cfg.auto_ranking = False
        cfg.rank_id = 0
        # Initialize the big memory pool.
        assert bm.initialize(STORE_URL, WORLD_SIZE, DEVICE_ID, cfg) == 0, "bm.initialize failed"
        bm_inited = True

        # Register HBM (not DRAM) in the big memory pool.
        handle = bm.create2(
            id=3,
            local_dram_size=0,
            max_dram_size=0,
            local_hbm_size=ONE_GIB,
            max_hbm_size=ONE_GIB,
            data_op_type=bm.BmDataOpType.DEVICE_RDMA,
            enable_56bits_gva=False,
        )

        # Join the big memory pool.
        assert handle.join() == 0, "join failed"

        # Get the DEVICE GVA of the HBM segment allocated for rank 0.
        # This memory resides on the device (HBM) and is not directly accessible by the host CPU.
        hbm_gva = handle.peer_rank_ptr(0, bm.BmMemType.DEVICE)
        assert hbm_gva != 0, "peer_rank_ptr(DEVICE) returned 0"

        # Copy data: Host → HBM (H2G), then HBM → Host (G2H)
        for n in COPY_BYTES:
            src = torch.arange(n // 4, dtype=torch.int32).contiguous()
            dst = torch.empty(n // 4, dtype=torch.int32)

            # Host to Device (HBM)
            assert handle.copy_data(src.data_ptr(), hbm_gva, n, bm.BmCopyType.H2G, 0) == 0, "copy_data H2G failed"
            # Device (HBM) to Host
            assert handle.copy_data(hbm_gva, dst.data_ptr(), n, bm.BmCopyType.G2H, 0) == 0, "copy_data G2H failed"

            assert torch.equal(dst, src), f"copy round-trip mismatch at size {n}"

        # Leave the big memory pool.
        assert handle.leave() == 0, "leave failed"
        assert mf.get_last_err_msg() == "", mf.get_last_err_msg()
        handle.destroy()

    finally:
        if bm_inited:
            bm.uninitialize(DEVICE_ID)
        mf.uninitialize()

    print("03_single_device_hbm_pool ok")


if __name__ == "__main__":
    main()