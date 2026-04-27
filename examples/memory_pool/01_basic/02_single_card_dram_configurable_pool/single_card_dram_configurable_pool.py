# Copyright: (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
import torch
import memfabric_hybrid as mf
from memfabric_hybrid import bm

LOCAL_DRAM_SIZE = 2 << 30  # 2GB, adjust this parameter to control the allocated DRAM pool size


def main():
    print("Starting single card DRAM configurable pool test")
    assert mf.initialize() == 0, "mf.initialize failed"
    bm_status = False
    try:
        config = bm.BmConfig()
        # initialize the big memory pool.
        assert bm.initialize(store_url="tcp://127.0.0.1:1234", world_size=1, device_id=0,
                        config=config) == 0, "bm.initialize failed"
        bm_status = True
        # register DRAM in the big memory pool.
        bm_handle = bm.create2(id=0, local_dram_size=LOCAL_DRAM_SIZE, max_dram_size=LOCAL_DRAM_SIZE,
                               data_op_type=bm.BmDataOpType.DEVICE_RDMA, enable_56bits_gva=False)
        # join the big memory pool.
        assert bm_handle.join() == 0, "bm_handle.join failed"
        for size, name in [(4096, "4KB"), (65536, "64KB"), (1048576, "1MB")]:
            print(f"  Testing with {name} data block")
            # get the GVA of the host contribution in the pool.
            local_host_ptr = bm_handle.peer_rank_ptr(peer_rank=0, mem_type=bm.BmMemType.HOST)
            src_tensor = torch.arange(size // 4, dtype=torch.int32).contiguous()
            dst_tensor = torch.empty([size // 4], dtype=torch.int32)
            # copy data from host to pool (H2G) and from pool to host (G2H)
            assert bm_handle.copy_data(src_ptr=src_tensor.data_ptr(), dst_ptr=local_host_ptr,
                                    size=size, type=bm.BmCopyType.H2G, flags=0) == 0, "copy_data H2G failed"
            assert bm_handle.copy_data(src_ptr=local_host_ptr, dst_ptr=dst_tensor.data_ptr(),
                                    size=size, type=bm.BmCopyType.G2H, flags=0) == 0, "copy_data G2H failed"
            assert torch.equal(src_tensor, dst_tensor), f"Data verification FAILED for {name}"
        # leave the big memory pool.
        assert bm_handle.leave() == 0, "bm_handle.leave failed"
        assert mf.get_last_err_msg() == "", mf.get_last_err_msg()
        bm_handle.destroy()
        del bm_handle
    finally:
        if bm_status:
            bm.uninitialize(0)
        mf.uninitialize()
    print(f"✓ Single card DRAM configurable pool test PASSED, local_dram_size: {LOCAL_DRAM_SIZE}")
    return 0


if __name__ == '__main__':
    main()
