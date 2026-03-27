## 目录结构介绍

```
├── ShiftPutGet
│   ├── shm_all_shift.cpp       // 算子kernel实现
│   ├── CMakeLists.txt          // 编译工程文件
│   ├── data_utils.h            // 应用程序日志打印相关头文件
│   ├── main.cpp                // 主函数，调用算子的应用程序
│   ├── build.sh                // 编译算子的脚本
│   └── run.sh                  // 运行算子的脚本
```

## 代码实现介绍

本样例简单验证了extra_context相关接口,smem_shm_put_xxx接口和smem_shm_uput_xxx接口

本样例需要在npu环境下编译运行

运行样例前请先编译安装**memfabric_hybrid的run包**,并source安装路径下的set_env.sh

另外，请在环境上提前安装NPU固件驱动和CANN包([环境安装参考链接](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0000.html))

安装完成后记得配置CANN环境变量
([参考安装Toolkit开发套件包的第三步配置环境变量](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0008.html?Mode=PmIns&OS=Ubuntu&Software=cannToolKit))

## 运行样例算子

- 打开样例目录   
  以命令行方式编译样例

  ```bash
  bash build.sh -v [ENV_VERSION]
  ```
    - ENV_VERSION:环境类型,支持填入A2/A3/A5,默认为A3

  示例如下
  ```bash
  bash build.sh -v A3
  ```

- 使用以下命令运行样例
  ```bash
  bash run.sh [RANK_SIZE] [SERVER_IP]
  ```
    - RANK_SIZE: 期望使用多少张卡，每张卡一个进程
    - SERVER_IP: ```tcp://<ip>:<port>``` configStore的server的监听ip和端口。关于 configStore 配置存储系统的说明，请参考  [config_store_cluster_ha](../../../doc/config_store_cluster_ha.md)。

  示例如下
  ```bash
  bash run.sh 8 tcp://127.0.0.1:8570
  ```
- 如需要跨机在A3超节点内运行，可以参考run.sh内执行shm_kernels命令在多个节点内运行