#!/bin/bash
RANK_SIZE=$1
IP_PORT=$2
echo " ====== example run, ranksize: ${RANK_SIZE} ip: ${IP_PORT} ====="

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:$LD_LIBRARY_PATH

for (( index = 0; index < ${RANK_SIZE}; index = index + 1 )); do
    ./out/bin/shm_example ${RANK_SIZE} ${index} ${IP_PORT} &
done