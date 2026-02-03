# NDR

# Build & Install

需要rdma_core三方库，解压缩到3rdparty目录下。
rdma_core: https://github.com/linux-rdma/rdma-core

编译前需要配置Ascend环境变量，命令默认为：
`source /usr/local/Ascend/ascend-toolkit/set_env.sh`

编译命令
1. `cd npu_direct_rdma`
2. `bash scripts/build.sh` (需要root权限)


# perftest
使用perftest测试rdma相关性能
## build
1. `cd ndr/perftest`
2. `make`
## test
与perftest原生测试方式相同，调用对应的测试例

增加参数：
1. `--using_npu_server device_idx`	在 server 使用 device_idx 号 npu 进行接下来的通信，不配置默认使用 cpu
2. `--using_npu_client device_idx`	在 client 使用 device_idx 号 npu 进行接下来的通信，不配置默认使用 cpu