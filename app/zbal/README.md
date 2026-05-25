## 🔄Latest News

* Open source on May 15, 2026

## 🎉Introduction

ZBAL (pronounced [zi:bəl]) stands for Zero Buffer Acceleration Library. It provides a collection of
highly optimized operators for LLM inference and training, with two key advantages: **zero intermediate
buffer** and **blazing fast** performance.

![architecture](./doc/images/architecture.png)

## 🧩Core Features

ZBAL provides two major capabilities:

1. **Secondary Memory Allocator** — manages GVA memory allocation on the device.

1. **Communication Operators** — a comprehensive set of collective and point-to-point operations, including
Dispatch/Combine (Normal and Low Latency variants) and classic collectives.

**Hardware support matrix (Ascend):**

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

**Step 1: Install the dependency**

Install `memfabric_hybrid` v1.1.0 or higher:

```bash
pip install memfabric_hybrid
```

**Step 2: Install ZBAL**

Option A — install the pre-built wheel:

```bash
pip install memfabric_zbal==v1.1.0
```

Option B — build from source:

```bash
git clone https://gitcode.com/Ascend/memfabric_hybrid.git
cd memfabric_hybrid/app/zbal/src/python/
rm -rf build dist zbal.*   # optional, clean previous builds
python3 setup.py bdist_wheel

cd dist
pip uninstall memfabric_zbal -y
pip install memfabric_zbal*
```

**Step 3: Verify the installation**

Run a test case to confirm everything is working:

```bash
cd memfabric_hybrid/app/zbal/test/python/operators/alltoallv/
bash test_zbal_alltoallv.sh
```

## 📑Documentation

* [Get Started](./doc/user_guide/get_started.md)
* [API Reference](./doc/api/api.md)

## 📦Requirements

**Hardware**

| Component | Specification   |
| --------- | --------------- |
| Device    | Ascend 910C     |
| Host      | aarch64 / x86   |

**Software**

| Component | Version         |
| --------- | --------------- |
| CANN      | 9.0.0 or later  |
| cmake     | >= 3.19         |
| GLIBC     | >= 2.28         |

## 📝 Additional Information

- [Security Note](./doc/SECURITYNOTE.md)
- [License](./LICENSE)
