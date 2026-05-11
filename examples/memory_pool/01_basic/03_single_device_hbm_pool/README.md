# 03_single_device_hbm_pool

## 场景
在样例 01 的流程不变前提下，将池类型从 DRAM 切换为 HBM。

## 目标
验证内存池的 HBM 池化能力可用，并建立 DRAM/HBM 两条基础基线。

## 使用能力
- 同 [01_single_device_dram_pool](../01_single_device_dram_pool/README.md)

## 规模建议
- world_size=1
- local_hbm_size=1GB（可按设备余量调整）
- 数据块：4KB、64KB、1MB

## 必要条件
设备需具备可用 HBM，且运行时/驱动可正确识别并分配对应设备内存。

## 验收标准
- HBM 池创建与生命周期操作成功
- 读写后数据一致
- 保持稳定无异常退出，不引入错误码
