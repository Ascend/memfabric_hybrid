# 04_no_xpu_host_rdma_dram_pool

## 场景
无卡环境（XPU_TYPE=NONE）下，使用 HOST_RDMA 协议创建 DRAM 池，完成最小读写闭环校验。

## 目标
验证在无卡部署形态下，DRAM 池能力可正常初始化、创建、访问与释放。

## 使用能力
- 同 [01_single_device_dram_pool](../01_single_device_dram_pool/README.md)

## 协议与配置要点
- `create2` 的 data_op_type 选择 HOST_RDMA 协议。
TODO: check下面的条件。。。
- 运行前按环境变量文档设置 HOST_RDMA 相关参数（如 `HCOM_MAX_SLICE_SIZE`、`HCOM_RECV_DATA_SIZE`、`HYBM_RDMA_SWAP_SPACE_SIZE`）。
- config store 可使用 `tcp://ip:port` 或 `etcd://ip:port`。

## 规模建议
- world_size=1
- local_dram_size=1GB, max_dram_size=1GB
- 数据块：4KB、64KB、1MB

## 必要条件
- 运行环境为无卡模式（XPU_TYPE=NONE），且 HOST_RDMA 依赖组件安装完整。
- 进程可访问 config store，并具备稳定网络连通性。

## 验收标准
- 创建与生命周期操作成功。
- 写入后读回数据一致。
- 无错误码残留（`get_last_err_msg` 为空）。
