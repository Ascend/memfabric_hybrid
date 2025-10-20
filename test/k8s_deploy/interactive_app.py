# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

"""Brief description of the module.
   Interactive calling of memcache
   Available commands: put <number>, get, remove, quit
"""

import time
import os
import unittest
from time import sleep
import faulthandler
from memcache import DistributedObjectStore
from memcache import ReplicateConfig

faulthandler.enable()  # 崩溃时自动打印 Python 堆栈

cache = DistributedObjectStore()
key = f"key_pid333"
PUT_DATA = b"some data!"


def set_up():
    res = cache.init(0)
    if res != 0:
        raise ValueError("res must be zero")
    sleep(1)
    print("object store set_up succeeded.")


def put(duplicates: int):
    print(f"Executing put operation duplicates={duplicates}")
    config = ReplicateConfig()
    config.replicaNum = duplicates
    put_res = cache.put(key, PUT_DATA, config)
    if put_res != 0:
        raise ValueError("put_res must be zero")
    sleep(1)
    print(f"Executing put operation succeeded, duplicates={duplicates}")


def get():
    print("Executing get operation")
    retrieved_data = cache.get(key)
    print(retrieved_data)
    sleep(1)
    if retrieved_data == PUT_DATA:
        print("Executing get operation succeeded.")
    else:
        print("Executing get operation failed.")


def remove():
    print("Performing remove...")
    exist_res = cache.is_exist(key)
    if exist_res == 1:
        cache.remove(key)
    sleep(1)
    print("object store removed ${}".format(key))


def stop():
    print("Performing cleanup (stop)...")
    cache.close()
    sleep(1)
    print("object store destroyed ${}".format(key))


def main():
    print("Welcome to the interactive command tool")
    print("Available commands: put <number>, get, remove, quit")
    set_up()
    while True:
        try:
            user_input = input("Enter command: ").strip()
            if not user_input:
                print("Invalid command. Please enter 'put <number>', 'get', 'remove' or 'quit'")
                continue

            parts = user_input.split()
            command = parts[0].lower()

            if command == 'put':
                if len(parts) != 2:
                    print("Usage: put <number>")
                else:
                    try:
                        value = int(parts[1])
                        put(value)
                    except ValueError:
                        print("Error: Please provide a valid integer after 'put'")
            elif command == 'get':
                if len(parts) != 1:
                    print("Usage: get")
                else:
                    get()
            elif command == 'remove':
                if len(parts) != 1:
                    print("Usage: remove")
                else:
                    remove()
            elif command in ('quit', 'exit'):
                print("Preparing to exit...")
                break
            else:
                print("Invalid command. Please enter 'put <number>', 'get', 'remove' or 'quit'")

        except KeyboardInterrupt:
            print("\nInterrupt received (Ctrl+C). Preparing to exit...")
            break
        except EOFError:
            print("\nEnd of input detected. Preparing to exit...")
            break


if __name__ == '__main__':
    try:
        main()
    finally:
        stop()  # Ensure cleanup runs regardless of how the program exits