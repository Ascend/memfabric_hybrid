# 02_multi_node_multi_card_dram

## 场景
跨节点部署内存池，形成多机多卡 DRAM 池并执行跨机数据访问。

## 目标
验证跨节点条件下内存池的连通性、一致性与基础稳定性。

## 使用能力
- 同 [01_single_node_multi_card_dram](../01_single_node_multi_card_dram/README.md)

## 规模建议
- 2 节点 × 1 卡（world_size=2，总 2 卡）
- 每 rank local_dram_size=1GB
- 数据大小 4MB

## 必要条件
单机至少 2 张可用NPU，节点间网络互通，所有 rank 均可稳定访问同一个 config store。

## 验收标准
- 跨机 rank 间互相拷贝的数据可达且一致
- 多卡场景流程无异常错误码
