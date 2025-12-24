# 构建

## 环境准备

#### 编译工具建议版本

- OS: Ubuntu 22.04 LTS+
- cmake: 3.20.x
- gcc: 11.4+
- python 3.11.10
- pybind11 2.10.3

#### (编译选择CANN依赖时)需要NPU固件驱动和CANN包

run包只能安装到npu环境上，且依赖于NPU固件驱动和CANN包，具体版本依赖详见下面的软件硬件配套说明

请在环境上提前安装NPU固件驱动和CANN包([环境安装参考链接](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0000.html))

安装完成后需要配置CANN环境变量([参考安装Toolkit开发套件包的第三步配置环境变量](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0008.html))

## 编译

memfabric_hybrid编译不依赖CANN和HDK.

1. 下载代码
```
git clone https://gitcode.com/Ascend/memfabric_hybrid
cd memfabric_hybrid
git clean -xdf
git reset --hard
```

2. 拉取第三方库
```
git submodule update --recursive --init
```

3. 编译

```
bash script/build_and_pack_run.sh --build_mode RELEASE --build_python ON --xpu_type NPU

```

- build_and_pack_run.sh支持3个参数,按顺序分别是<build_mode> <build_python> <xpu_type>
- build_mode: 编译类型, 可填RELEASE或DEBUG, 默认RELEASE
- build_python: 是否编译python的whl包,可填ON或OFF, 默认ON
- xpu_type: 指定异构设备，设置NPU为CANN版本，GPU为CUDA版本。
- 不填入参数情况下,默认执行build_and_pack_run.sh RELEASE ON NPU
4. ut运行

支持直接执行如下脚本编译并运行ut
```
bash script/run_ut.sh
```

## 安装使用

memfabric_hybrid将所有特性集成到run包中供用户使用，run包格式为 ```memfabric-hybrid-${version}_${os}_${arch}.run```

其中，versin表示memfabric_hybrid的版本；os表示操作系统,如linux；arch表示架构，如x86或aarch64

### run包安装

run包的默认安装根路径为 /usr/local/

安装完成后需要source安装路径下的memfabric_hybrid/set_env.sh

参考安装命令如下
```bash
bash memfabric_hybrid-1.0.0_linux_aarch64.run
source /usr/local/memfabric_hybrid/set_env.sh
```
> 📌 **注意**： A2环境使用DRAM池化需要根据每台机器池化内存的大小来配置大页内存，否则初始化失败
> 
> 检查是否配置大页:
> 
> ```grep Huge | /proc/meminfo```
> 
> 配置大页内存，以配置1024个大页为例
> 
> ```echo 1024 > /proc/sys/vm/nr_hugepages```

如果想要自定义安装路径，可以添加--install-path参数
```bash
bash memfabric_hybrid-1.0.0_linux_aarch64.run --install-path=${your path}
```

默认安装完成后目录结构如下
```
/usr/local/memfabric_hybrid
├── 1.0.0
│   ├── aarch64-linux
│   │   ├── bin
│   │   ├── include
│   │   │   ├── hybm
│   │   │   │   ├── hybm.h
│   │   │   │   ├── hybm_big_mem.h
│   │   │   │   ├── hybm_data_op.h
│   │   │   │   └── hybm_def.h
│   │   │   └── smem
│   │   │       ├── device
│   │   │       │   ├── smem_shm_aicore_base_api.h
│   │   │       │   ├── smem_shm_aicore_base_copy.h
│   │   │       │   ├── smem_shm_aicore_base_define.h
│   │   │       │   ├── smem_shm_aicore_base_meta.h
│   │   │       │   └── smem_shm_aicore_base_rdma.h
│   │   │       └── host
│   │   │           ├── smem.h
│   │   │           ├── smem_bm.h
│   │   │           ├── smem_bm_def.h
│   │   │           ├── smem_shm.h
│   │   │           ├── smem_shm_def.h
│   │   │           ├── smem_trans.h
│   │   │           └── smem_trans_def.h
│   │   ├── lib64
│   │   │   ├── libmf_hybm_core.so
│   │   │   └── libmf_smem.so
│   │   ├── script
│   │   │   ├── certs
│   │   │   │   ├── generate_client_cert.py
│   │   │   │   ├── generate_crl.py
│   │   │   │   ├── generate_root_cert.py
│   │   │   │   └── generate_server_cert.py
│   │   │   └── mock_server
│   │   └── wheel
│   │       ├── mf_adapter-1.0.0-cp311-cp311-linux_aarch64.whl
│   │       └── memfabric_hybrid-1.0.0-cp311-cp311-linux_aarch64.whl
│   ├── uninstall.sh
│   └── version.info
├── latest -> 1.0.0
└── set_env.sh

```

安装的python包如下

```text

root@localhost:/# pip show memfabric_hybrid
Name: memfabric_hybrid
Version: 1.0.0
Summary: python api for smem
Home-page: https://gitcode.com/Ascend/memfabric_hybrid
Author:
Author-email:
License: Apache License Version 2.0
Location: /usr/local/lib/python3.11/site-packages
Requires:
Required-by:
root@localhost:/# pip show mf_adapter
Name: mf_adapter
Version: 1.0.0
Summary: python api for mf_adapter
Home-page: https://gitcode.com/Ascend/memfabric_hybrid
Author:
Author-email:
License: Apache License Version 2.0
Location: /usr/local/lib/python3.11/site-packages
Requires:
Required-by:

root@localhost:/# tree /usr/local/lib/python3.11/site-packages/memfabric_hybrid
/usr/local/lib/python3.11/site-packages/memfabric_hybrid
├── VERSION
├── __init__.py
├── __pycache__
│   └── __init__.cpython-311.pyc
├── _pymf_hybrid.cpython-311-aarch64-linux-gnu.so
└── lib
    ├── libmf_hybm_core.so
    └── libmf_smem.so

root@localhost:/# tree /usr/local/lib/python3.11/site-packages/mf_adapter
/usr/local/lib/python3.11/site-packages/mf_adapter
├── VERSION
├── __init__.py
├── __pycache__
│   └── __init__.cpython-311.pyc
├── _pymf_transfer.cpython-311-aarch64-linux-gnu.so
└── lib
    ├── libmf_hybm_core.so
    └── libmf_smem.so 
```
在安装过程中，会默认尝试安装适配当前环境的memfabric-hybrid的whl包，如果未安装，则在使用python接口前需要用户手动安装(安装包路径参考上面目录结构)
```bash
# 检查是否安装memfabric_hybric
pip show mmefabric_hybrid
# 手动安装
pip install memfabric_hybrid-1.0.0-cp311-cp311-linux_aarch64.whl
```
