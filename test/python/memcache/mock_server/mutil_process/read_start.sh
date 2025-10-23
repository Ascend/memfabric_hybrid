#!/bin/bash
source /usr/local/Ascend/ascend-toolkit/set_env.sh
source /usr/local/mxc/memfabric_hybrid/set_env.sh
export MMC_LOCAL_CONFIG_PATH=$PWD/mmc-local-read.conf
export ASCEND_RT_VISIBLE_DEVICES=0,1,2,3,4,5,6,7
export ASCEND_TRANSPORT_PRINT=0  # mooncake变量，日志打印
export ASCEND_AGGREGATE_ENABLE=1 # mooncake变量，内存聚合
# nohup python3 read_mutil_process.py &> read.log 2>&1 &
python3 read_mutil_process.py
