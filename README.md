# MemFabric

## 🔄Latest News

- [2025/11] MemFabric项目于2025年11月30日开源，在昇腾芯片上提供高效的多链路D2RH,RH2D,RH2H,D2D,D2H,H2D等单边通信能力。

## 🎉概述

  MemFabric是一款开源内存池化软件，面向昇腾NPU服务器和超节点，其设计目的是:
  - 将多节点的异构设备内存(DRAM | HBM等)池化且提供高性能的全局内存直接访问的能力
  - 北向提供简单的内存语义访问接口(xcopy with global virtual address)
  - 南向屏蔽多种DMA引擎及多种总线/网络 (Device UB、Device RoCE、Host UB、Host RoCE等)
![architecture](./doc/source/architecture.png)
  可以通过简单的集成MemFabric，快速支撑大模型KV缓存、强化训练参数Reshard、生成式推荐缓存、模型参数缓存等多种业务场景。


## 🧩核心特性

- **全局统一编址内存池**
MemFabric通过构建逻辑上的全局内存语义统一编址，对分布在不同层级、不同节点的内存单元进行统一管理与使用，使系统能够像管理单一物理资源一样，对跨 CPU、NPU的内存资源进行统一寻址和透明访问，核心目的是实现内存资源的整合与统一调度，从而显著提升 AI 超节点的算力释放效率。
全局内存统一地址(Global Virual Address)的特点：
    - 它是一个简单的uint64
    - 所有进程的gva的起始地址一致
    - 所有进程的gva的按线性排布且一致
![architecture](./doc/source/unified_global_address.png)

- **跨机跨介质单边访问**
基于MemFabric内存语义统一编址，数据可以在跨节点的多级存储间实现透明访问和直接传输，典型跨节点跨介质的访问路径有：
    - D2RH：本机HBM到跨机DRAM
    - RH2D：跨机DRAM到本机HBM
    - RH2H：跨机DRAM到本机DRAM
MemFabric访问数据流和控制流如下图所示：
![architecture](./doc/source/one_copy.png)


## 🔥性能表现

### 时延测试
读写时延性能是一个衡量缓存池的重要指标，为对比测试内存语义和非内存语义，我们将MemFabric对接到MoonCake TE（MoonCake是业界开源的一款的分布式缓存软件）进行如下的测试：
- 测试环境：2台昇腾A3机器，节点1每个die对应一个写进程，节点2每个die对应一个读进程，共32个进程
- 测试步骤：
  1) 构造block模拟DeepSeek-R1模型KV大小，即：61*128K + 61*16K = 8784KB ≈ 8.57MB，共122个离散地址。
  2) 节点1所有进程调用put接口写入指定个数（8、16、32）的block，每个进程写512次，统计写总时延。
  3) 节点2所有进程调用get接口读取所有写入的block，每个进程共计读取512次，统计读总时延。
分别对内存语义（Memory标识，下同）和非内存语义（Message标识，下同）统计测试结果，绘制曲线对比如所示。

![memfabric-Latency-performance](./doc/memfabric-Latency-performance.JPG)

### 单DIE带宽测试
- 性能评估指标：单die跨机传输带宽。
- 测试环境：2台昇腾A3机器
- 测试步骤：
  1) 构造单次拷贝数据大小1G/2G，连续内存
  2) 进行RH2LD测试，循环调用RH2LD拷贝1000次，取时延平均值
  3) 进行LD2RH测试，循环调用LD2RH拷贝1000次，取时延平均值
  4) 进行LD2RD测试，循环调用LD2RD拷贝1000次，取时延平均值
  5) 进行RD2LD测试，循环调用RD2LD拷贝1000次，取时延平均值
![memfabric-Bandwidth-performance](./doc/memfabric-Bandwidth-performance.JPG)

### PrefixCache吞吐测试
在大模型推理中，尤其是需要长上下文或高并发处理请求时，KVCache的高效复用与调度成为关键，此类缓存需要在HBM、DRAM乃至 SSD之间频繁迁移，若无良好的池化支持，将导致显存拥堵和请求阻塞，因此，此场景是内存池化软件重要的应用场景。MemFabric +MemCache（类似MoonCake Store的分布式缓存软件，此文不赘述）联合国内某头部互联网AI部门，基于vLLM推理框架在昇腾A3超节点进行Prefill吞吐QPS测试，测试系统示意图如下图所示。

![Prefill-QPS](./doc/Prefill-QPS.JPG)

- 性能评估指标：2P1D（D是2机）共4台昇腾A3机器，观察P节点归一化QPS的提升，测试模型为DeepSeek-R1
-  PrefixCache KVPool配置：每台机器贡献40GB*16die共640GB的DRAM内存，组成4机共2.5TB的KVPool，存储最高水位85%，超过最高水位后淘汰5%
-  benchmark 配置：请求输入长度为4K，输出token为1；一共400组不同的前缀，每组发送25个请求，共约1w条请求；
-  命中率构造：每组请求通过相同前缀长度来构造命中率，在发送请求时，先发送每组的第一个请求（400个），再发送每组的第二个请求（400个）以此类推；在发送每组第二个请求时，保障了其第一个请求不会被换出。
- 分别对无KVPool（Baseline标识），内存语义KVPool（Memory标识）和非内存语义KVPool（Message标识）进行测试，绘制QPS吞吐对比曲线如图。
![memfabric-Throughput-performance](./doc/memfabric-Throughput-performance.JPG)

## 🔍目录结构

```
├── LICENSE                                 # LICENSE
├── .clang-format                           # 格式化配置
├── .gitmodules                             # git配置
├── .gitignore                              # git忽视文件
├── CMakeLists.txt                          # 项目的CMakeList
├── doc                                     # 文档目录
├── example                                 # 样例
│  ├── bm                                   # big memory样例
│  └── shm                                  # share memory样例
│  └── trans                                # 数据传输功能样例
│  └── decrypt                              # 自定义解密库示例
├── script                                  # 构建脚本
│  ├── build_and_pack_run.sh                # 编译+加包脚本
│  ├── build.sh                             # 编译脚本
│  ├── run_ut.sh                            # 编译+运行ut脚本
├── test                                    # test目录
│  ├── certs                                # 证书生成脚本
│  ├── python                               # python测试用例
│  └── ut                                   # 单元测试用例
├── src                                     # 源码
│  ├── acc_links                            # 内部通信层
│  ├── driver                               # 驱动层
│  └── hybm                                 # 存储管理层
│  └── mooncake_adapter                     # 对接mooncake
│  └── smem                                 # share memory+big memory接口实现
│  └── util                                 # 公共函数
├── README.md
```


## 🚀快速入门

请访问以下文档获取简易教程。

- [构建](./doc/build.md)：介绍组件编译和安装教程。

- [样例执行](./example/examples.md)：介绍如何端到端执行样例代码，包括C++和Python样例。

## 📑学习教程

- [C接口](./doc/API.md)：C接口介绍以及C接口对应的API列表

- [python接口](./doc/pythonAPI.md)：python接口介绍以及python接口对应的API列表


## 📦软件硬件配套说明
- 硬件型号支持
  - Atlas 800I A2/A3 系列产品
  - Atlas 800T A2/A3 系列产品
- 平台：aarch64/x86
- 配套软件：驱动固件 Ascend HDK 25.0.RC1、 CANN 8.1.RC1及之后版本
- cmake >= 3.19  
- GLIBC >= 2.28


## 📝相关信息

- [安全声明](./doc/SECURITYNOTE.md)

- [许可证](./LICENSE)