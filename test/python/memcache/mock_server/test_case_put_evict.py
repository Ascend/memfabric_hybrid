#!/usr/bin/python3.11
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

from test_case_common import TestClient

if __name__ == "__main__":
    client = TestClient('61.47.1.122', 5004)
    client.init_mmc()

    size = 1048576
    count = 10000
    keys = []
    for i in range(count):
        key = 'test_evict_' + str(i)
        client.put_from(key, size, 0)
        keys.append(key)

        if i % 29 == 0 and i > 0:
            client.get_into(keys[i - 29], size, 0) 
        

    client.batch_is_exit(keys)

