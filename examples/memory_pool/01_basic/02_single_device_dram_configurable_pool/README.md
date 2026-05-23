# 02_single_device_dram_configurable_pool

## 场景
在样例 01 基础上增加复杂度：同样是单机单卡 DRAM 池，使用 `create2` 接口设置本地 DRAM 大小与上限。

## 目标
验证使用 `create2` 配置 DRAM 池容量的可行性。

## 使用能力
- base: 同 [01_single_device_dram_pool](../01_single_device_dram_pool/README.md)
- `create2` 中传入 `local_dram_size` 和 `max_dram_size`

## 规模建议
- world_size=1
- local_dram_size=2GB, max_dram_size=2GB
- 数据块大小：4KB / 64KB / 1MB

## 必要条件
机器可用 DRAM 余量至少 2GB。

## 验收标准
- 2GB DRAM 池创建成功
- 4KB / 64KB / 1MB 三组数据均完成 H2G 和 G2H 读写校验
