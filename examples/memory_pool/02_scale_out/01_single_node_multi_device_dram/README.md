# 01_single_node_multi_device_dram

## 场景
单机多卡组建 DRAM 大池，验证 rank 间地址可见与跨 rank 数据访问。

## 目标
从单卡扩展到多卡，确认同机 scale-out 的功能正确性。

## 使用能力
- `initialize(store_url, world_size=2, device_id, config)`
- `create2(id, local_dram_size, max_dram_size, local_hbm_size=0, max_hbm_size=0, ...)`
- `BigMemory.join()`
- `BigMemory.peer_rank_ptr(peer_rank, mem_type)`（本 rank）
- `BigMemory.copy_data(src_ptr, dst_ptr, size, type, flags)`（H2G/G2H）
- `BigMemory.leave()` / `BigMemory.destroy()`

## 规模建议
- world_size=2
- 每 rank local_dram_size=1GB
- 数据大小 4MB

## 必要条件
单机至少 2 张可用NPU。

## 验收标准
- rank 间互相拷贝的数据可达且一致
- 多卡场景流程无异常错误码

## 运行

```bash
python3 01_single_node_multi_device_dram.py
```


## Q&A
