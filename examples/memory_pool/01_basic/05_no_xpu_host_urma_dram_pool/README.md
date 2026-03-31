# 05_no_xpu_host_urma_dram_pool

## 场景
无卡环境（XPU_TYPE=NONE）下，使用 HOST_URMA 协议创建 DRAM 池，完成最小读写闭环校验。

## 对比说明（仅协议差异，其他复用）
与 [04_no_xpu_host_rdma_dram_pool](../04_no_xpu_host_rdma_dram_pool/README.md) 相比，唯一差异是协议选择：04 在 `create2` 中使用 HOST_RDMA，05 在 `create2` 中使用 HOST_URMA。除此之外，初始化流程、DRAM 容量参数、拷贝路径、规模建议、必要条件与验收标准全部复用 04。

请自行修改 04 的脚本。
