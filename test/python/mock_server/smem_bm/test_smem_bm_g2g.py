#!/usr/bin/env python3.10
#  Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.

from smem_test_common import TestClient

if __name__ == "__main__":
    client = TestClient('127.0.0.1', 33001)
    size = 4096
    ret = client.init_smem_bm()
    assert ret == 0
    node0_hbm_gva = client.get_peer_rank_gva(0, 'npu')
    node0_dram_gva = client.get_peer_rank_gva(0, 'cpu')
    hbm_gva_1 = node0_hbm_gva
    hbm_gva_2 = hbm_gva_1 + size
    dram_gva_1 = node0_dram_gva
    dram_gva_2 = dram_gva_1 + size

    local_hbm = client.alloc_local_memory(size, 'npu')
    local_dram = client.alloc_local_memory(size, 'cpu')

    print(f"hbm_gva({hex(node0_hbm_gva)}) dram_gva({hex(node0_dram_gva)}) "
          f"local_hbm({hex(local_hbm)}) local_dram({hex(local_dram)})")

    ret = client.register_memory(local_hbm, size)
    assert ret == 0

    # ============= write ================
    # LH2GH
    ret = client.bm_copy_batch([local_dram], [dram_gva_1],
                               [size], 1, 'H2G', 0)
    assert ret == 0
    gva_data_sum = client.calc_gva_sum(dram_gva_1, size)
    expect_data_sum = client.calc_local_memory_sum(local_dram)
    assert gva_data_sum == expect_data_sum
    # LH2GD
    ret = client.bm_copy_batch([local_dram], [hbm_gva_1],
                               [size], 1, 'H2G', 0)
    assert ret == 0
    gva_data_sum = client.calc_gva_sum(hbm_gva_1, size)
    expect_data_sum = client.calc_local_memory_sum(local_dram)
    assert gva_data_sum == expect_data_sum

    # LD2GH
    ret = client.bm_copy_batch([local_hbm], [dram_gva_1],
                               [size], 1, 'L2G', 0)
    assert ret == 0
    gva_data_sum = client.calc_gva_sum(dram_gva_1, size)
    expect_data_sum = client.calc_local_memory_sum(local_hbm)
    assert gva_data_sum == expect_data_sum
    # LD2GD
    ret = client.bm_copy_batch([local_hbm], [hbm_gva_1],
                               [size], 1, 'L2G', 0)
    assert ret == 0
    gva_data_sum = client.calc_gva_sum(hbm_gva_1, size)
    expect_data_sum = client.calc_local_memory_sum(local_hbm)
    assert gva_data_sum == expect_data_sum

    # ============= read ================
    # GH2LH
    ret = client.bm_copy_batch([dram_gva_1], [local_dram],
                               [size], 1, 'G2H', 0)
    assert ret == 0
    gva_data_sum = client.calc_gva_sum(dram_gva_1, size)
    expect_data_sum = client.calc_local_memory_sum(local_dram)
    assert gva_data_sum == expect_data_sum
    # GH2LD
    ret = client.bm_copy_batch([dram_gva_1], [local_hbm],
                               [size], 1, 'G2L', 0)
    assert ret == 0
    gva_data_sum = client.calc_gva_sum(dram_gva_1, size)
    expect_data_sum = client.calc_local_memory_sum(local_hbm)
    assert gva_data_sum == expect_data_sum
    # GD2LH
    ret = client.bm_copy_batch([hbm_gva_1], [local_dram],
                               [size], 1, 'G2H', 0)
    assert ret == 0
    gva_data_sum = client.calc_gva_sum(hbm_gva_1, size)
    expect_data_sum = client.calc_local_memory_sum(local_dram)
    assert gva_data_sum == expect_data_sum
    # GD2LD
    ret = client.bm_copy_batch([hbm_gva_1], [local_hbm],
                               [size], 1, 'G2L', 0)
    assert ret == 0
    gva_data_sum = client.calc_gva_sum(hbm_gva_1, size)
    expect_data_sum = client.calc_local_memory_sum(local_hbm)
    assert gva_data_sum == expect_data_sum

    # ============= g2g ================
    # GH2GH
    ret = client.bm_copy_batch([dram_gva_1], [dram_gva_2],
                               [size], 1, 'G2G', 0)
    assert ret == 0
    gva_data_sum1 = client.calc_gva_sum(dram_gva_1, size)
    gva_data_sum2 = client.calc_gva_sum(dram_gva_2, size)
    assert gva_data_sum1 == gva_data_sum2
    # GH2GD
    ret = client.bm_copy_batch([dram_gva_1], [hbm_gva_1],
                               [size], 1, 'G2G', 0)
    assert ret == 0
    gva_data_sum1 = client.calc_gva_sum(dram_gva_1, size)
    gva_data_sum2 = client.calc_gva_sum(hbm_gva_1, size)
    assert gva_data_sum1 == expect_data_sum
    # GD2GH
    ret = client.bm_copy_batch([hbm_gva_1], [dram_gva_2],
                               [size], 1, 'G2G', 0)
    assert ret == 0
    gva_data_sum1 = client.calc_gva_sum(dram_gva_1, size)
    gva_data_sum2 = client.calc_gva_sum(dram_gva_2, size)
    assert gva_data_sum1 == expect_data_sum

    # GD2GD
    ret = client.bm_copy_batch([hbm_gva_1], [hbm_gva_2],
                               [size], 1, 'G2G', 0)
    assert ret == 0
    gva_data_sum1 = client.calc_gva_sum(hbm_gva_1, size)
    gva_data_sum2 = client.calc_gva_sum(hbm_gva_2, size)
    assert gva_data_sum1 == expect_data_sum

    client.free_local_memory(local_hbm)
    client.free_local_memory(local_dram)
    client.close_smem_bm()
