#!/bin/bash
RANK_SIZE=$1
IP_PORT=$2
echo " ====== example run, ranksize: ${RANK_SIZE} ip: ${IP_PORT} ====="

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:$LD_LIBRARY_PATH
export MEMFABRIC_HYBRID_TLS_ENABLE=0

for (( idx = 0; idx < ${RANK_SIZE}; idx = idx + 1 )); do
    ./out/bin/shm_example ${RANK_SIZE} ${idx} ${IP_PORT} &
done