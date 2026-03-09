# 环境变量说明

本文档列出 MemFabric_Hybrid 使用的所有环境变量及其用途。

## 运行时环境变量

以下环境变量在程序运行时读取，用于控制运行时行为：

| 环境变量名 | 默认值 | 说明 |
|-----------|-------|------|
| `ASCEND_HOME_PATH` | 无（必填） | NPU环境下Ascend安装路径，用于加载CANN动态库。GPU环境无需设置。 |
| `CUDA_HOME` | 无（必填） | GPU环境下CUDA安装路径，用于加载CUDA动态库。NPU环境无需设置。 |
| `ASCEND_RT_VISIBLE_DEVICES` | 无 | 设备可见性控制，用于将物理设备ID映射为逻辑设备ID。格式如`0,1,2,3`。 |
| `HCOM_MAX_SLICE_SIZE` | NPU: 1GB<br>其他: 1MB | HCOM传输最大切片大小（字节），控制单次传输数据分片上限。适用于HOST_RDMA/HOST_TCP/HOST_URMA传输模式。 |
| `HCOM_RECV_DATA_SIZE` | NPU: 1MB+1KB<br>其他: 1MB+1024 | HCOM接收数据缓冲区大小（字节）。建议设置为`HCOM_MAX_SLICE_SIZE + 1024`。 |
| `HYBM_RDMA_SWAP_SPACE_SIZE` | NPU: 4GB<br>其他: 1GB | RDMA交换空间大小（字节），用于Host RDMA数据传输的中转内存。 |
| `MEMFABRIC_HYBRID_EXTEND_LIB_PATH` | 无 | 扩展库路径，用于加载自定义的`libmf_hybm_copy_extend.so`库。 |
| `SHMEM_LOG_LEVEL` | 无 | Transfer模块日志级别，取值范围0-4（0:DEBUG, 1:INFO, 2:WARN, 3:ERROR, 4:OFF）。**仅Python接口下生效，bm/shm场景不生效。** |
| `ASCEND_MF_LOG_LEVEL` | 无 | MemFabric日志级别，优先级高于`SHMEM_LOG_LEVEL`。取值范围0-4。**仅Python接口下生效，bm/shm场景不生效。** |
| `ASCEND_MF_STORE_URL` | 无（必填） | MemFabric Store URL，用于Transfer Engine初始化时连接配置存储。格式如`tcp://ip:port`。 |
| `ACCLINK_CHECK_PERIOD_HOURS` | 24 | 控制路径SSL证书定期检查周期（小时），有效范围1-168。 |
| `ACCLINK_CERT_CHECK_AHEAD_DAYS` | 30 | 证书提前检查天数，在证书过期前多少天开始告警，有效范围1-365。 |

## 构建时环境变量

以下环境变量在编译构建时读取，用于控制构建行为：

| 环境变量名 | 默认值 | 说明 |
|-----------|-------|------|
| `MEMFABRIC_VERSION` | 无（必填） | 构建版本号，用于生成whl包版本。**仅构建Python whl包时需要，C++构建不需要。** |
| `BUILD_OPEN_ABI` | OFF | 是否使用开放ABI（`_GLIBCXX_USE_CXX11_ABI`）。ON表示使用C++11 ABI。 |
| `BUILD_MODE` | RELEASE | 构建模式。可选值：RELEASE、DEBUG、ASAN。 |
| `ENABLE_PTRACER` | ON | 是否启用ptracer性能打点工具。 |
| `XPU_TYPE` | NPU | 异构设备类型。可选值：NPU、GPU、NONE。 |

## 安装脚本环境变量

| 环境变量名 | 说明 |
|-----------|------|
| `ASCEND_TOOLKIT_HOME` | Ascend Toolkit安装路径，安装脚本检查用。 |

## 使用示例

### NPU环境运行前设置

```bash
export ASCEND_HOME_PATH=/usr/local/Ascend/ascend-toolkit
export ASCEND_MF_LOG_LEVEL=1
export HCOM_MAX_SLICE_SIZE=$((128*1024))
export HCOM_RECV_DATA_SIZE=$((128*1024+128))
export HYBM_RDMA_SWAP_SPACE_SIZE=$((128*1024*1024))
```

### GPU环境运行前设置

```bash
export CUDA_HOME=/usr/local/cuda
export ASCEND_MF_LOG_LEVEL=1
```

### 无卡环境（NONE）运行前设置

```bash
export ASCEND_MF_LOG_LEVEL=1
export HCOM_MAX_SLICE_SIZE=$((128*1024))
export HCOM_RECV_DATA_SIZE=$((128*1024+128))
export HYBM_RDMA_SWAP_SPACE_SIZE=$((128*1024*1024))
```
