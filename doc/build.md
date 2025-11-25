# 构建

## 编译

memfabric_hybrid编译不依赖CANN和HDK.

1. 下载代码
```
git clone git@gitee.com:ascend/memfabric_hybrid.git

cd memfabric_hybrid
```

2. 拉取第三方库
```
git submodule init

git submodule update --recursive
```

3. 编译

支持直接执行如下脚本编译
```
bash script/build.sh

build.sh支持5个参数,按顺序分别是<build_mode> <need_build_ut> <open_abi> <build_whl> <BUILD_COMPILER>
build_mode:编译类型,可填RELEASE或DEBUG
need_build_ut:是否编译uttest,可填ON或OFF
open_abi:编译时是否添加-D_GLIBCXX_USE_CXX11_ABI=1宏,可填ON或OFF
build_whl:是否编译python的whl包,可填ON或OFF
build_compiler:编译器选择，输入bisheng可手动指定编译器为bisheng。
不填入参数情况下,默认执行build.sh RELEASE OFF ON ON gcc
```

4. ut运行

支持直接执行如下脚本编译并运行ut
```
bash script/run_ut.sh
```

## 安装使用

memfabric_hybrid将所有特性集成到run包中供用户使用，run包格式为 ```mxc-memfabric-hybrid-${version}_${os}_${arch}.run```

其中，versin表示memfabric_hybrid的版本；os表示操作系统,如linux；arch表示架构，如x86或aarch64

### run包编译

可以直接执行如下命令进行编译，在output目录下生成run包
```
bash script/build_and_pack_run.sh
```

### run包依赖

run包只能安装到npu环境上，且依赖于NPU固件驱动和CANN包，具体版本依赖详见下面的软件硬件配套说明

请在环境上提前安装NPU固件驱动和CANN包([环境安装参考链接](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0000.html))

安装完成后需要配置CANN环境变量([参考安装Toolkit开发套件包的第三步配置环境变量](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0008.html))

### run包安装

run包的默认安装根路径为 /usr/local/

安装完成后需要source安装路径下的mxc/memfabric_hybrid/set_env.sh

参考安装命令如下
```bash
bash mxc-memfabric_hybrid-1.0.0_linux_aarch64.run
source /usr/local/mxc/memfabric_hybrid/set_env.sh
```

如果想要自定义安装路径，可以添加--install-path参数
```bash
bash mxc-memfabric_hybrid-1.0.0_linux_aarch64.run --install-path=${your path}
```

安装完成后目录结构如下
```
${INSTALL_PATH}/
    |--mxc
          |--memfabric_hybrid
              |-- latest
              |-- set_env.sh
              |-- ${version}
                   |-- ${arch}-${os}
                        |-- include    (头文件)
                        |-- lib64      (so库)
                        |-- whl        (python的whl包)
                   |-- uninstall.sh
                   |-- version.info

default ${INSTALL_PATH} is /usr/local/
```

在安装过程中，会默认尝试安装适配当前环境的memfabric-hybrid的whl包，如果未安装，则在使用python接口前需要用户手动安装(安装包路径参考上面目录结构)

memfabric-hybrid 默认开启tls通信加密。如果想关闭，需要主动调用`smem_set_conf_store_tls`接口关闭：
```c
int32_t ret = smem_set_conf_store_tls(false, nullptr, 0);
```
具体细节详见安全声明章节
