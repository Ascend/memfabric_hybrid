# 02_multi_node_multi_device_dram

## 场景
跨节点部署内存池，形成多机多卡 DRAM 池并执行跨机数据访问。

## 目标
验证跨节点条件下内存池的连通性、一致性与基础稳定性。

## 使用能力
- 同 [01_single_node_multi_device_dram](../01_single_node_multi_device_dram/README.md)

## 规模建议
- 2 节点 × 1 卡（world_size=2，总 2 卡）
- 每 rank local_dram_size=1GB
- 数据大小 4MB

## 必要条件
- 每节点至少 1 张 NPU；两机互通且能连 **同一 config store**

## 运行

将 `192.168.1.10` 换成首节点（config store / rank 0）IP，两机相同：

```bash
# 1) 节点 A — 先执行
python3 02_multi_node_multi_device_dram.py 0 192.168.1.10
```

```bash
# 2) 节点 B — 建议在 rank 0 已 H2G 并进入 sleep 后再起 rank 1（rank 1 join 后仍会 sleep 再读，降低竞态）
python3 02_multi_node_multi_device_dram.py 1 192.168.1.10
```

## 验收标准
- rank 1 侧 G2H 与 `arange` 预期一致，进程正常退出
- rank 0 在 Ctrl+C 后能清理退出
