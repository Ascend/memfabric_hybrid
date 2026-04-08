# 02_extend_local_mem

## 场景
单机多卡最小闭环：通过 create2 创建 DEVICE_RDMA 的一定容量的 DRAM 池，完成一次写入与读回校验，扩容后再次写入读回校验

## 目标
验证内存池基础生命周期与最小数据通路是否工作正常。

## 使用能力
- `initialize(store_url, world_size, device_id, config)`
- `create2(id, local_dram_size, max_dram_size, local_hbm_size=0, max_hbm_size=0, ...)`
- `extend_local_mem(...)`
- `BigMemory.join()`
- `BigMemory.peer_rank_ptr(peer_rank, mem_type)`（本 rank）
- `BigMemory.copy_data(...)`（H2G / G2H）
- `BigMemory.leave()` / `BigMemory.destroy()`

## 规模建议
- world_size=8
- local_dram_size=1GB, max_dram_size=8GB
- 数据块：1KB

## 必要条件
单机至少 1 张可用NPU，且进程可访问 config store( tcp://ip:port ）

## 验收标准
- 初始化、创建、join、copy、leave、destroy 全流程返回成功
- 写入后读回数据一致
- 无错误码残留（`get_last_err_msg` 为空）

## 运行（Python）

依赖：已安装 **`memfabric_hybrid`**。

```bash
cd examples/memory_pool/04_feature/02_extend_local_mem
python3 02_extend_local_mem.py
```

成功时打印 `alloc extend memory test ok`。

## Q&A
