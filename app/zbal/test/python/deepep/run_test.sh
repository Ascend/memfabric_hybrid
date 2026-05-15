#!/bin/bash

rm -rf ./logs
rm -rf ./export_only_prof_dir/*

export ASCEND_PROCESS_LOG_PATH=./logs
export ASCEND_GLOBAL_LOG_LEVEL=3
export DEEP_NORMAL_MODE_USE_INT8_QUANT=1
export TASK_QUEUE_ENABLE=2

python test_low_latency.py
