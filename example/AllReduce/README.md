## 目录结构介绍
```
├── AllReduce
│   ├── scripts
│   │   └── gen_data.py         // 输入数据和真值数据生成脚本
│   ├── shm_all_reduce.cpp      // 算子kernel实现
│   ├── CMakeLists.txt          // 编译工程文件
│   ├── data_utils.h            // 应用程序日志打印相关头文件
│   ├── main.cpp                // 主函数，调用算子的应用程序
│   ├── build.sh                // 编译算子的脚本
│   └── run.sh                  // 运行算子的脚本
```
## 代码实现介绍
本样例中实现的是固定shape为16*2048的AllReduce算子。

本样例需要在npu环境下编译运行

运行样例前请先编译安装**memfabric_hybrid的run包**,并source安装路径下的set_env.sh

另外，请在环境上提前安装NPU固件驱动和CANN包([环境安装参考链接](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0000.html))

安装完成后记得配置CANN环境变量
([参考安装Toolkit开发套件包的第三步配置环境变量](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0008.html))

## 运行样例算子
- 打开样例目录   
  以命令行方式编译样例

  ```bash
  bash build.sh -v [SOC_VERSION]
  ```
    - SOC_VERSION：昇腾AI处理器型号，如果无法确定具体的[SOC_VERSION]，则在安装昇腾AI处理器的服务器执行npu-smi info命令进行查询，在查询到的“Name”前增加Ascend信息，例如“Name”对应取值为xxxyy，实际配置的[SOC_VERSION]值为Ascendxxxyy。支持以下产品型号：
        - Atlas 训练系列产品
        - Atlas 推理系列产品AI Core
        - Atlas A2训练系列产品/Atlas 800I A2推理产品
        - Atlas 200/500 A2推理产品

  示例如下
  ```bash
  bash build.sh -v Ascend910B3
  ```

- 使用以下命令运行样例
  ```bash
  bash run.sh [RANK_SIZE] [SERVER_IP]
  ```
    - RANK_SIZE: 期望使用多少张卡，每张卡一个进程
    - SERVER_IP: ```tcp://<ip>:<port>``` rank0的监听ip

  示例如下
  ```bash
  bash run.sh 8 tcp://127.0.0.1:8570
  ```
- 如需要跨机在A3超节点内运行，可以参考run.sh内执行shm_kernels命令在多个节点内运行