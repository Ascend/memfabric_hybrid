#!/usr/bin/python3.11
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

from test_case_common import TestClient


if __name__ == "__main__":
    client = TestClient("61.47.1.122", 5004)

    client.init_mmc()
    
    key_num = 8
    block_num = 16
    media = 1 # 0 cpu | 1 npu
    start = 15
    end = start + 2
    
    for k in range(start, end):
        keys = []
        for j in range(key_num):
            keys.append('key-' + str(k) + '-' + str(j))

        res1 = client.batch_put_from_layers(
            keys,
            [[1024 * 16 if i % 2 == 0 else 1024 * 128 for i in range(block_num)] for _ in range(key_num)],
            media
        )

        res2 = client.batch_get_into_layers(
            keys,
            [[1024 * 16 if i % 2 == 0 else 1024 * 128 for i in range(block_num)] for _ in range(key_num)],
            media
        )

        print(f"{res1 == res2}")

        keys1 = [f"key2_{i:03d}_{k}" for i in range(key_num)]
        sizes1 = [1024] * key_num
        res4 = client.batch_put_from(keys1, sizes1, media)
        res5 = client.batch_get_into(keys1, sizes1, media)
        print(f"{res4 == res5}")


