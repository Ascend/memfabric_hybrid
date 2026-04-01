# 01_enable_unified_address_space

## 场景
开启内存池 unified_address_space 特性，对比开启前后的地址访问行为与使用方式。

## 目标
给出一个“特性开关类”最小样例模板，后续可复用到其他特性。

## 使用能力
- `BmConfig.unified_address_space`（开关）
- `bm.initialize` / `create2` / `join`
- `peer_rank_ptr` 与 `gva_to_va`
- 同步 `copy_data`

## 规模建议
- world_size=2
- 每 rank local_dram_size=1GB
- 开关前后执行同一组访问用例

## 必要条件
当前版本与运行环境必须支持 unified_address_space，且所有 rank 使用一致配置。

## 验收标准
- 开关前后功能结果一致（数据正确）
- 地址语义差异可解释、可复现
- 文档明确给出启用场景与限制
