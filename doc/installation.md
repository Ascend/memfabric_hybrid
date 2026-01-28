# 软件安装

## 编译软件包

### 编译工具建议版本

- OS: Ubuntu 22.04 LTS+
- cmake: 3.20.x
- gcc: 11.4+
- python 3.11.10
- pybind11 2.10.3
- make 4.3 or ninja 1.10.1

### 编译

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

执行如下命令进行编译，编译成功后，会生成run包在output目录下
```
bash script/build_and_pack_run.sh --build_mode RELEASE --build_python ON --xpu_type NPU --build_test OFF --build_hcom OFF

```

- build_and_pack_run.sh支持5个参数，分别是<build_mode> <build_python> <xpu_type> <build_test> <build_hcom>
- build_mode: 编译类型，可填RELEASE或DEBUG，默认RELEASE
- build_python: 是否编译python的whl包，可填ON或OFF，默认ON
- xpu_type: 指定异构设备，设置NPU为CANN版本，GPU为CUDA版本，NONE为无卡环境, 默认NPU
- build_test: 是否编译打包测试工具和样例代码等，可填ON或OFF，默认OFF
- build_hcom: 是否编译hcom，可填ON或OFF，默认OFF（如果数据传输类型需要使用HOST_RDMA、HOST_TCP、HOST_URMA时，需要设置为ON，且环境需要安装libibverbs-dev，可通过apt install libibverbs-dev进行安装）

## 环境准备

编译时xpu_type选择NPU时，编译出来的包在运行时，运行环境上需要安装NPU固件驱动和CANN包。

依赖的NPU固件驱动和CANN包，具体版本详见下面的软件硬件配套说明

请在环境上提前安装NPU固件驱动和CANN包([环境安装参考链接](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0000.html))

安装完成后需要配置CANN环境变量([参考安装Toolkit开发套件包的第三步配置环境变量](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0008.html?Mode=PmIns&OS=Ubuntu&Software=cannToolKit))

## 安装软件包

memfabric_hybrid将所有特性集成到run包中供用户使用，run包格式为 ```memfabric-hybrid-${version}_${os}_${arch}.run```

其中，version表示memfabric_hybrid的版本；os表示操作系统，如linux；arch表示架构，如x86或aarch64

### run包安装

run包的默认安装根路径为 /usr/local/

安装完成后需要source安装路径下的memfabric_hybrid/set_env.sh

参考安装命令如下（此处以1.0.0版本为例）
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

安装的run包可以通过如下命令查看版本（此处以默认安装路径为例）

```
root@localhost:/# cat /usr/local/memfabric_hybrid/latest/version.info
Version:1.0.0
Platform:aarch64
Kernel:linux
CommitId:034c71e58f1d70fe691644b2b18e0b8418c40b7a
```

安装的python包可以通过如下命令查看版本

```text
root@localhost:/# pip show memfabric_hybrid
Name: memfabric_hybrid
Version: 1.0.0
Summary: python api for memfabric hybrid
Home-page: https://gitcode.com/Ascend/memfabric_hybrid
Author:
Author-email:
License: Mulan PSL v2
Location: /usr/local/lib/python3.11/site-packages
Requires:
Required-by:
```

### whl包安装
在安装过程中，会默认尝试安装适配当前环境的memfabric-hybrid的whl包，如果未安装，则在使用python接口前需要用户手动安装

```bash
# 检查是否安装memfabric_hybrid
pip show memfabric_hybrid
```

1. 用已安装的run包目录下的whl包进行安装（此处以默认安装路径为例）
```bash
# 手动安装
pip install /usr/local/memfabric_hybrid/latest/aarch64-linux/wheel/memfabric_hybrid-1.0.0-cp311-cp311-linux_aarch64.whl
```
2. whl包已发布到[pypi](https://pypi.org/project/memfabric-hybrid/#files)，可以直接进行在线安装
```bash
# 手动安装（这里以1.0.0版本为例）
pip install memfabric_hybrid==1.0.0
```

whl包安装完成后，需要设置LD_LIBRARY_PATH环境变量
```bash
# 此处以python3.11为例
export LD_LIBRARY_PATH=/usr/local/lib/python3.11/site-packages/memfabric_hybrid/lib/:$LD_LIBRARY_PATH
```bash

## 卸载软件包
### 卸载run包
执行run包安装路径（此处以默认安装路径为例）下的卸载脚本进行卸载。
```bash
bash /usr/local/memfabric_hybrid/latest/uninstall.sh
```
### 卸载whl包
```bash
pip uninstall memfabric_hybrid
```