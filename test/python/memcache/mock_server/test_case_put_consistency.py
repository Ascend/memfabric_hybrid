#!/usr/bin/python3.11
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

import json
import hashlib
import argparse
import re
from test_case_common import TestClient

# 单机测试：执行后查看mismatch是否为0
# 双击测试：分别执行--put-only和--get-only，对比两侧的hash_value是否一致


def byte_size(size):
    """parse string argument with this type"""
    units = {"B": 1, "KB": 1024, "MB": 1024**2, "GB": 1024**3}
    size = size.upper().strip()
    pattern = r"^(\d+(?:\.\d+)?)([A-Z]+)?$"
    match = re.match(pattern, size)
    if match:
        number = match.group(1)
        unit = match.group(2)
        if unit in units:
            return int(float(number) * units[unit])
        elif not unit:
            return int(number)
        else:
            raise argparse.ArgumentTypeError(f"invalid byte size unit: {unit}")
    else:
        raise argparse.ArgumentTypeError(f"invalid byte size format: {size}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--put-only",
        action="store_true",
        help="whether to perform only the put operations, used in dual-host testing (default: %(default)s)",
    )
    parser.add_argument(
        "--get-only",
        action="store_true",
        help="whether to perform only the get operations, used in dual-host testing (default: %(default)s)",
    )
    parser.add_argument(
        "--host",
        default="127.0.0.1",
        help="specify the host for connecting to the mock server (default: %(default)s)",
    )
    parser.add_argument(
        "--port",
        default=8557,
        help="specify the port for connecting to the mock server (default: %(default)s)",
    )
    parser.add_argument(
        "--count",
        default=256,
        help="specify the number of test data items (default: %(default)s)",
    )
    parser.add_argument(
        "--size",
        type=byte_size,
        default=1 * 1024 * 1024,
        help="specify the byte size of the test data (default: %(default)s)",
    )
    parser.add_argument(
        "--get-offset",
        default=20,
        help="specify how many data items to put before performing the get operation (default: %(default)s)",
    )
    parser.add_argument(
        "--media",
        default=1,
        help="specify the device for creating the tensor, where 0 is for CPU and 1 is for NPU (default: %(default)s)",
    )
    args = parser.parse_args()

    perform_puts = not args.get_only
    perform_gets = not args.put_only

    client = TestClient(args.host, args.port)
    client.init_mmc()

    count = args.count
    size = args.size
    media = args.media
    get_offset = args.get_offset  # get item after put <get_offset> more items
    keys = []
    put_value = []
    hasher = (
        hashlib.sha256()
    )  # calculate hash over all tests, useful in dual-host testing
    failures = 0
    mismatch = 0
    for put_idx in range(count + get_offset):
        if perform_puts and put_idx < count:
            key = "test_evict_" + str(put_idx)
            print(f"[{put_idx+1}/{count}] put data: {key}")
            res = client.put_from(key, size, media)
            ret, value = json.loads(res)
            keys.append(key)
            put_value.append(value)
            hasher.update(res.encode())

        if perform_gets and put_idx >= get_offset:
            get_idx = put_idx - get_offset
            key = "test_evict_" + str(get_idx)
            print(f"[{get_idx+1}/{count}] get data: {key}")
            res = client.get_into(key, size, media)
            ret, value = json.loads(res)
            if ret != 0:
                failures += 1
            if perform_puts:
                equals = put_value[get_idx] == value
                if ret == 0 and not equals:
                    mismatch += 1
                print(
                    f"[{get_idx+1}/{count}] {equals=} {key=} put_value={put_value[get_idx]} get_value={value}"
                )
                print()
            else:
                keys.append(key)
                put_value.append(value)
                hasher.update(res.encode())

    print(f"{count=}")
    print(f"{failures=}")
    print(f"{mismatch=}")
    hash_value = hasher.hexdigest()
    print(f"{hash_value=}")
    if perform_gets:
        client.batch_remove(keys)
    client.batch_is_exit(keys)
