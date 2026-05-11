# Copyright: (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
import torch
import memfabric_hybrid as mf
from memfabric_hybrid import bm

ONE_GIB = 1 << 30 # 1 GB
COPY_BYTES = (4 * 1024, 64 * 1024, 1024 * 1024)  # 4 KB, 64 KB, 1 MB
STORE_URL = "tcp://127.0.0.1:8570"
WORLD_SIZE = 1
DEVICE_ID = 0


def main():
    mf.set_log_level(1)  # set log level to info.
    assert mf.initialize() == 0, "mf.initialize failed"
    bm_inited = False
    try:
        cfg = bm.BmConfig()
        # initialize the big memory pool.
        assert bm.initialize(STORE_URL, WORLD_SIZE, DEVICE_ID, cfg) == 0, "bm.initialize failed"
        bm_inited = True
        # register DRAM in the big memory pool.
        handle = bm.create2(
            id=0,
            local_dram_size=ONE_GIB,
            max_dram_size=ONE_GIB,
            data_op_type=bm.BmDataOpType.DEVICE_RDMA,
        )
        # join the big memory pool.
        assert handle.join() == 0, "join failed"
        # get the GVA of the host contribution in the pool.
        host_gva = handle.peer_rank_ptr(0, bm.BmMemType.HOST)
        assert host_gva != 0, "peer_rank_ptr(HOST) returned 0"
        # copy data from host to pool (H2G) and from pool to host (G2H)
        for n in COPY_BYTES:
            src = torch.arange(n // 4, dtype=torch.int32).contiguous()
            dst = torch.empty(n // 4, dtype=torch.int32)
            assert handle.copy_data(src.data_ptr(), host_gva, n, bm.BmCopyType.H2G, 0) == 0, "copy_data H2G failed"
            assert handle.copy_data(host_gva, dst.data_ptr(), n, bm.BmCopyType.G2H, 0) == 0, "copy_data G2H failed"
            assert torch.equal(dst, src), "copy round-trip mismatch"
        # leave the big memory pool.
        assert handle.leave() == 0, "leave failed"
        assert mf.get_last_err_msg() == "", mf.get_last_err_msg()
        handle.destroy()
    finally:
        if bm_inited:
            bm.uninitialize(0)
        mf.uninitialize()
    print("01_single_device_dram_pool ok")


if __name__ == "__main__":
    main()