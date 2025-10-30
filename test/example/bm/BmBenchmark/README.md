## 代码实现介绍
本样例基于smem_bm层接口，测试如下路径的带宽/时延性能：
```
1、H2D
2、D2H
3、H2RD
4、H2RH
5、D2RH
6、D2RD
7、RH2D
8、RH2H
9、RD2D
10、RD2H
```

本样例需要在npu环境下编译运行

首先,请在环境上提前安装NPU固件驱动和CANN包([环境安装参考链接](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0000.html))

当前HDK固件驱动需要使用特定版本([社区版HDK下载链接](https://www.hiascend.com/hardware/firmware-drivers/community))

安装完成后需要配置CANN环境变量
([参考安装Toolkit开发套件包的第三步配置环境变量](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0008.html))

运行样例前请先编译安装**memfabric_hybrid的run包**,默认安装路径为/usr/local/,然后source安装路径下的set_env.sh

memfabric_hybrid参考安装命令
```bash
bash mxc-memfabric_hybrid-1.0.0_linux_aarch64.run
source /usr/local/mxc/memfabric_hybrid/set_env.sh
```

## 编译
在当前目录执行如下命令即可
  ```bash
  mkdir build
  cmake . -B build
  make -C build
  ```
或执行编译脚本：
```
bash build_bm_perf_benchmark.sh
```

## 运行
编译完成后,会在当前目录生成bm_example可执行文件
执行方式如下,支持多节点运行
  ```bash
  ./bm_perf_benchmark [WORLD_SIZE] [LOCAL_RANK_SIZE] [RANK_START] [SERVER_IP] [BLOCK_SIZE] [LOOP_COUNT] [BATCH_SIZE]
  ```
    - WORLD_SIZE: 整个集群使用的卡数
    - LOCAL_RANK_SIZE: 在本节点使用的卡数
    - RANK_START: 本节点的rankId的起始值,本节点的rankId范围就是[RANK_START, RANK_START + LOCAL_RANK_SIZE)
    - SERVER_IP: ```tcp://<ip>:<port>``` configStore的server的监听ip和端口
    - BLOCK_SIZE: 单个拷贝数据块的大小
    - BATCH_SIZE：每次下发拷贝的block个数
    - LOOP_COUNT：循环下发多少次拷贝

  示例如下(假设期望指定监听8570端口)
  ```bash
  单节点运行2张卡: 
  ./bm_perf_benchmark 2 2 0 tcp://127.0.0.1:8570 4194304 10000 1
  
  两节点运行16张卡,每节点8张(假设nodeA的ip为x.x.x.x):
  nodeA: ./bm_perf_benchmark 16 8 0 tcp://x.x.x.x:8570 4194304 10000 1
  nodeB: ./bm_perf_benchmark 16 8 8 tcp://x.x.x.x:8570 4194304 10000 1
  ```
