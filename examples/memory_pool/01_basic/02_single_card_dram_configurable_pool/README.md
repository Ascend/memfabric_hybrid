# 02_single_card_dram_configurable_pool

## 场景
在样例 01 基础上增加复杂度：同样是单机单卡 DRAM 池，但容量可调（本地容量与上限分离）。

## 目标
验证不同容量档位下内存池创建与访问行为一致。

## 使用能力
- 同 [01_single_card_dram_pool](../01_single_card_dram_pool/README.md)

## 规模建议
- world_size=1
- 容量档位：512MB / 2GB / 8GB，max_dram_size固定为64GB
- 每档执行同一组读写校验

## 必要条件
机器可用 DRAM 余量需覆盖最大档位（含进程额外开销），并保证参数合法（max_dram_size >= local_dram_size）。

## 验收标准
- 三档容量均可成功完成创建与读写校验
- 相同负载下功能结果一致
- 容量切换不引入错误码
