# 02_multi_node_multi_card_dram

## 场景
跨节点部署内存池，形成多机多卡 DRAM 池并执行跨机数据访问。

## 目标
验证跨节点条件下内存池的连通性、一致性与基础稳定性。

## 使用能力（仅内存池）
- `bm.initialize(store_url, world_size, device_id, config)`（跨节点）
- `bm.create2(...)` + `join`
- `peer_rank_ptr`
- 同步 `copy_data` / `copy_data_batch`

## 规模建议
- 2 节点 × 2 卡（world_size=4）
- 每 rank local_dram_size=1~2GB
- 块大小：4KB~16MB

## 必要条件
节点间网络互通，所有 rank 均可稳定访问同一个 config store，且机器时间同步偏差可控。

## 验收标准
- 跨节点 copy 正确率 100%
- 长时间运行无 rank 掉队
- 故障日志可定位到具体 rank/节点
