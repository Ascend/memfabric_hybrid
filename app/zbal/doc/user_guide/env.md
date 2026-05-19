# ZBAL Environment Variables

## Core Runtime

| 名称 | 含义 | 取值范围 |
|------|------|----------|
| `ZBAL_OP_DEFUALT_STREAM` | AIV通信算子是否使用默认流 | `0`（关闭）/ `1`（开启），默认 `0` |
| `ZBAL_HCCL_OP` | 回退到HCCL的通信算子列表，逗号分隔 | 字符串，如 `"allgather,allreduce,broadcast"`，默认空 |
| `ZBAL_ENABLE_GRAPH` | 是否启用NPU计算图支持 | `0`（关闭）/ `1`（开启），默认 `0` |
| `MEMFABRIC_HYBRID_LIBRARY_PATH` | MemFabric混合库的动态库路径 | 字符串（路径），默认自动从Python包查找 |
| `ASCEND_HOME_PATH` | Ascend CANN软件包安装目录 | 字符串（路径），默认 `/usr/local/Ascend/ascend-toolkit/latest` |
| `MASTER_ADDR` | 分布式主节点IP地址（用于bootstrap） | 字符串（IP地址），默认 `127.0.0.1` |
| `MASTER_PORT` | 分布式主节点端口（用于bootstrap，实际端口=MASTER_PORT-20） | 整数，默认 `6789` |

## Profiling / Tracing

| 名称 | 含义 | 取值范围 |
|------|------|----------|
| `ZBAL_PROF_ENABLE` | 是否启用profiling trace采集 | `0`（关闭）/ `1`（开启），默认 `0` |
| `ZBAL_PROF_MAX_TRACING_COUNT` | 每个核最大trace记录数 | 整数，范围 `[20480, 204800)`，默认 `20480`，超出范围自动重置为默认值 |
| `ZBAL_PROF_DIR` | Profiling trace输出文件目录 | 字符串（路径），默认 `/home/` |

## MoE / DeepEP

| 名称 | 含义 | 取值范围 |
|------|------|----------|
| `DEEPEP_BALANCE_FACTOR_HIGH` | 负载均衡高阈值系数 | 浮点数，必须 `> 1.1`，默认 `1.2` |
| `DEEPEP_BALANCE_FACTOR_LOW` | 负载均衡低阈值系数 | 浮点数，必须 `> 0.9`，默认 `1.0` |
| `DEEPEP_ENABLE_REBALANCE` | 是否启用dispatch动态重均衡 | `0`（关闭）/ 非0（开启），默认 `0` |
| `MOE_EXPERT_TOKEN_NUMS_TYPE` | 专家接收token数的输出格式 | `0`（前缀和）/ `1`（每个专家的token数），默认 `1` |
| `MOE_SHARED_EXPERT_RANK_NUM` | 共享专家所在的rank数量 | 整数 `≥ 0`，默认 `0` |
| `MOE_ENABLE_TOPK_NEG_ONE` | 是否启用topk=-1（丢弃token功能） | `0`（关闭）/ `1`（开启），默认 `0` |
| `DEEP_NORMAL_MODE_USE_INT8_QUANT` | MoE dispatch是否使用INT8量化 | `"1"` 启用，其他值不启用，默认关闭 |

## Memory / Allocator

### PYTORCH_NPU_ALLOC_CONF

格式：`key1:value1,key2:value2,...`

| Token | 含义 | 取值范围 |
|-------|------|----------|
| `max_split_size_mb` | 最大可split的block大小（MB） | 整数，必须 `> 1` MB，默认无限制 |
| `garbage_collection_threshold` | GC触发阈值（已分配内存占比） | 浮点数 `(0.0, 1.0)`，默认 `0`（不触发GC） |
| `expandable_segments` | 是否启用可扩展内存段 | `True` / `False`，默认 `False` |
| `base_addr_aligned_kb` | 大block基地址对齐（KB） | 整数（KB），默认 `128` |

注意：`expandable_segments` 与 `max_split_size_mb` / `garbage_collection_threshold` 互斥，不能同时启用。

### ZBAL_NPU_ALLOC_CONF

格式：`key1:value1,key2:value2,...`

