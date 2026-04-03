#!/usr/bin/env python3
import torch
import memfabric_hybrid as mf
from memfabric_hybrid import bm
import time

LOCAL_DRAM_SIZE = 4 << 30  # 4GB
BLOCK_SIZE = 4 << 20       # 4MB
BATCH_SIZES = (16, 128, 512)
STORE_URL = "tcp://127.0.0.1:8570"
WORLD_SIZE = 1
DEVICE_ID = 0


def main():
    mf.set_log_level(3) # 1: info, 3:error
    assert mf.initialize() == 0, "mf.initialize failed"
    bm_inited = False

    try:
        cfg = bm.BmConfig()
        assert bm.initialize(STORE_URL, WORLD_SIZE, DEVICE_ID, cfg) == 0, "bm.initialize failed"
        bm_inited = True

        # Create BM with SDMA protocol (no registration needed for local H2G/G2H)
        handle = bm.create2(
            id=0,
            local_dram_size=LOCAL_DRAM_SIZE,
            max_dram_size=LOCAL_DRAM_SIZE,
            data_op_type=bm.BmDataOpType.SDMA,
        )
        print("Step1: BM initialize SUCCESS")

        assert handle.join() == 0, "join failed"

        # Get HOST GVA in the pool (for H2G/G2H)
        host_gva = handle.peer_rank_ptr(0, bm.BmMemType.HOST)
        assert host_gva != 0, "peer_rank_ptr(HOST) returned 0"

        # Allocate source and destination buffers (NO registration)
        src = torch.arange(BLOCK_SIZE // 4, dtype=torch.int32).contiguous()
        dst = torch.empty(BLOCK_SIZE // 4, dtype=torch.int32)

        print("Step2: Skip address registration (SDMA does not require it)")

        TOTAL_BYTES = LOCAL_DRAM_SIZE

        for batch_size in BATCH_SIZES:
            bytes_per_round = batch_size * BLOCK_SIZE
            total_rounds = TOTAL_BYTES // bytes_per_round
            assert TOTAL_BYTES % bytes_per_round == 0

            # ===== H2G =====
            h2g_start = time.time()
            for round_idx in range(total_rounds):
                base_offset = round_idx * bytes_per_round
                src_addrs = [src.data_ptr()] * batch_size
                dst_addrs = [host_gva + base_offset + i * BLOCK_SIZE for i in range(batch_size)]
                sizes = [BLOCK_SIZE] * batch_size

                ret = handle.copy_data_batch(
                    src_addrs, dst_addrs, sizes, batch_size,
                    bm.BmCopyType.H2G, 0
                )
                assert ret == 0, f"H2G failed at batch={batch_size}, ret={ret}"
            h2g_time = time.time() - h2g_start

            # ===== G2H =====
            g2h_start = time.time()
            for round_idx in range(total_rounds):
                base_offset = round_idx * bytes_per_round
                src_addrs = [host_gva + base_offset + i * BLOCK_SIZE for i in range(batch_size)]
                dst_addrs = [dst.data_ptr()] * batch_size
                sizes = [BLOCK_SIZE] * batch_size

                ret = handle.copy_data_batch(
                    src_addrs, dst_addrs, sizes, batch_size,
                    bm.BmCopyType.G2H, 0
                )
                assert ret == 0, f"G2H failed at batch={batch_size}, ret={ret}"
            g2h_time = time.time() - g2h_start

            assert torch.equal(dst, src), "Data mismatch!"

            h2g_latency_ms = int(h2g_time * 1000)
            g2h_latency_ms = int(g2h_time * 1000)
            h2g_throughput_gb = TOTAL_BYTES / (h2g_time * 1024 * 1024 * 1024)
            g2h_throughput_gb = TOTAL_BYTES / (g2h_time * 1024 * 1024 * 1024)

            print(f"Step3: copy_data_batch H2G/G2H SUCCESS")
            print(f"Batch size: {batch_size}, Total size: {TOTAL_BYTES / (1024**3):.2f} GB")
            print(f"  H2G: {h2g_latency_ms}ms, Throughput: {h2g_throughput_gb:.2f} GB/s")
            print(f"  G2H: {g2h_latency_ms}ms, Throughput: {g2h_throughput_gb:.2f} GB/s")

        assert handle.leave() == 0, "leave failed"
        assert mf.get_last_err_msg() == "", mf.get_last_err_msg()
        handle.destroy()
        print("Step4: BM destroy SUCCESS")

    finally:
        if bm_inited:
            bm.uninitialize(DEVICE_ID)
        mf.uninitialize()

    print("03_device_sdma ok")


if __name__ == "__main__":
    main()