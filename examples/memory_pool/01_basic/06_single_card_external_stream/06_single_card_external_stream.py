# Copyright: (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
import torch
import torch_npu
import memfabric_hybrid as mf
from memfabric_hybrid import bm

ONE_GIB = 1 << 30  # 1 GB
COPY_BYTES = (4 * 1024, 64 * 1024, 1024 * 1024)  # 4 KB, 64 KB, 1 MB
STORE_URL = "tcp://127.0.0.1:8570"
WORLD_SIZE = 1
DEVICE_ID = 0


def main():
    torch.npu.set_device(0)
    mf.set_log_level(1)  # set log level to info.
    assert mf.initialize() == 0, "mf.initialize failed"
    bm_inited = False
    try:
        cfg = bm.BmConfig()
        # initialize the big memory pool.
        assert bm.initialize(STORE_URL, WORLD_SIZE, DEVICE_ID, cfg) == 0, "bm.initialize failed"
        bm_inited = True
        # register DRAM in the big memory pool.
        handle = bm.create2(id=0, local_dram_size=0, max_dram_size=0, local_hbm_size=ONE_GIB, max_hbm_size=ONE_GIB,
                            data_op_type=bm.BmDataOpType.SDMA)
        # join the big memory pool.
        assert handle.join() == 0, "join failed"
        # get the GVA of the host contribution in the pool.
        npu_gva = handle.peer_rank_ptr(0, bm.BmMemType.DEVICE)
        assert npu_gva != 0, "peer_rank_ptr(HOST) returned 0"
        # copy data from host to pool (H2G) and from pool to host (G2H)
        use_external_stream_flag = 4
        for n in COPY_BYTES:
            src = torch.arange(n // 4, dtype=torch.int32, device="npu").contiguous()
            dst = torch.empty(n // 4, dtype=torch.int32, device="npu")
            assert handle.copy_data(src.data_ptr(), npu_gva, n, bm.BmCopyType.L2G, use_external_stream_flag,
                                    torch.npu.current_stream().npu_stream) == 0, "copy_data L2G failed"
            assert handle.copy_data(npu_gva, dst.data_ptr(), n, bm.BmCopyType.G2L, use_external_stream_flag,
                                    torch.npu.current_stream().npu_stream) == 0, "copy_data G2L failed"
            torch.npu.current_stream().synchronize()
            assert torch.equal(dst, src), "copy round-trip mismatch"
        # leave the big memory pool.
        assert handle.leave() == 0, "leave failed"
        assert mf.get_last_err_msg() == "", mf.get_last_err_msg()
        handle.destroy()
    finally:
        if bm_inited:
            bm.uninitialize(0)
        mf.uninitialize()
    print("06_single_card_external_stream ok")


if __name__ == "__main__":
    main()
