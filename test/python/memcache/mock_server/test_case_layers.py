#!/usr/bin/python3.11
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

from test_case_common import TestClient


if __name__ == "__main__":
    client = TestClient("127.0.0.1", 5432)

    client.init_mmc()

    res = client.put_from_layers(
        "key",
        [1024] * 10,
        0
    )
    print(res)

    res = client.get_into_layers(
        "key",
        [1024] * 10,
        0
    )
    print(res)

    res = client.batch_put_from_layers(
        ["key-0", "key-1"],
        [[1024 for _ in range(10)] for _ in range(2)],
        0
    )
    print(res)

    res = client.batch_get_into_layers(
        ["key-0", "key-1"],
        [[1024 for _ in range(10)] for _ in range(2)],
        0
    )
    print(res)