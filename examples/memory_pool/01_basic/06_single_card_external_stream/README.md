# 06_single_card_external_stream

## 场景
单机单卡最小闭环：通过 create2 创建 DEVICE_SDMA 的一定容量的 HBM 池，使用外部传入stream，完成一次写入与读回校验。

## 目标
验证内存池基础生命周期与最小数据通路是否工作正常。

## 使用能力
- `initialize(store_url, world_size, device_id, config)`
- `create2(id, local_dram_size, max_dram_size, local_hbm_size=0, max_hbm_size=0, ...)`
- `BigMemory.join()`
- `BigMemory.peer_rank_ptr(peer_rank, mem_type)`（本 rank）
- `BigMemory.copy_data(...)`（L2G / G2L）
- `BigMemory.leave()` / `BigMemory.destroy()`

## 规模建议
- world_size=1
- local_dram_size=1GB, max_dram_size=1GB
- 数据块：4KB、64KB、1MB

## 必要条件
单机至少 1 张可用NPU，且进程可访问 config store( tcp://ip:port ）

## 验收标准
- 初始化、创建、join、copy、leave、destroy 全流程返回成功
- 写入后读回数据一致
- 无错误码残留（`get_last_err_msg` 为空）

## 运行（Python）

依赖：已安装 **`memfabric_hybrid`**。

```bash
cd examples/memory_pool/01_basic/06_single_card_external_stream
python3 06_single_card_external_stream.py
```

成功时打印 `06_single_card_external_stream ok`。

## Q&A
