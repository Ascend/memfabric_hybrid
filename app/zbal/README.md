## 🔄Latest News

* Open source on May 15, 2026

## 🎉Introduction

ZBAL pronounced [zi:bəl], stands for Zero Buffer Acceleration Library. It contains a bunch of well-tuned operators
for LLM inference and training, which has two key advantages: <b>zero intermediate buffer</b> and <b>blazing fast</b>.

![architecture](./doc/images/architecture.png)

## 🧩Core Features

Two major features:

* Secondary memory allocator: which takes charge of memory allocation of GVA of low device
* A bunch of key communication operations: Dispatch/Combine Normal and Low Latency, some classic communication operations

*Hardware support matrix with Ascend:*

| Communication operations           | A3 Single Node | A3 SuperPod |
| ---------------------------------- | -------------- | ----------- |
| Dispatch Normal with Quant         | Y              | Y           |
| Dispatch Normal without Quant      | Y              | Y           |
| Combine Normal without Quant       | Y              | Y           |
| Dispatch Low Latency with Quant    | Y              | Y           |
| Dispatch Low Latency without Quant | Y              | Y           |
| Combine Low Latency without Quant  | Y              | Y           |
| AllToAll                           | Y              | Y           |
| ReduceScatter                      | Y              | Y           |
| AllGather                          | Y              | Y           |
| AllReduce                          | Y              | Y           |

## 🔥Performance

* [Details](./doc/performance/prof.md)

## 🚀Quickstart

- You can install ZBAL with command `pip install memfabric_zbal==[v1.1.0]` or with source code as doing following steps. 

1. Install the dependency memfabric_hybrid package.

    ```bash
    pip install memfabric_hybrid
    ```
    The version of memfabric_hybrid must be higher than v1.1.0.

2. Git clone the current repo and build wheel package.

    ```bash
    git clone https://gitcode.com/Ascend/memfabric_hybrid.git
    cd memfabric_hybrid/app/zbal/src/python/
    rm -rf build dist zbal.*   # optional
    python3 setup.py bdist_wheel
    ```

3. Install wheel package.

    ```bash
    cd memfabric_hybrid/app/zbal/src/python/dist
    pip uninstall memfabric_zbal -y
    pip install memfabric_zbal*
    ```

4. Run a python testcase to check installation. For more details, check the test shell script.

    ```bash
    cd memfabric_hybrid/app/zbal/test/python/operators/alltoallv/
    bash test_zbal_alltoallv.sh
    ```

## 📑How to use

* [Get Started](./doc/user_guide/get_started.md)
* [API Reference](./doc/api/api.md)

## 📦Prerequisite hardware and software

- Hardware
    - Device: Ascend 910C
    - Host: aarch64/x86

- Software:
    - CANN 9.0.0 and later
    - cmake >= 3.19
    - GLIBC >= 2.28

## 📝 Other information

- [Security Note](./doc/SECURITYNOTE.md)

- [License](./LICENSE)
