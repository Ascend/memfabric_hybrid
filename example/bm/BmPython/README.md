## 代码实现介绍
本样例简单验证了`big memory`相关`python`接口

本样例需要在npu环境下编译运行

首先,请在环境上提前安装NPU固件驱动和CANN包([环境安装参考链接](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0000.html))

HDK固件驱动需要使用**25.0.RC1**及以上版本([社区版HDK下载链接](https://www.hiascend.com/hardware/firmware-drivers/community))

安装完成后需要配置CANN环境变量
([参考安装Toolkit开发套件包的第三步配置环境变量](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1alpha002/softwareinst/instg/instg_0008.html))

运行样例前请先编译安装**memfabric_hybrid的run包**,默认安装路径为/usr/local/,然后source安装路径下的set_env.sh

memfabric_hybrid参考安装命令
```bash
bash memfabric_hybrid-1.0.0_linux_aarch64.run # 以实际安装包路径和名称为准
source /usr/local/memfabric_hybrid/set_env.sh
```

## 运行

执行方式如下,支持多节点运行

```bash
python3 smem_bm_example.py --world_size <WORLD_SIZE> --local_ranks <LOCAL_RANK_SIZE> --rank_start <RANK_START> --url <STORE_URL> --auto_ranking <AUTO_RANKING>
```
    - WORLD_SIZE: 整个集群使用的卡数
    - LOCAL_RANK_SIZE: 在本节点使用的卡数
    - RANK_START: 本节点的rankId的起始值,本节点的rankId范围就是[RANK_START, RANK_START + LOCAL_RANK_SIZE)
    - STORE_URL: `tcp://<ip>:<port>` configStore的server的监听ip和端口
    - AUTO_RANKING: 可选参数,不填则默认不开启auto_rank; true表示开启, false表示不开启(开启autorank,则bm内部会自动生成全局rankId,不需要用户指定)

示例如下(假设期望指定监听8570端口)

(1) 单节点运行8张卡: 

```bash
python3 smem_bm_example.py --world_size 8 --local_ranks 8 --rank_start 0 --url tcp://127.0.0.1:8570
```

(2) 单节点运行8张卡,并启用autorank: 

```bash
python3 smem_bm_example.py --world_size 8 --local_ranks 8 --rank_start 0 --url tcp://127.0.0.1:8570 --auto_ranking true
```

(3) 两节点运行16张卡,每节点8张(假设nodeA的ip为x.x.x.x):  

nodeA: 
```bash
python3 smem_bm_example.py --world_size 16 --local_ranks 8 --rank_start 0 --url tcp://x.x.x.x:8570
```

nodeB:
```bash
python3 smem_bm_example.py --world_size 16 --local_ranks 8 --rank_start 8 --url tcp://x.x.x.x:8570
```