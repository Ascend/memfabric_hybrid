#!/bin/bash
GPUS_PER_NODE=8
export HCCL_CONNECT_TIMEOUT=120
MASTER_ADDR=localhost
MASTER_PORT=6095
NNODES=1
NODE_RANK=0

DISTRIBUTED_ARGS="--nproc_per_node $GPUS_PER_NODE --nnodes $NNODES --node_rank $NODE_RANK --master_addr $MASTER_ADDR --master_port $MASTER_PORT"
torchrun  $DISTRIBUTED_ARGS -m pytest -s tests/st/test_reshard.py