## 目录结构介绍
```
├── AddKernelInvocationNeo
│   ├── cmake                   // 编译工程文件
│   ├── shm_all_shift.cpp       // 算子kernel实现
│   ├── CMakeLists.txt          // 编译工程文件
│   ├── data_utils.h            // 数据读入写出函数
│   ├── main.cpp                // 主函数，调用算子的应用程序，含CPU域及NPU域调用
│   └── run.sh                  // 编译运行算子的脚本
```
## 代码实现介绍
本样例简单验证了smem_put_xxx接口和smem_uput_xxx接口
运行样例前请先编译smem和hybm,生成lib库，并将编译结果放在```memfabric_hybrid/output```中

## 运行样例算子
  - 打开样例目录   
    以命令行方式编译样例

    ```bash
    bash build.sh -r [RUN_MODE] -v  [SOC_VERSION]
    ```
    - RUN_MODE：编译方式，可选择CPU调试，NPU仿真，NPU上板。支持参数为[cpu /sim / npu]
    - SOC_VERSION：昇腾AI处理器型号，如果无法确定具体的[SOC_VERSION]，则在安装昇腾AI处理器的服务器执行npu-smi info命令进行查询，在查询到的“Name”前增加Ascend信息，例如“Name”对应取值为xxxyy，实际配置的[SOC_VERSION]值为Ascendxxxyy。支持以下产品型号：
      - Atlas 训练系列产品
      - Atlas 推理系列产品AI Core
      - Atlas A2训练系列产品/Atlas 800I A2推理产品
      - Atlas 200/500 A2推理产品

    示例如下
    ```bash
    bash build.sh -r npu -v Ascend910B3
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