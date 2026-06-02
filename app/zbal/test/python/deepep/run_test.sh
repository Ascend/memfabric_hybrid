#!/bin/bash

export TASK_QUEUE_ENABLE=2
export ASCEND_PROCESS_LOG_PATH=./logs
export ASCEND_GLOBAL_LOG_LEVEL=2
rm -rf ./logs

# for normal mode
export DEEP_NORMAL_MODE_USE_INT8_QUANT=1
python test_normal.py --num-processes 8

# for low lantency mode
#rm -rf ./export_only_prof_dir/*
#python test_low_latency.py --num-processes 8

cat ./logs/rank00_*.log
