# Copyright: (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
import time
import torch
import memfabric_hybrid as mf
from memfabric_hybrid import bm

LOCAL_DRAM_SIZE = 4 << 30 # 4GB
BLOCK_SIZE = 4 << 20 # 4MB block
BATCH_SIZES = (32, 128, 256)
STORE_URL = "tcp://127.0.0.1:8570"
WORLD_SIZE = 1
DEVICE_ID = 0


def main():
    # 1: info, 3: error
    mf.set_log_level(3)
    assert mf.initialize() == 0, "mf.initialize failed"
    bm_inited = False

    try:
        # initialize the big memory pool.
        cfg = bm.BmConfig()
        assert bm.initialize(STORE_URL, WORLD_SIZE, DEVICE_ID, cfg) == 0, "bm.initialize failed"
        bm_inited = True

        # register DRAM in the big memory pool.
        handle = bm.create2(
            id=0,
            local_dram_size=LOCAL_DRAM_SIZE,
            max_dram_size=LOCAL_DRAM_SIZE,
            data_op_type=bm.BmDataOpType.DEVICE_RDMA,
        )
        print(f"Step1: BM initialize SUCCESS")

        # join the big memory pool.
        assert handle.join() == 0, "join failed"

        # get the GVA of the host contribution in the pool.
        host_gva = handle.peer_rank_ptr(0, bm.BmMemType.HOST)
        assert host_gva != 0, "peer_rank_ptr(HOST) returned 0"
        src = torch.arange(BLOCK_SIZE // 4, dtype=torch.int32).contiguous()
        dst = torch.empty(BLOCK_SIZE // 4, dtype=torch.int32)

        # copy data from host to pool (H2G) and from pool to host (G2H)
        batch_results = []
        for batch_size in BATCH_SIZES:
            batch_bytes = batch_size * BLOCK_SIZE
            num_loops = LOCAL_DRAM_SIZE // batch_bytes
            total_bytes = num_loops * batch_bytes
            
            h2g_total_time = 0
            g2h_total_time = 0
            
            for _ in range(num_loops):
                src_addrs = [src.data_ptr() for _ in range(batch_size)]
                dst_addrs = [host_gva + i * BLOCK_SIZE for i in range(batch_size)]
                sizes = [BLOCK_SIZE] * batch_size

                start_time = time.time()
                assert handle.copy_data_batch(
                    src_addrs, dst_addrs, sizes, batch_size, bm.BmCopyType.H2G, 0
                ) == 0, "copy_data_batch H2G failed"
                h2g_total_time += time.time() - start_time

                src_addrs = [host_gva + i * BLOCK_SIZE for i in range(batch_size)]
                dst_addrs = [dst.data_ptr() for _ in range(batch_size)]
                sizes = [BLOCK_SIZE] * batch_size

                start_time = time.time()
                assert handle.copy_data_batch(
                    src_addrs, dst_addrs, sizes, batch_size, bm.BmCopyType.G2H, 0
                ) == 0, "copy_data_batch G2H failed"
                g2h_total_time += time.time() - start_time
                assert torch.equal(dst, src), "bm check ok"

            batch_results.append({
                'batch_size': batch_size,
                'total_bytes': total_bytes,
                'h2g_time': h2g_total_time,
                'g2h_time': g2h_total_time
            })

        # Print H2G results
        for result in batch_results:
            batch_size = result['batch_size']
            total_bytes = result['total_bytes']
            h2g_time_ms = int(result['h2g_time'] * 1000)
            total_gb = total_bytes // (1024 ** 3)
            h2g_throughput = int(total_bytes / (result['h2g_time'] * 1024 * 1024 * 1024))
            print(
                f"H2G  Batch size: {batch_size}, Total size: {total_gb} GB, "  
                f"      Time: {h2g_time_ms}ms, Throughput: {h2g_throughput} GB/s"
            )
        
        # Print G2H results
        for result in batch_results:
            batch_size = result['batch_size']
            total_bytes = result['total_bytes']
            g2h_time_ms = int(result['g2h_time'] * 1000)
            total_gb = total_bytes // (1024 ** 3)
            g2h_throughput = int(total_bytes / (result['g2h_time'] * 1024 * 1024 * 1024))
            print(
                f"G2H  Batch size: {batch_size}, Total size: {total_gb} GB, "  
                f"      Time: {g2h_time_ms}ms, Throughput: {g2h_throughput} GB/s"
            )

        print(f"Step2: copy_data_batch H2G、G2H SUCCESS")
        # leave the big memory pool.
        assert handle.leave() == 0, "leave failed"
        assert mf.get_last_err_msg() == "", mf.get_last_err_msg()
        handle.destroy()
        print(f"Step3: BM destroy SUCCESS")
    finally:
        if bm_inited:
            bm.uninitialize(0)
        mf.uninitialize()
    print("01_copy_data_batch ok")


if __name__ == "__main__":
    main()
