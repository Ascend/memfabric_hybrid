## 简介

本项目提供了C++和Python的调用样例，请开发者根据实际情况参考对应实现。

## 目录说明

```
├── examples                       
│   ├── bm              # big memory样例
│   ├── shm             # share memory样例
│   ├── trans           # trans 样例
```

## 开发样例

| **样例名称**                                       | **样例介绍**       | **开发语言** |
|------------------------------------------------|----------------|----------|
| [BmCpp](./bm/BmCpp/README.md)                  | BM C++样例       | C++      |
| [BmPython](./bm/BmPython/README.md)            | bm Python样例    | Python   |
| [BmBenchmark](./bm/BmBenchmark/README.md)      | bm Benchmark样例 | C++      |
| [shm_AllReduce](./shm/AllReduce/README.md)     | shm C++样例      | C++      |
| [shm_RDMADemo](./shm/RDMADemo/README.md)       | shm C++样例      | C++      |
| [shm_ShiftPutGet](./shm/ShiftPutGet/README.md) | shm C++样例      | C++      |
| [trans](./trans/perf/README.md)                | trans C++样例    | C++      |

## 常见问题

| 现象                                 | 介绍                                    |
|------------------------------------|---------------------------------------|
| 创建BM失败  ❗                     | [link](../doc/faq/bm_create_fail.md)      |