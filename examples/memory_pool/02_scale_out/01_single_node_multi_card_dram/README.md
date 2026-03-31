# 01_single_node_multi_card_dram

## 场景
单机多卡组建 DRAM 大池，验证 rank 间地址可见与跨 rank 数据访问。

## 目标
从单卡扩展到多卡，确认同机 scale-out 的功能正确性。

## 使用能力（仅内存池）
- `bm.initialize(..., world_size>1, ...)`
- `bm.create2(...)` + `join`
- `peer_rank_ptr(peer_rank, ...)`
- 同步 `copy_data` / `copy_data_batch`
- `leave/destroy`

## 规模建议
- world_size=2~4
- 每 rank local_dram_size=1~2GB
- 批量任务：32 条，块大小 64KB / 1MB

## 必要条件
单机至少 2 张可用设备卡，且 rank 与 device_id 映射在各进程中保持一致。

## 验收标准
- 任意 rank 到任意 rank 的数据可达且一致
- 多卡场景流程无挂起、无异常错误码
- 相比单卡可观察到总吞吐提升趋势