| Token | 含义 | 取值范围 |
|-------|------|----------|
| `max_split_size_mb` | 最大可split的block大小（MB） | 整数，必须 `> 1` MB |
| `garbage_collection_threshold` | GC触发阈值 | 浮点数 `(0.0, 1.0)`，默认 `0` |
| `segment_size_mb` | 内存段大小（MB） | 整数（MB），默认 `2` |
| `use_sma_allocator` | 是否使用SMA分配器 | `True` / `False`，默认 `True` |
| `use_vmm_for_static_memory` | 静态内存（权重/KV cache）是否使用VMM模式 | `True` / `False`，默认 `False` |
| `small_heap_size` | 小堆内存大小 | 整数（字节），默认 `2097152`（2MB） |
| `small_heap_threshold` | 小堆分配阈值 | 整数（字节），默认 `524288`（512KB） |

## Build

| 名称 | 含义 | 取值范围 |
|------|------|----------|
| `ASCEND_TOOLKIT_HOME` | Ascend ToolKit安装目录（用于kernel编译） | 字符串（路径），默认 `/usr/local/Ascend/ascend-toolkit/latest` |
| `SHMEM_HOME_PATH` | 共享内存固件路径（kernel编译include源） | 字符串（路径），无默认值 |
| `DEBUG_MODE` | 是否以Debug模式编译（`CMAKE_BUILD_TYPE=Debug`） | `ON`/`OFF`/`1`/`YES`/`TRUE`/`Y`，默认 `FALSE` |
| `IS_MANYLINUX` | 是否编译manylinux wheel包 | `ON`/`OFF`/`1`/`YES`/`TRUE`/`Y`，默认 `FALSE` |
| `ENABLE_ZBAL_UT` | 是否编译单元测试 | `ON`/`OFF`/`1`/`YES`/`TRUE`/`Y`，默认 `OFF` |
| `ASCEND_INCLUDE_DIR` | Ascend头文件目录（覆盖默认路径） | 字符串（路径），默认 `${ASCEND_HOME_PATH}/aarch64-linux/include` |

## Test-Only

以下环境变量仅在测试脚本中使用：

| 名称 | 含义 | 取值范围 |
|------|------|----------|
| `ENABLE_DEBUG_LOG` | 单元测试启用DEBUG级别日志 | 任意非空值即启用，默认关闭 |
| `RANK` | 全局rank编号 | 整数 `≥ 0` |
| `LOCAL_RANK` | 本地rank编号 | 整数 `≥ 0` |
| `WORLD_SIZE` | 总rank数 | 整数 `≥ 1` |
| `TEST_TYPE` | 算子测试数据类型 | 字符串，如 `"int"`, `"fp16"`, `"bf16"`, `"int8"` 等 |
| `CURRENT_DIR` | 测试脚本当前工作目录 | 字符串（路径），默认 `.` |
| `ZBAL_AG_LIST` | AllGather测试是否走list kernel路径 | `"0"` / `"1"`，默认 `"0"` |
| `ZBAL_AG_2_DIMS` | AllGather测试是否走2D kernel路径 | `"0"` / `"1"`，默认 `"0"` |
| `CHECK_PRECISION` | 算子测试是否检查精度 | `"0"` / `"1"`，默认 `"1"` |
| `ENABLE_PROFILING` | 算子测试是否启用profiling | `"0"` / `"1"`，默认 `"0"` |
| `PROFILING_STEP` | Profiling迭代步数 | 整数，默认 `10` |
| `ZBAL_ENABLE_ALLTOALL_PERF_TEST` | AllToAll/AllToAllV性能测试模式 | `"0"` / `"1"`，默认 `"0"` |

## External Integration (SGLang)

以下环境变量在文档和集成脚本中引⽤，由上层框架（SGLang）解析，ZBAL本身不直接读取：

| 名称 | 含义 | 取值范围 |
|------|------|----------|
| `SGLANG_ZBAL_LOCAL_MEM_SIZE` | ZBAL每卡分配的总内存大小（MB） | 整数（MB），如 `58368` |
| `SGLANG_ENABLE_TP_MEMORY_INBALANCE_CHECK` | 是否启用TP内存不均衡检查 | `0` / `1`，默认 `0` |
| `SGLANG_ZBAL_BOOTSTRAP_URL` | ZBAL bootstrap地址 | 字符串，如 `"tcp://127.0.0.1:24669"` |
| `ENABLE_ZBAL` | 训练项目（如LLaMA-Factory）中启用ZBAL的标志 | `"0"` / `"1"`，默认 `"0"` |
