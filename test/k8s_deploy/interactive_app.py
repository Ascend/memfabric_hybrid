# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

"""Brief description of the module.
   Interactive calling of memcache
   Available commands: put, get, remove, quit
"""

import faulthandler

from memcache import DistributedObjectStore
from memcache import ReplicateConfig

faulthandler.enable()  # 崩溃时自动打印 Python 堆栈

STORE = DistributedObjectStore()
DEFAULT_REPLICA_NUMBER = 2
AVAILABLE_COMMANDS = "'put <key> <value>', 'get <key>', 'remove <key>' or 'quit'/'exit'"


def set_up():
    res = STORE.init(0)
    if res != 0:
        raise ValueError(f"init failed, res={res}")
    print("object store set_up succeeded.")


def put(key: str, value: bytes):
    print(f"Executing put operation: key={key}, value={value}")
    config = ReplicateConfig()
    config.replicaNum = DEFAULT_REPLICA_NUMBER
    res = STORE.put(key, value, config)
    if res != 0:
        raise ValueError(f"put failed, res={res}")
    print(f"Put operation succeeded")


def get(key: str):
    print(f"Executing get operation: key={key}")
    retrieved_data = STORE.get(key)
    print(f"Get result: {retrieved_data}")


def remove(key: str):
    print(f"Executing remove operation: key={key}")
    res = STORE.remove(key)
    if res != 0:
        raise ValueError(f"remove failed, res={res}")
    print(f"Remove operation succeeded")


def stop():
    print("Performing cleanup (stop)...")
    STORE.close()
    print("object store destroyed")


def main():
    print("Welcome to the interactive command tool")
    print(f"Available commands: {AVAILABLE_COMMANDS}")
    set_up()
    while True:
        try:
            user_input = input("Enter command: ").strip()
            if not user_input:
                print(f"Invalid command. Please enter {AVAILABLE_COMMANDS}")
                continue

            parts = user_input.split()
            command = parts[0].lower()

            if command == 'put':
                if len(parts) != 3:
                    print("Usage: put <key> <value>")
                else:
                    put(parts[1], bytes(parts[2], "utf-8"))
            elif command == 'get':
                if len(parts) != 2:
                    print("Usage: get <key>")
                else:
                    get(parts[1])
            elif command == 'remove':
                if len(parts) != 2:
                    print("Usage: remove <key>")
                else:
                    remove(parts[1])
            elif command in ('quit', 'exit'):
                print("Preparing to exit...")
                break
            else:
                print(f"Invalid command. Please enter {AVAILABLE_COMMANDS}")

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